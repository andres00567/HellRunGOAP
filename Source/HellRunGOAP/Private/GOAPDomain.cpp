#include "GOAPDomain.h"
#include "GOAPActionTask.h"
#include "Algo/AllOf.h"

namespace
{
    bool CheckCondition(const FGOAPCondition& Condition,
        const TMap<FGuid, const FGOAPFactDefinition*>& Facts,
        const TMap<FName, const FGOAPFactDefinition*>& NamedFacts,FText& OutError)
    {
        const FGOAPFactDefinition* const* Fact = Facts.Find(Condition.FactId);
        if(!Fact&&!Condition.FactName.IsNone())Fact=NamedFacts.Find(Condition.FactName);
        if (!Fact)
        {
            OutError = NSLOCTEXT("GOAP", "MissingConditionFact",
                "A condition references a missing fact.");
            return false;
        }
        if (Condition.Comparison != EGOAPComparison::IsSet
            && Condition.Comparison != EGOAPComparison::IsNotSet
            && Condition.Value.Type != (*Fact)->Type)
        {
            OutError = FText::Format(NSLOCTEXT("GOAP", "ConditionTypeMismatch",
                "Condition for fact '{0}' has the wrong value type."),
                FText::FromName((*Fact)->Name));
            return false;
        }
        return true;
    }

    bool CheckEffect(const FGOAPEffect& Effect,
        const TMap<FGuid, const FGOAPFactDefinition*>& Facts,
        const TMap<FName, const FGOAPFactDefinition*>& NamedFacts,FText& OutError)
    {
        const FGOAPFactDefinition* const* Fact = Facts.Find(Effect.FactId);
        if(!Fact&&!Effect.FactName.IsNone())Fact=NamedFacts.Find(Effect.FactName);
        if (!Fact)
        {
            OutError = NSLOCTEXT("GOAP", "MissingEffectFact",
                "An effect references a missing fact.");
            return false;
        }
        if (Effect.Operation != EGOAPEffectOperation::Clear
            && Effect.Value.Type != (*Fact)->Type)
        {
            OutError = FText::Format(NSLOCTEXT("GOAP", "EffectTypeMismatch",
                "Effect for fact '{0}' has the wrong value type."),
                FText::FromName((*Fact)->Name));
            return false;
        }
        return true;
    }
}

