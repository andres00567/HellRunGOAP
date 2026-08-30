#pragma once

#include "CoreMinimal.h"
#include "GOAPTypes.h"

class HELLRUNGOAP_API FGOAPPlanner
{
public:
    static FGOAPPlanResult Plan(const FGOAPCompiledDomain& Domain,
        const FGOAPPlanningState& InitialState, const FGuid& GoalId,
        int32 MaximumExpandedNodes = 256,
        const TSet<FGuid>* ExcludedActions = nullptr);
    /** Utility-order goal selection with feasibility fallback. A temporarily
     *  unreachable high-priority goal must not prevent a lower-priority
     *  executable plan from running. PreferredGoal is tried first while a
     *  brain's commitment window is active, but is never forced. */
    static FGOAPPlanResult PlanBestEligibleGoal(
        const FGOAPCompiledDomain& Domain,
        const FGOAPPlanningState& InitialState,
        int32 MaximumExpandedNodes = 256,
        const FGuid& PreferredGoal = FGuid(),
        TArray<FGOAPGoalScore>* OutScores = nullptr,
        FString* OutFailure = nullptr,
        const TSet<FGuid>* ExcludedActions = nullptr);
    static void ScoreGoals(const FGOAPCompiledDomain& Domain,
        const FGOAPPlanningState& State, TArray<FGOAPGoalScore>& OutScores);
};
