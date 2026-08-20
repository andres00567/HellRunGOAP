#include "GOAPBrainComponent.h"

#include "GOAPActionTask.h"
#include "GOAPPlanner.h"
#include "GOAPPlanningSubsystem.h"
#include "GOAPSensor.h"
#include "GOAPWorldStateSubsystem.h"
#include "Algo/AllOf.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"

namespace
{
    TAutoConsoleVariable<int32> CVarHellRunGOAPDebug(
        TEXT("hellrun.ai.GOAPDebug"), 0,
        TEXT("GOAP diagnostics: 0=off, 1=goal/action, 2=goal/action and typed facts."),
        ECVF_Cheat);
    TAutoConsoleVariable<int32> CVarHellRunGOAPTrace(
        TEXT("hellrun.ai.GOAPTrace"), 0,
        TEXT("Write GOAP solve records to the log when nonzero."), ECVF_Cheat);
}

UGOAPBrainComponent::UGOAPBrainComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bStartWithTickEnabled=true;
}

void UGOAPBrainComponent::BeginPlay()
{
    Super::BeginPlay();
    if(UGOAPWorldStateSubsystem* State=GetWorld()?GetWorld()->GetSubsystem<UGOAPWorldStateSubsystem>():nullptr)
        State->RegisterBrain(this);
    if(bStartLogicAutomatically) StartLogic();
}

void UGOAPBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopLogic(TEXT("Owner ended play"));
    if(UGOAPWorldStateSubsystem* State=GetWorld()?GetWorld()->GetSubsystem<UGOAPWorldStateSubsystem>():nullptr)
        State->UnregisterBrain(this);
    Super::EndPlay(EndPlayReason);
}

bool UGOAPBrainComponent::StartLogic()
{
    StopLogic(TEXT("Restarting"));
    if(!Domain) return false;
    TArray<FText> Errors;
    if(!Domain->Compile(CompiledDomain,&Errors))
    {
        for(const FText& Error:Errors) UE_LOG(LogTemp,Error,TEXT("GOAP %s: %s"),*GetNameSafe(GetOwner()),*Error.ToString());
        return false;
    }
    AgentFacts.Reset(); Sensors.Reset();
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
    const float AgentPhase=static_cast<float>(GetTypeHash(GetOwner()->GetFName())%1024u)/1024.0f;
    for(const UGOAPSensor* Template:Domain->Sensors)
        if(Template)
        {
            UGOAPSensor* Sensor=DuplicateObject<UGOAPSensor>(Template,this);
            Sensor->NextUpdateTime=Now+AgentPhase*Sensor->PhaseSpread*Sensor->UpdateInterval;
            Sensors.Add(Sensor);
        }
    bRunning=true; bReplanRequested=true; LastReplanReason=TEXT("Logic started");
    SetComponentTickEnabled(true); return true;
}

void UGOAPBrainComponent::StopLogic(const FString& Reason)
{
    if(ActiveTask&&ActiveTask->GetStatus()==EGOAPTaskStatus::Running)
        ActiveTask->Abort(Reason);
    ActiveTask=nullptr; ActiveActionId.Invalidate(); ActiveGoalId.Invalidate();
    CurrentPlan={}; RemainingActions.Reset(); bRunning=false;
    bPlanQueued=false;
}

void UGOAPBrainComponent::RequestReplan(const FString& Reason)
{ bReplanRequested=true; LastReplanReason=Reason; }

const FGOAPCompiledFact* UGOAPBrainComponent::FindFact(const FName Name,
    int32* OutIndex) const
{
    for(int32 Index=0;Index<CompiledDomain.Facts.Num();++Index)
        if(CompiledDomain.Facts[Index].Name==Name)
        {if(OutIndex)*OutIndex=Index;return &CompiledDomain.Facts[Index];}
    return nullptr;
}

bool UGOAPBrainComponent::SetFact(const FName FactName,const FGOAPValue& Value,
    const FName Source,const float Confidence,const float Lifetime)
{
    const FGOAPCompiledFact* Fact=FindFact(FactName);
    return Fact&&SetFactById(Fact->Id,Value,Source,Confidence,Lifetime);
}