bool UGOAPDomain::Compile(FGOAPCompiledDomain& OutDomain,
    TArray<FText>* OutErrors) const
{
    OutDomain.Reset();
    TArray<FText> Errors, Warnings;
    if (!Validate(Errors, Warnings))
    {
        if (OutErrors) *OutErrors = MoveTemp(Errors);
        return false;
    }
    for (const FGOAPFactDefinition& Fact : Facts)
    {
        FGOAPCompiledFact& Compiled = OutDomain.Facts.AddDefaulted_GetRef();
        Compiled.Id=Fact.Id; Compiled.Name=Fact.Name; Compiled.Type=Fact.Type;
        Compiled.Scope=Fact.Scope; Compiled.DefaultValue=Fact.DefaultValue;
        Compiled.bPlanningFact=Fact.bPlanningFact;
        Compiled.bTriggersReplan=Fact.bTriggersReplan;
        Compiled.ChangeTolerance=Fact.ChangeTolerance;
        OutDomain.FactIndices.Add(Compiled.Id, OutDomain.Facts.Num()-1);
    }
    for (const UGOAPActionDefinition* Action : Actions)
    {
        FGOAPCompiledAction& Compiled = OutDomain.Actions.AddDefaulted_GetRef();
        Compiled.Id=Action->Id; Compiled.Name=Action->Name;
        Compiled.Preconditions=Action->Preconditions; Compiled.Effects=Action->Effects;
        for(FGOAPCondition& C:Compiled.Preconditions)
            if(const FGOAPFactDefinition* F=C.FactName.IsNone()?FindFact(C.FactId):FindFact(C.FactName))
            {C.FactId=F->Id;C.FactName=F->Name;}
        for(FGOAPEffect& E:Compiled.Effects)
            if(const FGOAPFactDefinition* F=E.FactName.IsNone()?FindFact(E.FactId):FindFact(E.FactName))
            {E.FactId=F->Id;E.FactName=F->Name;}
        Compiled.TaskClass=Action->TaskClass; Compiled.Tags=Action->Tags;
        Compiled.TaskTemplate=Action->TaskTemplate;
        Compiled.Cost=Action->Cost; Compiled.Timeout=Action->Timeout;
        Compiled.bInterruptible=Action->bInterruptible;
        OutDomain.ActionIndices.Add(Compiled.Id, OutDomain.Actions.Num()-1);
    }
    for (const UGOAPGoalDefinition* Goal : Goals)
    {
        FGOAPCompiledGoal& Compiled = OutDomain.Goals.AddDefaulted_GetRef();
        Compiled.Id=Goal->Id; Compiled.Name=Goal->Name;
        Compiled.ActivationConditions=Goal->ActivationConditions;
        Compiled.DesiredState=Goal->DesiredState;
        Compiled.Considerations=Goal->Considerations;
        for(FGOAPCondition& C:Compiled.ActivationConditions)
            if(const FGOAPFactDefinition* F=C.FactName.IsNone()?FindFact(C.FactId):FindFact(C.FactName))
            {C.FactId=F->Id;C.FactName=F->Name;}
        for(FGOAPCondition& C:Compiled.DesiredState)
            if(const FGOAPFactDefinition* F=C.FactName.IsNone()?FindFact(C.FactId):FindFact(C.FactName))
            {C.FactId=F->Id;C.FactName=F->Name;}
        for(FGOAPUtilityConsideration& C:Compiled.Considerations)
            if(const FGOAPFactDefinition* F=C.FactName.IsNone()?FindFact(C.FactId):FindFact(C.FactName))
            {C.FactId=F->Id;C.FactName=F->Name;}
        Compiled.BasePriority=Goal->BasePriority;
        Compiled.CommitmentSeconds=Goal->CommitmentSeconds;
        OutDomain.GoalIndices.Add(Compiled.Id, OutDomain.Goals.Num()-1);
    }
    return true;
}

