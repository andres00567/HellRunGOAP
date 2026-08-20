#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GOAPDomain.h"
#include "GOAPPlanner.h"
#include "GOAPSimulation.h"

namespace
{
    FGOAPFactDefinition Fact(FName Name,EGOAPValueType Type,
        const FGOAPValue& Default)
    {
        FGOAPFactDefinition F;F.EnsureId();F.Name=Name;F.Type=Type;
        F.DefaultValue=Default;return F;
    }
    FGOAPCondition Equals(const FGOAPFactDefinition& F,const FGOAPValue& V)
    {FGOAPCondition C;C.FactName=F.Name;C.FactId=F.Id;C.Value=V;return C;}
    FGOAPEffect Sets(const FGOAPFactDefinition& F,const FGOAPValue& V)
    {FGOAPEffect E;E.FactName=F.Name;E.FactId=F.Id;E.Value=V;return E;}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGOAPChoosesLeastCostPlan,
    "HellRun.GOAP.Planner.ChoosesLeastCostPlan",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FGOAPChoosesLeastCostPlan::RunTest(const FString&)
{
    UGOAPDomain* Domain=NewObject<UGOAPDomain>();
    Domain->Facts.Add(Fact(TEXT("HasTarget"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    Domain->Facts.Add(Fact(TEXT("InCover"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    UGOAPActionDefinition* Direct=NewObject<UGOAPActionDefinition>(Domain);
    Direct->EnsureId();Direct->Name=TEXT("Rush");Direct->Cost=8;
    Direct->Effects.Add(Sets(Domain->Facts[1],FGOAPValue::MakeBool(true)));
    Domain->Actions.Add(Direct);
    UGOAPActionDefinition* Acquire=NewObject<UGOAPActionDefinition>(Domain);
    Acquire->EnsureId();Acquire->Name=TEXT("AcquireTarget");Acquire->Cost=1;
    Acquire->Effects.Add(Sets(Domain->Facts[0],FGOAPValue::MakeBool(true)));
    Domain->Actions.Add(Acquire);
    UGOAPActionDefinition* Cover=NewObject<UGOAPActionDefinition>(Domain);
    Cover->EnsureId();Cover->Name=TEXT("MoveToCover");Cover->Cost=2;
    Cover->Preconditions.Add(Equals(Domain->Facts[0],FGOAPValue::MakeBool(true)));
    Cover->Effects.Add(Sets(Domain->Facts[1],FGOAPValue::MakeBool(true)));
    Domain->Actions.Add(Cover);
    UGOAPGoalDefinition* Goal=NewObject<UGOAPGoalDefinition>(Domain);
    Goal->EnsureId();Goal->Name=TEXT("Survive");
    Goal->DesiredState.Add(Equals(Domain->Facts[1],FGOAPValue::MakeBool(true)));
    Domain->Goals.Add(Goal);
    FGOAPCompiledDomain Compiled;TArray<FText> Errors;
    TestTrue(TEXT("Domain compiles"),Domain->Compile(Compiled,&Errors));
    FGOAPPlanningState State;for(const auto& F:Compiled.Facts)State.Values.Add(F.DefaultValue);
    const FGOAPPlanResult Plan=FGOAPPlanner::Plan(Compiled,State,Goal->Id,128);
    TestTrue(TEXT("Plan succeeds"),Plan.bSucceeded);
    TestEqual(TEXT("Uses two lower-cost actions"),Plan.ActionIds.Num(),2);
    if(Plan.ActionIds.Num()==2)
    {TestEqual(TEXT("Acquire first"),Plan.ActionIds[0],Acquire->Id);TestEqual(TEXT("Cover second"),Plan.ActionIds[1],Cover->Id);}
    TestEqual(TEXT("Cost is optimal"),Plan.Cost,3.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGOAPRejectsInvalidDomain,
    "HellRun.GOAP.Domain.RejectsTypeMismatch",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FGOAPRejectsInvalidDomain::RunTest(const FString&)
{
    UGOAPDomain* D=NewObject<UGOAPDomain>();
    D->Facts.Add(Fact(TEXT("Alive"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    UGOAPActionDefinition* A=NewObject<UGOAPActionDefinition>(D);A->EnsureId();A->Name=TEXT("Bad");
    A->Effects.Add(Sets(D->Facts[0],FGOAPValue::MakeVector(FVector::ZeroVector)));D->Actions.Add(A);
    UGOAPGoalDefinition* G=NewObject<UGOAPGoalDefinition>(D);G->EnsureId();G->Name=TEXT("Live");
    G->DesiredState.Add(Equals(D->Facts[0],FGOAPValue::MakeBool(true)));D->Goals.Add(G);
    TArray<FText>E,W;TestFalse(TEXT("Type mismatch rejected"),D->Validate(E,W));
    TestTrue(TEXT("Validation reports an error"),!E.IsEmpty());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGOAPSimulationMatchesPrefix,
    "HellRun.GOAP.Simulation.ExpectedPrefix",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FGOAPSimulationMatchesPrefix::RunTest(const FString&)
{
    UGOAPDomain* D=NewObject<UGOAPDomain>();
    D->Facts.Add(Fact(TEXT("Done"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    UGOAPActionDefinition* A=NewObject<UGOAPActionDefinition>(D);A->EnsureId();A->Name=TEXT("DoIt");
    A->Effects.Add(Sets(D->Facts[0],FGOAPValue::MakeBool(true)));D->Actions.Add(A);
    UGOAPGoalDefinition* G=NewObject<UGOAPGoalDefinition>(D);G->EnsureId();G->Name=TEXT("Finish");
    G->DesiredState.Add(Equals(D->Facts[0],FGOAPValue::MakeBool(true)));D->Goals.Add(G);
    FGOAPSimulationCase C;C.Name=TEXT("Basic");C.ForcedGoalName=G->Name;C.ExpectedActionPrefix.Add(A->Name);
    const FGOAPSimulationResult R=FGOAPSimulator::Run(*D,C);
    TestTrue(TEXT("Simulation passes"),R.bPassed);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGOAPSkipsSatisfiedHighPriorityGoal,
    "HellRun.GOAP.Goals.SkipsAlreadySatisfiedGoal",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FGOAPSkipsSatisfiedHighPriorityGoal::RunTest(const FString&)
{
    UGOAPDomain* D=NewObject<UGOAPDomain>();
    D->Facts.Add(Fact(TEXT("Safe"),EGOAPValueType::Bool,FGOAPValue::MakeBool(true)));
    D->Facts.Add(Fact(TEXT("Pressure"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    UGOAPActionDefinition* A=NewObject<UGOAPActionDefinition>(D);A->EnsureId();A->Name=TEXT("PressureEnemy");
    A->Effects.Add(Sets(D->Facts[1],FGOAPValue::MakeBool(true)));D->Actions.Add(A);
    UGOAPGoalDefinition* Safe=NewObject<UGOAPGoalDefinition>(D);Safe->EnsureId();Safe->Name=TEXT("Survive");Safe->BasePriority=20;
    Safe->DesiredState.Add(Equals(D->Facts[0],FGOAPValue::MakeBool(true)));D->Goals.Add(Safe);
    UGOAPGoalDefinition* Pressure=NewObject<UGOAPGoalDefinition>(D);Pressure->EnsureId();Pressure->Name=TEXT("Attack");Pressure->BasePriority=10;
    Pressure->DesiredState.Add(Equals(D->Facts[1],FGOAPValue::MakeBool(true)));D->Goals.Add(Pressure);
    FGOAPCompiledDomain Compiled;TArray<FText> Errors;
    TestTrue(TEXT("Domain compiles"),D->Compile(Compiled,&Errors));
    FGOAPPlanningState State;for(const auto& F:Compiled.Facts)State.Values.Add(F.DefaultValue);
    TArray<FGOAPGoalScore> Scores;FGOAPPlanner::ScoreGoals(Compiled,State,Scores);
    const FGOAPGoalScore* FirstEligible=Scores.FindByPredicate(
        [](const FGOAPGoalScore& Score){return Score.bEligible;});
    TestTrue(TEXT("Another goal remains eligible"),FirstEligible!=nullptr);
    if(FirstEligible)TestEqual(TEXT("Satisfied safety goal yields to attack"),
        FirstEligible->GoalName,FName(TEXT("Attack")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGOAPCoordinatedSquadPlan,
    "HellRun.GOAP.Planner.CoordinatesSuppressionAndFlank",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FGOAPCoordinatedSquadPlan::RunTest(const FString&)
{
    UGOAPDomain* D=NewObject<UGOAPDomain>();
    D->Facts.Add(Fact(TEXT("HasFlank"),EGOAPValueType::Bool,FGOAPValue::MakeBool(true)));
    D->Facts.Add(Fact(TEXT("Suppressed"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    D->Facts.Add(Fact(TEXT("AtFlank"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    D->Facts.Add(Fact(TEXT("FlankAttack"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    UGOAPActionDefinition* Move=NewObject<UGOAPActionDefinition>(D);Move->EnsureId();Move->Name=TEXT("MoveFlank");
    Move->Preconditions={Equals(D->Facts[0],FGOAPValue::MakeBool(true)),Equals(D->Facts[1],FGOAPValue::MakeBool(true))};
    Move->Effects.Add(Sets(D->Facts[2],FGOAPValue::MakeBool(true)));D->Actions.Add(Move);
    UGOAPActionDefinition* Fire=NewObject<UGOAPActionDefinition>(D);Fire->EnsureId();Fire->Name=TEXT("FireFlank");
    Fire->Preconditions.Add(Equals(D->Facts[2],FGOAPValue::MakeBool(true)));
    Fire->Effects.Add(Sets(D->Facts[3],FGOAPValue::MakeBool(true)));D->Actions.Add(Fire);
    UGOAPGoalDefinition* G=NewObject<UGOAPGoalDefinition>(D);G->EnsureId();G->Name=TEXT("GainAdvantage");
    G->ActivationConditions.Add(Equals(D->Facts[1],FGOAPValue::MakeBool(true)));
    G->DesiredState.Add(Equals(D->Facts[3],FGOAPValue::MakeBool(true)));D->Goals.Add(G);
    FGOAPCompiledDomain Compiled;TArray<FText> Errors;
    TestTrue(TEXT("Domain compiles"),D->Compile(Compiled,&Errors));
    FGOAPPlanningState State;for(const auto& F:Compiled.Facts)State.Values.Add(F.DefaultValue);
    const FGOAPPlanResult WithoutSuppression=FGOAPPlanner::Plan(Compiled,State,G->Id,64);
    TestFalse(TEXT("Flank cannot begin before shared suppression"),WithoutSuppression.bSucceeded);
    State.Values[1]=FGOAPValue::MakeBool(true);
    const FGOAPPlanResult Coordinated=FGOAPPlanner::Plan(Compiled,State,G->Id,64);
    TestTrue(TEXT("Shared suppression unlocks flank chain"),Coordinated.bSucceeded);
    TestEqual(TEXT("Flank chain has movement and fire"),Coordinated.ActionIds.Num(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGOAPFallsBackFromUnreachablePriorityGoal,
    "HellRun.GOAP.Goals.FallsBackFromUnreachablePriorityGoal",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FGOAPFallsBackFromUnreachablePriorityGoal::RunTest(const FString&)
{
    UGOAPDomain* D=NewObject<UGOAPDomain>();
    D->Facts.Add(Fact(TEXT("CoverKnown"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    D->Facts.Add(Fact(TEXT("InCover"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    D->Facts.Add(Fact(TEXT("HasLane"),EGOAPValueType::Bool,FGOAPValue::MakeBool(true)));
    D->Facts.Add(Fact(TEXT("Fired"),EGOAPValueType::Bool,FGOAPValue::MakeBool(false)));
    UGOAPActionDefinition* Cover=NewObject<UGOAPActionDefinition>(D);
    Cover->EnsureId();Cover->Name=TEXT("MoveToCover");
    Cover->Preconditions.Add(Equals(D->Facts[0],FGOAPValue::MakeBool(true)));
    Cover->Effects.Add(Sets(D->Facts[1],FGOAPValue::MakeBool(true)));D->Actions.Add(Cover);
    UGOAPActionDefinition* Fire=NewObject<UGOAPActionDefinition>(D);
    Fire->EnsureId();Fire->Name=TEXT("FireFromPosition");
    Fire->Preconditions.Add(Equals(D->Facts[2],FGOAPValue::MakeBool(true)));
    Fire->Effects.Add(Sets(D->Facts[3],FGOAPValue::MakeBool(true)));D->Actions.Add(Fire);
    UGOAPGoalDefinition* Survive=NewObject<UGOAPGoalDefinition>(D);
    Survive->EnsureId();Survive->Name=TEXT("Survive");Survive->BasePriority=20;
    Survive->DesiredState.Add(Equals(D->Facts[1],FGOAPValue::MakeBool(true)));D->Goals.Add(Survive);
    UGOAPGoalDefinition* Engage=NewObject<UGOAPGoalDefinition>(D);
    Engage->EnsureId();Engage->Name=TEXT("Engage");Engage->BasePriority=5;
    Engage->DesiredState.Add(Equals(D->Facts[3],FGOAPValue::MakeBool(true)));D->Goals.Add(Engage);
    FGOAPCompiledDomain Compiled;TArray<FText> Errors;
    TestTrue(TEXT("Domain compiles"),D->Compile(Compiled,&Errors));
    FGOAPPlanningState State;for(const auto& F:Compiled.Facts)State.Values.Add(F.DefaultValue);
    TArray<FGOAPGoalScore> Scores;
    const FGOAPPlanResult Plan=FGOAPPlanner::PlanBestEligibleGoal(
        Compiled,State,64,FGuid(),&Scores);
    TestTrue(TEXT("A lower utility executable goal still produces a plan"),Plan.bSucceeded);
    TestEqual(TEXT("Executable engage goal selected"),Plan.GoalId,Engage->Id);
    TestEqual(TEXT("Fire action runs"),Plan.ActionIds.Num(),1);
    if(Plan.ActionIds.Num()==1)TestEqual(TEXT("Selected action is fire"),Plan.ActionIds[0],Fire->Id);
    return true;
}

#endif