bool UGOAPBrainComponent::SetFactById(const FGuid& FactId,const FGOAPValue& Value,
    const FName Source,const float Confidence,const float Lifetime)
{
    const int32* Index=CompiledDomain.FactIndices.Find(FactId);
    if(!Index||CompiledDomain.Facts[*Index].Type!=Value.Type) return false;
    const FGOAPCompiledFact& Fact=CompiledDomain.Facts[*Index];
    if(Fact.Scope!=EGOAPFactScope::Agent)
    {
        if(UGOAPWorldStateSubsystem* State=GetWorld()?GetWorld()->GetSubsystem<UGOAPWorldStateSubsystem>():nullptr)
            return State->SetSharedFact(Fact.Scope,SquadKey,FactId,Value,Source,Confidence,Lifetime);
        return false;
    }
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
    FGOAPWorldFactRecord& Record=AgentFacts.FindOrAdd(FactId);
    // Source is diagnostic provenance, not part of the symbolic value. An
    // action effect and a sensor confirming the same fact must not bounce the
    // brain between identical plans merely because their labels differ.
    const bool bChanged=ValuesDiffer(Fact,Record.Value,Value);
    Record.Value=Value; Record.Source=Source; Record.Confidence=FMath::Clamp(Confidence,0.0f,1.0f);
    Record.UpdatedAt=Now; Record.ExpiresAt=Lifetime>0.0f?Now+Lifetime:-1.0;
    if(bChanged)
    {
        ++LocalRevision;
        if(Fact.bTriggersReplan)
            RequestReplan(FString::Printf(TEXT("Fact changed: %s"),*Fact.Name.ToString()));
    }
    return true;
}

bool UGOAPBrainComponent::ValuesDiffer(const FGOAPCompiledFact& Fact,
    const FGOAPValue& A,const FGOAPValue& B) const
{
    if(A.Type!=B.Type) return true;
    if(A.Type==EGOAPValueType::Float)
        return !FMath::IsNearlyEqual(A.FloatValue,B.FloatValue,Fact.ChangeTolerance);
    if(A.Type==EGOAPValueType::Vector)
        return !A.VectorValue.Equals(B.VectorValue,Fact.ChangeTolerance);
    return !A.Equals(B);
}

void UGOAPBrainComponent::ExpireAgentFacts(const double Now)
{
    TArray<FGuid,TInlineAllocator<8>> Expired;
    for(const TPair<FGuid,FGOAPWorldFactRecord>& Pair:AgentFacts)
        if(Pair.Value.IsExpired(Now)) Expired.Add(Pair.Key);
    for(const FGuid& Id:Expired)
    {
        AgentFacts.Remove(Id); ++LocalRevision;
        if(const int32* Index=CompiledDomain.FactIndices.Find(Id))
            if(CompiledDomain.Facts[*Index].bTriggersReplan)
                RequestReplan(FString::Printf(TEXT("Fact expired: %s"),
                    *CompiledDomain.Facts[*Index].Name.ToString()));
    }
}

void UGOAPBrainComponent::NotifySharedFactChanged(const EGOAPFactScope Scope,
    const FName ChangedSquad,const FGuid& FactId,const FString& Reason)
{
    if(!bRunning||Scope==EGOAPFactScope::Agent) return;
    if(Scope==EGOAPFactScope::Squad&&ChangedSquad!=SquadKey) return;
    const int32* Index=CompiledDomain.FactIndices.Find(FactId);
    if(!Index) return;
    const FGOAPCompiledFact& Fact=CompiledDomain.Facts[*Index];
    if(Fact.Scope!=Scope||!Fact.bTriggersReplan) return;
    ++LocalRevision;
    RequestReplan(Reason.IsEmpty()
        ? FString::Printf(TEXT("Shared fact changed: %s"),*Fact.Name.ToString())
        : Reason);
}