bool UGOAPDomain::Validate(TArray<FText>& OutErrors,
    TArray<FText>& OutWarnings) const
{
    OutErrors.Reset(); OutWarnings.Reset();
    TMap<FGuid, const FGOAPFactDefinition*> FactMap;
    TMap<FName, const FGOAPFactDefinition*> NamedFactMap;
    TSet<FName> Names;
    for (const FGOAPFactDefinition& Fact : Facts)
    {
        if (!Fact.Id.IsValid() || Fact.Name.IsNone())
            OutErrors.Add(NSLOCTEXT("GOAP", "InvalidFact", "Every fact needs a stable id and name."));
        else if (FactMap.Contains(Fact.Id) || Names.Contains(Fact.Name))
            OutErrors.Add(FText::Format(NSLOCTEXT("GOAP", "DuplicateFact", "Duplicate fact '{0}'."), FText::FromName(Fact.Name)));
        else { FactMap.Add(Fact.Id, &Fact);NamedFactMap.Add(Fact.Name,&Fact); Names.Add(Fact.Name); }
        if (Fact.Type != Fact.DefaultValue.Type)
            OutErrors.Add(FText::Format(NSLOCTEXT("GOAP", "DefaultType", "Default value for '{0}' has the wrong type."), FText::FromName(Fact.Name)));
    }
    TSet<FGuid> NodeIds;
    TSet<FName> NodeNames;
    for (const UGOAPActionDefinition* Action : Actions)
    {
        if (!Action || !Action->Id.IsValid() || Action->Name.IsNone())
        { OutErrors.Add(NSLOCTEXT("GOAP", "InvalidAction", "Every action needs a stable id and name.")); continue; }
        if (NodeIds.Contains(Action->Id)) OutErrors.Add(FText::Format(NSLOCTEXT("GOAP", "DuplicateAction", "Duplicate action '{0}'."), FText::FromName(Action->Name)));
        NodeIds.Add(Action->Id);
        if(NodeNames.Contains(Action->Name)) OutErrors.Add(FText::Format(
            NSLOCTEXT("GOAP","DuplicateActionName","Duplicate action name '{0}'."),FText::FromName(Action->Name)));
        NodeNames.Add(Action->Name);
        if (Action->Effects.IsEmpty()) OutWarnings.Add(FText::Format(NSLOCTEXT("GOAP", "NoEffects", "Action '{0}' has no planning effects."), FText::FromName(Action->Name)));
        if(!Action->TaskClass&&!Action->TaskTemplate) OutWarnings.Add(FText::Format(
            NSLOCTEXT("GOAP","SymbolicAction","Action '{0}' has no task class and will only apply symbolic effects."),FText::FromName(Action->Name)));
        for (const FGOAPCondition& C : Action->Preconditions) { FText E; if(!CheckCondition(C,FactMap,NamedFactMap,E)) OutErrors.Add(E); }
        for (const FGOAPEffect& E : Action->Effects) { FText Error; if(!CheckEffect(E,FactMap,NamedFactMap,Error)) OutErrors.Add(Error); }
    }
    for (const UGOAPGoalDefinition* Goal : Goals)
    {
        if (!Goal || !Goal->Id.IsValid() || Goal->Name.IsNone())
        { OutErrors.Add(NSLOCTEXT("GOAP", "InvalidGoal", "Every goal needs a stable id and name.")); continue; }
        if (NodeIds.Contains(Goal->Id)) OutErrors.Add(FText::Format(NSLOCTEXT("GOAP", "DuplicateGoal", "Duplicate goal '{0}'."), FText::FromName(Goal->Name)));
        NodeIds.Add(Goal->Id);
        if(NodeNames.Contains(Goal->Name)) OutErrors.Add(FText::Format(
            NSLOCTEXT("GOAP","DuplicateGoalName","Goal/action name '{0}' is not unique."),FText::FromName(Goal->Name)));
        NodeNames.Add(Goal->Name);
        if (Goal->DesiredState.IsEmpty()) OutErrors.Add(FText::Format(NSLOCTEXT("GOAP", "NoGoalState", "Goal '{0}' has no desired state."), FText::FromName(Goal->Name)));
        for (const FGOAPCondition& C : Goal->ActivationConditions) { FText E; if(!CheckCondition(C,FactMap,NamedFactMap,E)) OutErrors.Add(E); }
        for (const FGOAPCondition& C : Goal->DesiredState) { FText E; if(!CheckCondition(C,FactMap,NamedFactMap,E)) OutErrors.Add(E); }
        for(const FGOAPUtilityConsideration& C:Goal->Considerations)
        {
            const FGOAPFactDefinition* const* Fact=FactMap.Find(C.FactId);
            if(!Fact&&!C.FactName.IsNone())Fact=NamedFactMap.Find(C.FactName);
            if(!Fact)OutErrors.Add(FText::Format(NSLOCTEXT("GOAP","MissingUtilityFact",
                "Utility consideration on goal '{0}' references a missing fact."),FText::FromName(Goal->Name)));
            else if((*Fact)->Type!=EGOAPValueType::Bool&&(*Fact)->Type!=EGOAPValueType::Integer
                &&(*Fact)->Type!=EGOAPValueType::Float)
                OutErrors.Add(FText::Format(NSLOCTEXT("GOAP","NonNumericUtilityFact",
                    "Utility consideration '{0}' must reference a bool, integer, or float fact."),
                    FText::FromName((*Fact)->Name)));
        }
    }
    if(Goals.IsEmpty()) OutErrors.Add(NSLOCTEXT("GOAP","NoGoals","A GOAP domain needs at least one goal."));
    if(!OutErrors.IsEmpty()) return false;

    // Relaxed reachability analysis catches black-box domains whose desired
    // states cannot be produced from defaults by any action chain.
    TSet<FGuid> ReachableFacts;
    for(const FGOAPFactDefinition& Fact:Facts)
        if(Fact.DefaultValue.IsSet()) ReachableFacts.Add(Fact.Id);
    bool bChanged=true;
    while(bChanged)
    {
        bChanged=false;
        for(const UGOAPActionDefinition* Action:Actions)
        {
            if(!Action)continue;
            const bool bInputsReachable=Algo::AllOf(Action->Preconditions,
                [&](const FGOAPCondition& C){return ReachableFacts.Contains(C.FactId);});
            if(!bInputsReachable)continue;
            for(const FGOAPEffect& Effect:Action->Effects)
                if(!ReachableFacts.Contains(Effect.FactId))
                {ReachableFacts.Add(Effect.FactId);bChanged=true;}
        }
    }
    for(const UGOAPGoalDefinition* Goal:Goals)
        if(Goal)
            for(const FGOAPCondition& Desired:Goal->DesiredState)
                if(!ReachableFacts.Contains(Desired.FactId))
                    OutWarnings.Add(FText::Format(NSLOCTEXT("GOAP","UnreachableGoal",
                        "Goal '{0}' references a fact no reachable action can establish."),
                        FText::FromName(Goal->Name)));
    return OutErrors.IsEmpty();
}

