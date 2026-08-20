#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GOAPPlanningSubsystem.generated.h"

class UGOAPBrainComponent;

/** Central planning budget. It prevents a spawn wave from running every
 *  agent's A* search in the same frame. Action execution remains per-agent. */
UCLASS()
class HELLRUNGOAP_API UGOAPPlanningSubsystem final
    : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    bool Enqueue(UGOAPBrainComponent* Brain);

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableInEditor() const override { return false; }
    virtual void Deinitialize() override;

    UPROPERTY(EditAnywhere, Category="GOAP|Performance", meta=(ClampMin="1", ClampMax="64"))
    int32 MaximumPlansPerFrame=4;

    UPROPERTY(EditAnywhere, Category="GOAP|Performance", meta=(ClampMin="0.1", Units="ms"))
    float PlanningTimeBudgetMs=1.5f;

private:
    TArray<TWeakObjectPtr<UGOAPBrainComponent>> Queue;
    TSet<TWeakObjectPtr<UGOAPBrainComponent>> Queued;
};
