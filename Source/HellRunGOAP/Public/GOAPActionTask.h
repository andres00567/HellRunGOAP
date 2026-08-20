#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GOAPTypes.h"
#include "GOAPActionTask.generated.h"

class UGOAPBrainComponent;

UCLASS(Abstract, Blueprintable, EditInlineNew)
class HELLRUNGOAP_API UGOAPActionTask : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(UGOAPBrainComponent* InBrain, const FGuid& InActionId);

    /** Runtime-owned lifecycle entry point. Always use this instead of calling
     *  the Blueprint event directly so the task state cannot desynchronize. */
    EGOAPTaskStatus Activate();

    /** Runtime-owned abort entry point. */
    void Abort(const FString& Reason);

    UFUNCTION(BlueprintNativeEvent, Category="GOAP|Action")
    EGOAPTaskStatus StartTask();
    virtual EGOAPTaskStatus StartTask_Implementation();

    UFUNCTION(BlueprintNativeEvent, Category="GOAP|Action")
    void TickTask(float DeltaSeconds);
    virtual void TickTask_Implementation(float DeltaSeconds) {}

    UFUNCTION(BlueprintNativeEvent, Category="GOAP|Action")
    void AbortTask(const FString& Reason);
    virtual void AbortTask_Implementation(const FString& Reason) {}

    UFUNCTION(BlueprintCallable, Category="GOAP|Action")
    void Succeed(const FString& Reason = TEXT("Completed"));

    UFUNCTION(BlueprintCallable, Category="GOAP|Action")
    void Fail(const FString& Reason);

    UFUNCTION(BlueprintPure, Category="GOAP|Action")
    UGOAPBrainComponent* GetBrain() const { return Brain.Get(); }

    UFUNCTION(BlueprintPure, Category="GOAP|Action")
    AActor* GetAgent() const;

    UFUNCTION(BlueprintPure, Category="GOAP|Action")
    EGOAPTaskStatus GetStatus() const { return Status; }

    UFUNCTION(BlueprintPure, Category="GOAP|Action")
    FGuid GetActionId() const { return ActionId; }

    const FString& GetCompletionReason() const { return CompletionReason; }

private:
    TWeakObjectPtr<UGOAPBrainComponent> Brain;
    FGuid ActionId;
    EGOAPTaskStatus Status=EGOAPTaskStatus::Inactive;
    FString CompletionReason;
};

/** Immediate task for symbolic and simulation-friendly actions. */
UCLASS(Blueprintable)
class HELLRUNGOAP_API UGOAPActionTask_Immediate : public UGOAPActionTask
{
    GENERATED_BODY()
public:
    virtual EGOAPTaskStatus StartTask_Implementation() override
    { return EGOAPTaskStatus::Succeeded; }
};