const FGOAPFactDefinition* UGOAPDomain::FindFact(const FGuid& Id) const
{
    return Facts.FindByPredicate([&](const FGOAPFactDefinition& F){return F.Id==Id;});
}

const FGOAPFactDefinition* UGOAPDomain::FindFact(const FName Name) const
{
    return Facts.FindByPredicate([&](const FGOAPFactDefinition& F){return F.Name==Name;});
}

#if WITH_EDITOR
void UGOAPDomain::PostLoad()
{
    Super::PostLoad();
    for (FGOAPFactDefinition& Fact : Facts) Fact.EnsureId();
    for (UGOAPActionDefinition* Action : Actions) if (Action) Action->EnsureId();
    for (UGOAPGoalDefinition* Goal : Goals) if (Goal) Goal->EnsureId();
    NormalizeReferences();
}

void UGOAPDomain::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    for (FGOAPFactDefinition& Fact : Facts)
    {
        Fact.EnsureId();
        Fact.DefaultValue.Type = Fact.Type;
    }
    for (UGOAPActionDefinition* Action : Actions) if (Action) Action->EnsureId();
    for (UGOAPGoalDefinition* Goal : Goals) if (Goal) Goal->EnsureId();
    NormalizeReferences();
    Super::PostEditChangeProperty(Event);
}

void UGOAPDomain::NormalizeReferences()
{
    auto NormalizeCondition=[&](FGOAPCondition& C)
    {
        const FGOAPFactDefinition* F=!C.FactName.IsNone()?FindFact(C.FactName):FindFact(C.FactId);
        if(F){C.FactId=F->Id;C.FactName=F->Name;
            if(C.Comparison!=EGOAPComparison::IsSet&&C.Comparison!=EGOAPComparison::IsNotSet)C.Value.Type=F->Type;}
    };
    auto NormalizeEffect=[&](FGOAPEffect& E)
    {
        const FGOAPFactDefinition* F=!E.FactName.IsNone()?FindFact(E.FactName):FindFact(E.FactId);
        if(F){E.FactId=F->Id;E.FactName=F->Name;if(E.Operation!=EGOAPEffectOperation::Clear)E.Value.Type=F->Type;}
    };
    for(UGOAPActionDefinition* A:Actions)if(A)
    {for(FGOAPCondition& C:A->Preconditions)NormalizeCondition(C);for(FGOAPEffect& E:A->Effects)NormalizeEffect(E);}
    for(UGOAPGoalDefinition* G:Goals)if(G)
    {
        for(FGOAPCondition& C:G->ActivationConditions)NormalizeCondition(C);
        for(FGOAPCondition& C:G->DesiredState)NormalizeCondition(C);
        for(FGOAPUtilityConsideration& C:G->Considerations)
            if(const FGOAPFactDefinition* F=!C.FactName.IsNone()?FindFact(C.FactName):FindFact(C.FactId))
            {C.FactId=F->Id;C.FactName=F->Name;}
    }
    for(FGOAPSimulationCase& C:SimulationCases)
        for(TPair<FName,FGOAPValue>& P:C.InitialWorldState)
            if(const FGOAPFactDefinition* F=FindFact(P.Key))P.Value.Type=F->Type;
}

void UGOAPActionDefinition::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    if(UGOAPDomain* D=GetTypedOuter<UGOAPDomain>()){D->NormalizeReferences();D->MarkPackageDirty();}
}

void UGOAPGoalDefinition::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    if(UGOAPDomain* D=GetTypedOuter<UGOAPDomain>()){D->NormalizeReferences();D->MarkPackageDirty();}
}
#endif
