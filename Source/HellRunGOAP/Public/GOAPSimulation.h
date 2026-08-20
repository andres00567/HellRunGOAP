#pragma once

#include "CoreMinimal.h"
#include "GOAPDomain.h"
#include "GOAPSimulation.generated.h"

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPSimulationResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="GOAP|Simulation")
    FName CaseName;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="GOAP|Simulation")
    bool bPassed=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="GOAP|Simulation")
    FName SelectedGoal;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="GOAP|Simulation")
    TArray<FName> Plan;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="GOAP|Simulation")
    float Cost=0.0f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="GOAP|Simulation")
    int32 ExpandedNodes=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="GOAP|Simulation")
    TArray<FGOAPGoalScore> GoalScores;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="GOAP|Simulation")
    FString Message;
};

class HELLRUNGOAP_API FGOAPSimulator
{
public:
    static FGOAPSimulationResult Run(const UGOAPDomain& Domain,
        const FGOAPSimulationCase& TestCase);
};