bool UGOAPBrainComponent::ClearFact(const FName FactName)
{
    const FGOAPCompiledFact* Fact=FindFact(FactName); if(!Fact)return false;
    bool bRemoved=false;
    if(Fact->Scope==EGOAPFactScope::Agent)bRemoved=AgentFacts.Remove(Fact->Id)>0;
    else if(UGOAPWorldStateSubsystem* State=GetWorld()?GetWorld()->GetSubsystem<UGOAPWorldStateSubsystem>():nullptr)
        bRemoved=State->ClearSharedFact(Fact->Scope,SquadKey,Fact->Id);
    if(bRemoved){++LocalRevision;RequestReplan(FString::Printf(TEXT("Fact cleared: %s"),*FactName.ToString()));}
    return bRemoved;
}

bool UGOAPBrainComponent::GetFact(const FName FactName,FGOAPValue& OutValue) const
{
    const FGOAPCompiledFact* Fact=FindFact(FactName); if(!Fact)return false;
    FGOAPWorldFactRecord Record;
    if(Fact->Scope==EGOAPFactScope::Agent)
    {
        const FGOAPWorldFactRecord* Found=AgentFacts.Find(Fact->Id);
        const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
        if(Found&&!Found->IsExpired(Now))Record=*Found;
        else{OutValue=Fact->DefaultValue;return true;}
    }
    else
    {
        const UGOAPWorldStateSubsystem* State=GetWorld()?GetWorld()->GetSubsystem<UGOAPWorldStateSubsystem>():nullptr;
        if(!State||!State->GetSharedFact(Fact->Scope,SquadKey,Fact->Id,Record))
        {OutValue=Fact->DefaultValue;return true;}
    }
    OutValue=Record.Value; return true;
}

bool UGOAPBrainComponent::GetBoolFact(const FName Name,bool& Out) const
{FGOAPValue V;if(!GetFact(Name,V)||V.Type!=EGOAPValueType::Bool)return false;Out=V.BoolValue;return true;}
bool UGOAPBrainComponent::GetFloatFact(const FName Name,float& Out) const
{FGOAPValue V;if(!GetFact(Name,V)||V.Type!=EGOAPValueType::Float)return false;Out=V.FloatValue;return true;}
bool UGOAPBrainComponent::GetVectorFact(const FName Name,FVector& Out) const
{FGOAPValue V;if(!GetFact(Name,V)||V.Type!=EGOAPValueType::Vector)return false;Out=V.VectorValue;return true;}
bool UGOAPBrainComponent::GetObjectFact(const FName Name,UObject*& Out) const
{FGOAPValue V;if(!GetFact(Name,V)||V.Type!=EGOAPValueType::Object)return false;Out=V.ObjectValue;return true;}
bool UGOAPBrainComponent::SetBoolFact(FName N,bool V,FName S,float C,float L)
{return SetFact(N,FGOAPValue::MakeBool(V),S,C,L);}
bool UGOAPBrainComponent::SetFloatFact(FName N,float V,FName S,float C,float L)
{return SetFact(N,FGOAPValue::MakeFloat(V),S,C,L);}
bool UGOAPBrainComponent::SetVectorFact(FName N,FVector V,FName S,float C,float L)
{return SetFact(N,FGOAPValue::MakeVector(V),S,C,L);}
bool UGOAPBrainComponent::SetObjectFact(FName N,UObject* V,FName S,float C,float L)
{return SetFact(N,FGOAPValue::MakeObject(V),S,C,L);}

bool UGOAPBrainComponent::BuildPlanningState(FGOAPPlanningState& OutState) const
{
    if(!CompiledDomain.IsValid())return false;
    OutState.Values.SetNum(CompiledDomain.Facts.Num());
    for(int32 Index=0;Index<CompiledDomain.Facts.Num();++Index)
    {
        FGOAPValue Value;
        if(!GetFact(CompiledDomain.Facts[Index].Name,Value))Value=CompiledDomain.Facts[Index].DefaultValue;
        OutState.Values[Index]=Value;
    }
    return true;
}

bool UGOAPBrainComponent::ConditionsHold(const TArray<FGOAPCondition>& Conditions) const
{
    FGOAPPlanningState State; return BuildPlanningState(State)
        && Algo::AllOf(Conditions,[&](const FGOAPCondition& C){return GOAPEvaluateCondition(C,CompiledDomain,State);});
}

