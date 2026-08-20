#include "GOAPSimulation.h"
#include "GOAPPlanner.h"

FGOAPSimulationResult FGOAPSimulator::Run(const UGOAPDomain& Domain,
    const FGOAPSimulationCase& TestCase)
{
    FGOAPSimulationResult Result; Result.CaseName=TestCase.Name;
    FGOAPCompiledDomain Compiled; TArray<FText> Errors;
    if(!Domain.Compile(Compiled,&Errors))
    {
        Result.Message=Errors.IsEmpty()?TEXT("Domain compilation failed")
            :Errors[0].ToString();
        return Result;
    }
    FGOAPPlanningState State; State.Values.Reserve(Compiled.Facts.Num());
    for(const FGOAPCompiledFact& Fact:Compiled.Facts)
        State.Values.Add(Fact.DefaultValue);
    for(const TPair<FName,FGOAPValue>& Pair:TestCase.InitialWorldState)
    {
        const FGOAPCompiledFact* Fact=Compiled.Facts.FindByPredicate(
            [&](const FGOAPCompiledFact& F){return F.Name==Pair.Key;});
        const int32* Index=Fact?Compiled.FactIndices.Find(Fact->Id):nullptr;
        if(!Index||Compiled.Facts[*Index].Type!=Pair.Value.Type)
        {Result.Message=TEXT("Simulation contains a missing or incorrectly typed fact");return Result;}
        State.Values[*Index]=Pair.Value;
    }
    FGuid GoalId;
    if(!TestCase.ForcedGoalName.IsNone())
        if(const FGOAPCompiledGoal* G=Compiled.Goals.FindByPredicate(
            [&](const FGOAPCompiledGoal& Item){return Item.Name==TestCase.ForcedGoalName;}))GoalId=G->Id;
    if(!GoalId.IsValid())
    {
        FGOAPPlanner::ScoreGoals(Compiled,State,Result.GoalScores);
        if(const FGOAPGoalScore* Best=Result.GoalScores.FindByPredicate(
            [](const FGOAPGoalScore& S){return S.bEligible&&S.Score>0.0f;}))
            GoalId=Best->GoalId;
    }
    else FGOAPPlanner::ScoreGoals(Compiled,State,Result.GoalScores);
    const int32* GoalIndex=Compiled.GoalIndices.Find(GoalId);
    if(!GoalIndex){Result.Message=TEXT("No eligible or forced goal");return Result;}
    Result.SelectedGoal=Compiled.Goals[*GoalIndex].Name;
    const FGOAPPlanResult Plan=FGOAPPlanner::Plan(Compiled,State,GoalId,
        Domain.MaximumExpandedNodes);
    Result.Cost=Plan.Cost; Result.ExpandedNodes=Plan.ExpandedNodes;
    for(const FGuid& Id:Plan.ActionIds)
        if(const int32* I=Compiled.ActionIndices.Find(Id))
            Result.Plan.Add(Compiled.Actions[*I].Name);
    if(!Plan.bSucceeded){Result.Message=Plan.FailureReason;return Result;}
    if(TestCase.ExpectedActionPrefix.Num()>Plan.ActionIds.Num())
    {Result.Message=TEXT("Plan is shorter than the expected prefix");return Result;}
    for(int32 I=0;I<TestCase.ExpectedActionPrefix.Num();++I)
        if(!Compiled.ActionIndices.Contains(Plan.ActionIds[I])
            ||Compiled.Actions[*Compiled.ActionIndices.Find(Plan.ActionIds[I])].Name
                !=TestCase.ExpectedActionPrefix[I])
        {Result.Message=FString::Printf(TEXT("Expected action %d does not match"),I);return Result;}
    Result.bPassed=true;
    Result.Message=TEXT("Plan reached the desired world state");
    return Result;
}
