#include "GOAPPlanner.h"
#include "Algo/AllOf.h"
#include "Algo/Count.h"

namespace
{
    bool AllConditions(const TArray<FGOAPCondition>& Conditions,
        const FGOAPCompiledDomain& Domain, const FGOAPPlanningState& State)
    {
        return Algo::AllOf(Conditions, [&](const FGOAPCondition& C)
        { return GOAPEvaluateCondition(C, Domain, State); });
    }

    int32 Unsatisfied(const TArray<FGOAPCondition>& Conditions,
        const FGOAPCompiledDomain& Domain, const FGOAPPlanningState& State)
    {
        return Algo::CountIf(Conditions, [&](const FGOAPCondition& C)
        { return !GOAPEvaluateCondition(C, Domain, State); });
    }

    float AdmissibleLowerBound(const TArray<FGOAPCondition>& Conditions,
        const FGOAPCompiledDomain& Domain, const FGOAPPlanningState& State)
    {
        if (Unsatisfied(Conditions, Domain, State) == 0) return 0.0f;
        float CheapestAction = TNumericLimits<float>::Max();
        for (const FGOAPCompiledAction& Action : Domain.Actions)
            CheapestAction = FMath::Min(CheapestAction,
                FMath::Max(0.001f, Action.Cost));
        return CheapestAction == TNumericLimits<float>::Max()
            ? 0.0f : CheapestAction;
    }

    struct FNode
    {
        FGOAPPlanningState State;
        TArray<FGuid, TInlineAllocator<8>> Plan;
        float Cost = 0.0f;
        float Estimate = 0.0f;
    };
}

FGOAPPlanResult FGOAPPlanner::Plan(const FGOAPCompiledDomain& Domain,
    const FGOAPPlanningState& InitialState, const FGuid& GoalId,
    const int32 MaximumExpandedNodes)
{
    FGOAPPlanResult Result; Result.GoalId=GoalId;
    const int32* GoalIndex=Domain.GoalIndices.Find(GoalId);
    if(!GoalIndex){Result.FailureReason=TEXT("Goal is not part of the compiled domain");return Result;}
    const FGOAPCompiledGoal& Goal=Domain.Goals[*GoalIndex];
    if(!AllConditions(Goal.ActivationConditions,Domain,InitialState))
    {Result.FailureReason=TEXT("Goal activation conditions are not satisfied");return Result;}
    if(AllConditions(Goal.DesiredState,Domain,InitialState)){Result.bSucceeded=true;return Result;}

    TArray<FNode,TInlineAllocator<128>> Open;
    TMap<FGOAPPlanningState,float> Best;
    FNode& Root=Open.AddDefaulted_GetRef(); Root.State=InitialState;
    Root.Estimate=AdmissibleLowerBound(Goal.DesiredState,Domain,InitialState);
    Best.Add(InitialState,0.0f);
    const int32 Limit=FMath::Clamp(MaximumExpandedNodes,8,4096);
    while(!Open.IsEmpty()&&Result.ExpandedNodes<Limit)
    {
        int32 Cheapest=0;
        for(int32 I=1;I<Open.Num();++I)
            if(Open[I].Cost+Open[I].Estimate<Open[Cheapest].Cost+Open[Cheapest].Estimate) Cheapest=I;
        FNode Node=MoveTemp(Open[Cheapest]); Open.RemoveAtSwap(Cheapest,1,EAllowShrinking::No);
        ++Result.ExpandedNodes;
        if(AllConditions(Goal.DesiredState,Domain,Node.State))
        {
            Result.bSucceeded=true; Result.Cost=Node.Cost;
            Result.ActionIds=Node.Plan; Result.VisitedStates=Best.Num(); return Result;
        }
        for(const FGOAPCompiledAction& Action:Domain.Actions)
        {
            if(!AllConditions(Action.Preconditions,Domain,Node.State)) continue;
            FGOAPPlanningState Next=Node.State;
            for(const FGOAPEffect& Effect:Action.Effects) GOAPApplyEffect(Effect,Domain,Next);
            if(Next==Node.State) continue;
            const float NewCost=Node.Cost+FMath::Max(0.001f,Action.Cost);
            if(const float* Existing=Best.Find(Next);Existing&&*Existing<=NewCost) continue;
            Best.Add(Next,NewCost);
            FNode& Child=Open.AddDefaulted_GetRef(); Child.State=MoveTemp(Next);
            Child.Cost=NewCost;
            Child.Estimate=AdmissibleLowerBound(Goal.DesiredState,Domain,Child.State);
            Child.Plan=Node.Plan; Child.Plan.Add(Action.Id);
        }
    }
    Result.VisitedStates=Best.Num();
    Result.FailureReason=Result.ExpandedNodes>=Limit
        ? TEXT("Planner expansion budget exhausted") : TEXT("No action sequence reaches the goal");
    return Result;
}