const FGOAPCompiledAction* UGOAPBrainComponent::FindAction(const FGuid& Id) const
{
    const int32* Index=CompiledDomain.ActionIndices.Find(Id);
    return Index?&CompiledDomain.Actions[*Index]:nullptr;
}

void UGOAPBrainComponent::TickSensors(const double Now)
{
    for(UGOAPSensor* Sensor:Sensors)
        if(Sensor&&Now>=Sensor->NextUpdateTime)
        {Sensor->Sample(this);Sensor->NextUpdateTime=Now+FMath::Max(0.0f,Sensor->UpdateInterval);}
}

void UGOAPBrainComponent::Replan(const double Now)
{
    FGOAPPlanningState State; if(!BuildPlanningState(State))return;
    const FGuid Preferred=ActiveGoalId.IsValid()&&Now<GoalCommitUntil
        ?ActiveGoalId:FGuid();
    FString SelectionFailure;
    CurrentPlan=FGOAPPlanner::PlanBestEligibleGoal(CompiledDomain,State,
        Domain->MaximumExpandedNodes,Preferred,&LastGoalScores,&SelectionFailure);
    if(!CurrentPlan.bSucceeded)
    {
        if(bEnableTrace||CVarHellRunGOAPTrace.GetValueOnGameThread()!=0)
            UE_LOG(LogTemp,Warning,
                TEXT("GOAP: agent=%s result=no-plan reason=%s request=%s"),
                *GetNameSafe(GetOwner()),*SelectionFailure,
                *LastReplanReason);
        CurrentPlan={};RemainingActions.Reset();ActiveGoalId.Invalidate();
        bReplanRequested=false;LastPlanTime=Now;
        LastReplanReason=SelectionFailure;
        return;
    }
    RemainingActions=CurrentPlan.ActionIds; ActiveGoalId=CurrentPlan.GoalId;
    const FGOAPGoalScore* Selected=LastGoalScores.FindByPredicate(
        [&](const FGOAPGoalScore& Score){return Score.GoalId==ActiveGoalId;});
    if(!Selected)return;
    const int32* GoalIndex=CompiledDomain.GoalIndices.Find(ActiveGoalId);
    GoalCommitUntil=Now+(GoalIndex?CompiledDomain.Goals[*GoalIndex].CommitmentSeconds:0.0f);
    LastPlanTime=Now; bReplanRequested=false;
    OnPlanChanged.Broadcast(CurrentPlan);
    if(bEnableTrace||CVarHellRunGOAPTrace.GetValueOnGameThread()!=0)
        UE_LOG(LogTemp,Display,TEXT("GOAP: agent=%s goal=%s success=%d actions=%d cost=%.2f expanded=%d visited=%d reason=%s"),
            *GetNameSafe(GetOwner()),*Selected->GoalName.ToString(),CurrentPlan.bSucceeded?1:0,
            CurrentPlan.ActionIds.Num(),CurrentPlan.Cost,CurrentPlan.ExpandedNodes,CurrentPlan.VisitedStates,*LastReplanReason);
}

void UGOAPBrainComponent::BeginNextAction()
{
    if(RemainingActions.IsEmpty())return;
    ActiveActionId=RemainingActions[0]; RemainingActions.RemoveAt(0);
    const FGOAPCompiledAction* Action=FindAction(ActiveActionId);
    if(!Action||!ConditionsHold(Action->Preconditions))
    {FinishActiveAction(EGOAPTaskStatus::Failed,TEXT("Runtime preconditions diverged from plan"));return;}
    ActiveTask=Action->TaskTemplate
        ?DuplicateObject<UGOAPActionTask>(Action->TaskTemplate,this)
        :NewObject<UGOAPActionTask>(this,Action->TaskClass
            ?Action->TaskClass.Get():UGOAPActionTask_Immediate::StaticClass());
    ActiveTask->Initialize(this,Action->Id);
    ActionStartedAt=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
    const EGOAPTaskStatus Started=ActiveTask->Activate();
    OnActionChanged.Broadcast(Action->Name,Started);
    if(Started==EGOAPTaskStatus::Succeeded||Started==EGOAPTaskStatus::Failed)
        FinishActiveAction(Started,Started==EGOAPTaskStatus::Succeeded
            ?TEXT("Immediate action completed")
            :(ActiveTask->GetCompletionReason().IsEmpty()
                ?TEXT("Action activation failed")
                :ActiveTask->GetCompletionReason()));
}

void UGOAPBrainComponent::FinishActiveAction(const EGOAPTaskStatus Status,const FString& Reason)
{
    const FGOAPCompiledAction* Action=FindAction(ActiveActionId);
    if(Action&&Status==EGOAPTaskStatus::Succeeded)
    {
        FGOAPPlanningState State; BuildPlanningState(State);
        for(const FGOAPEffect& Effect:Action->Effects)
        {
            GOAPApplyEffect(Effect,CompiledDomain,State);
            if(const int32* Index=CompiledDomain.FactIndices.Find(Effect.FactId))
                SetFactById(Effect.FactId,State.Values[*Index],Action->Name,1.0f,Effect.Lifetime);
        }
    }
    if(Action)OnActionChanged.Broadcast(Action->Name,Status);
    ActiveTask=nullptr; ActiveActionId.Invalidate();
    if(Status!=EGOAPTaskStatus::Succeeded)
    {RemainingActions.Reset();RequestReplan(FString::Printf(TEXT("Action failed: %s"),*Reason));}
}

void UGOAPBrainComponent::TickComponent(const float DeltaTime,const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    if(!bRunning||!Domain)return;
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
    ExpireAgentFacts(Now);
    TickSensors(Now);
    const int32 DebugLevel=CVarHellRunGOAPDebug.GetValueOnGameThread();
    if(DebugLevel>0)
    {
        AActor* DisplayActor=GetOwner();
        if(const AController* Controller=Cast<AController>(GetOwner()))
            if(Controller->GetPawn())DisplayActor=Controller->GetPawn();
        const FGOAPBrainDebugSnapshot Debug=GetDebugSnapshot();
        FString Text=FString::Printf(TEXT("GOAP %s / %s\nPlan: %s\nWhy: %s"),
            *Debug.ActiveGoal.ToString(),*Debug.ActiveAction.ToString(),
            *FString::JoinBy(Debug.RemainingPlan,TEXT(" -> "),
                [](const FName Name){return Name.ToString();}),
            *Debug.LastReplanReason);
        if(DebugLevel>1)
            for(const FGOAPFactDebugEntry& Fact:Debug.Facts)
                Text+=FString::Printf(TEXT("\n%s=%s [%s:%s]"),*Fact.Name.ToString(),
                    *Fact.Value.ToString(),*UEnum::GetValueAsString(Fact.Scope),
                    Fact.bUsingDefault?TEXT("default"):*Fact.Source.ToString());
        DrawDebugString(GetWorld(),DisplayActor->GetActorLocation()
            +FVector::UpVector*145.0f,Text,DisplayActor,FColor::Cyan,0.0f,true);
    }
    if(ActiveTask)
    {
        const FGOAPCompiledAction* Action=FindAction(ActiveActionId);
        if(bReplanRequested&&Action&&Action->bInterruptible)
        {
            ActiveTask->Abort(LastReplanReason);
            FinishActiveAction(EGOAPTaskStatus::Aborted,LastReplanReason);
            return;
        }
        ActiveTask->TickTask(DeltaTime);
        Action=FindAction(ActiveActionId);
        if(Action&&Action->Timeout>0.0f&&Now-ActionStartedAt>=Action->Timeout)
        {ActiveTask->Abort(TEXT("Timeout"));}
        const EGOAPTaskStatus Status=ActiveTask->GetStatus();
        if(Status==EGOAPTaskStatus::Succeeded||Status==EGOAPTaskStatus::Failed
            ||Status==EGOAPTaskStatus::Aborted)
            FinishActiveAction(Status,ActiveTask->GetCompletionReason());
        return;
    }
    // Never begin the tail of an obsolete plan. Sensors and completed action
    // effects can invalidate the planning state between two operators. Queue a
    // fresh solve first and let that solve decide whether the old tail is still
    // valid. Starting the next task here used to make it run for one frame and
    // then get aborted on the following tick, which was especially visible as
    // Cultists repeatedly starting and cancelling their movement/fire actions.
    if(bReplanRequested&&!bPlanQueued&&Now-LastPlanTime>=Domain->MinimumReplanInterval)
    {
        if(UGOAPPlanningSubsystem* Planning=GetWorld()->GetSubsystem<UGOAPPlanningSubsystem>())
        {bPlanQueued=Planning->Enqueue(this);}
    }
    if(bReplanRequested||bPlanQueued)
        return;
    if(!ActiveTask&&!RemainingActions.IsEmpty())BeginNextAction();
    else if(!ActiveTask&&RemainingActions.IsEmpty()&&CurrentPlan.bSucceeded)
    {
        FGOAPPlanningState State;
        const int32* GoalIndex=CompiledDomain.GoalIndices.Find(ActiveGoalId);
        if(GoalIndex&&BuildPlanningState(State)
            &&!Algo::AllOf(CompiledDomain.Goals[*GoalIndex].DesiredState,[&](const FGOAPCondition& C){return GOAPEvaluateCondition(C,CompiledDomain,State);}))
            RequestReplan(TEXT("Goal effects no longer hold"));
    }
}