FGOAPPlanResult FGOAPPlanner::PlanBestEligibleGoal(
    const FGOAPCompiledDomain& Domain,const FGOAPPlanningState& InitialState,
    const int32 MaximumExpandedNodes,const FGuid& PreferredGoal,
    TArray<FGOAPGoalScore>* OutScores,FString* OutFailure)
{
    TArray<FGOAPGoalScore> LocalScores;
    TArray<FGOAPGoalScore>& Scores=OutScores?*OutScores:LocalScores;
    ScoreGoals(Domain,InitialState,Scores);
    TArray<const FGOAPGoalScore*,TInlineAllocator<8>> Candidates;
    for(const FGOAPGoalScore& Score:Scores)
        if(Score.bEligible&&Score.Score>0.0f)Candidates.Add(&Score);
    if(PreferredGoal.IsValid())
    {
        const int32 PreferredIndex=Candidates.IndexOfByPredicate(
            [&](const FGOAPGoalScore* Score)
            {return Score&&Score->GoalId==PreferredGoal;});
        if(PreferredIndex>0)Candidates.Swap(0,PreferredIndex);
    }
    FString Failures;
    for(const FGOAPGoalScore* Candidate:Candidates)
    {
        FGOAPPlanResult Attempt=Plan(Domain,InitialState,Candidate->GoalId,
            MaximumExpandedNodes);
        if(Attempt.bSucceeded&&!Attempt.ActionIds.IsEmpty())
        {
            if(OutFailure)OutFailure->Reset();
            return Attempt;
        }
        if(!Failures.IsEmpty())Failures+=TEXT("; ");
        Failures+=FString::Printf(TEXT("%s: %s"),
            *Candidate->GoalName.ToString(),Attempt.bSucceeded
                ?TEXT("already converged"):*Attempt.FailureReason);
    }
    FGOAPPlanResult Failed;
    Failed.FailureReason=Candidates.IsEmpty()
        ?TEXT("No eligible unsatisfied goal")
        :FString::Printf(TEXT("No executable eligible goal (%s)"),*Failures);
    if(OutFailure)*OutFailure=Failed.FailureReason;
    return Failed;
}

void FGOAPPlanner::ScoreGoals(const FGOAPCompiledDomain& Domain,
    const FGOAPPlanningState& State, TArray<FGOAPGoalScore>& OutScores)
{
    OutScores.Reset();
    for(const FGOAPCompiledGoal& Goal:Domain.Goals)
    {
        FGOAPGoalScore& Score=OutScores.AddDefaulted_GetRef();
        Score.GoalId=Goal.Id; Score.GoalName=Goal.Name;
        Score.bEligible=AllConditions(Goal.ActivationConditions,Domain,State);
        if(!Score.bEligible){Score.Reason=TEXT("Activation conditions failed");continue;}
        // A satisfied goal is not a decision. Leaving it eligible lets a high
        // priority safety goal monopolize selection after the agent is already
        // safe, producing a successful empty plan and an idle character.
        if(AllConditions(Goal.DesiredState,Domain,State))
        {
            Score.bEligible=false;
            Score.Reason=TEXT("Desired state already satisfied");
            continue;
        }
        float Utility=FMath::Max(0.0f,Goal.BasePriority);
        for(const FGOAPUtilityConsideration& C:Goal.Considerations)
        {
            const int32* Index=Domain.FactIndices.Find(C.FactId);
            if(!Index||!State.Values.IsValidIndex(*Index)){Utility=0.0f;break;}
            float Input=static_cast<float>(State.Values[*Index].AsNumber());
            if(C.bInvert) Input=1.0f-Input;
            const FRichCurve* Curve=C.ResponseCurve.GetRichCurveConst();
            Utility*=FMath::Max(0.0f,(Curve&&Curve->GetNumKeys()>0?Curve->Eval(Input):Input)*C.Weight);
        }
        Score.Score=Utility; Score.Reason=TEXT("Eligible");
    }
    OutScores.Sort([](const FGOAPGoalScore& A,const FGOAPGoalScore& B){return A.Score>B.Score;});
}