void UGOAPBrainComponent::ExecuteQueuedReplan()
{
    bPlanQueued=false;
    if(!bRunning||!bReplanRequested||ActiveTask||!GetWorld()) return;
    Replan(GetWorld()->GetTimeSeconds());
}

FGOAPBrainDebugSnapshot UGOAPBrainComponent::GetDebugSnapshot() const
{
    FGOAPBrainDebugSnapshot D; D.DomainName=Domain?Domain->GetFName():NAME_None;
    if(const int32* I=CompiledDomain.GoalIndices.Find(ActiveGoalId))D.ActiveGoal=CompiledDomain.Goals[*I].Name;
    if(const FGOAPCompiledAction* A=FindAction(ActiveActionId))D.ActiveAction=A->Name;
    D.ActionStatus=ActiveTask?ActiveTask->GetStatus():EGOAPTaskStatus::Inactive;
    for(const FGuid& Id:RemainingActions)if(const FGOAPCompiledAction* A=FindAction(Id))D.RemainingPlan.Add(A->Name);
    D.GoalScores=LastGoalScores; D.LastPlan=CurrentPlan; D.LastReplanReason=LastReplanReason;
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
    const UGOAPWorldStateSubsystem* Shared=GetWorld()
        ?GetWorld()->GetSubsystem<UGOAPWorldStateSubsystem>():nullptr;
    for(const FGOAPCompiledFact& Fact:CompiledDomain.Facts)
    {
        FGOAPFactDebugEntry& Entry=D.Facts.AddDefaulted_GetRef();
        Entry.Name=Fact.Name;Entry.Scope=Fact.Scope;Entry.Value=Fact.DefaultValue;
        FGOAPWorldFactRecord Record;bool bFound=false;
        if(Fact.Scope==EGOAPFactScope::Agent)
        {
            if(const FGOAPWorldFactRecord* Found=AgentFacts.Find(Fact.Id);
                Found&&!Found->IsExpired(Now)){Record=*Found;bFound=true;}
        }
        else if(Shared)bFound=Shared->GetSharedFact(Fact.Scope,SquadKey,Fact.Id,Record);
        if(bFound)
        {
            Entry.Value=Record.Value;Entry.Source=Record.Source;
            Entry.Confidence=Record.Confidence;Entry.bUsingDefault=false;
            Entry.ExpiresIn=Record.ExpiresAt>=0.0
                ?FMath::Max(0.0f,static_cast<float>(Record.ExpiresAt-Now)):-1.0f;
        }
    }
    D.WorldStateRevision=LocalRevision; return D;
}
