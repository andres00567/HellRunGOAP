#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GOAPDomain.h"
#include "GOAPBrainComponent.generated.h"

class UGOAPActionTask;
class UGOAPSensor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGOAPPlanChanged,
    const FGOAPPlanResult&, Plan);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGOAPActionChanged,
    FName, ActionName, EGOAPTaskStatus, Status);
DECLARE_MULTICAST_DELEGATE_OneParam(FGOAPRuntimeEventNative,
    const FGOAPRuntimeEvent&);

UCLASS(ClassGroup=(AI), BlueprintType, Blueprintable,
    meta=(BlueprintSpawnableComponent))
class HELLRUNGOAP_API UGOAPBrainComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UGOAPBrainComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GOAP")
    TObjectPtr<UGOAPDomain> Domain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName SquadKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    bool bStartLogicAutomatically=true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP|Debug")
    bool bEnableTrace=false;

    /** Optional caller-defined group used by per-world debug filters. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP|Debug")
    FName DebugGroup;

    /** A failed executor is temporarily removed from planning. This prevents
     * an agent from selecting the same unusable route or attack every replan. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP|Recovery",
        meta=(ClampMin="0.05", Units="s"))
    float ActionFailureCooldown=0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP|Recovery",
        meta=(ClampMin="0.05", Units="s"))
    float MaximumActionFailureCooldown=6.0f;

    UPROPERTY(BlueprintAssignable, Category="GOAP")
    FGOAPPlanChanged OnPlanChanged;

    UPROPERTY(BlueprintAssignable, Category="GOAP")
    FGOAPActionChanged OnActionChanged;

    /** Native, synchronous debugger stream. Subscribers copy events into their
     * own bounded storage and must remove their delegate handles on teardown. */
    FGOAPRuntimeEventNative& OnRuntimeEvent() { return RuntimeEvent; }
    /** Process-wide native stream for debugger services that must subscribe
     * before a PIE world or any brain exists. */
    static FGOAPRuntimeEventNative& OnAnyRuntimeEvent()
    { return AnyRuntimeEvent; }
    const FGuid& GetRuntimeAgentId() const { return RuntimeAgentId; }

    UFUNCTION(BlueprintCallable, Category="GOAP")
    bool StartLogic();

    UFUNCTION(BlueprintPure, Category="GOAP")
    bool IsRunning() const { return bRunning; }

    UFUNCTION(BlueprintCallable, Category="GOAP")
    void StopLogic(const FString& Reason=TEXT("Stopped"));

    UFUNCTION(BlueprintCallable, Category="GOAP")
    void RequestReplan(const FString& Reason);

    UFUNCTION(BlueprintCallable, Category="GOAP|World State")
    bool SetFact(FName FactName, const FGOAPValue& Value,
        FName Source=NAME_None, float Confidence=1.0f, float Lifetime=0.0f);

    UFUNCTION(BlueprintCallable, Category="GOAP|World State")
    bool SetFactById(const FGuid& FactId, const FGOAPValue& Value,
        FName Source=NAME_None, float Confidence=1.0f, float Lifetime=0.0f);

    UFUNCTION(BlueprintCallable, Category="GOAP|World State")
    bool ClearFact(FName FactName);

    UFUNCTION(BlueprintPure, Category="GOAP|World State")
    bool GetFact(FName FactName, FGOAPValue& OutValue) const;

    UFUNCTION(BlueprintPure, Category="GOAP|World State")
    bool GetBoolFact(FName FactName, bool& OutValue) const;

    UFUNCTION(BlueprintPure, Category="GOAP|World State")
    bool GetFloatFact(FName FactName, float& OutValue) const;

    UFUNCTION(BlueprintPure, Category="GOAP|World State")
    bool GetVectorFact(FName FactName, FVector& OutValue) const;

    UFUNCTION(BlueprintPure, Category="GOAP|World State")
    bool GetObjectFact(FName FactName, UObject*& OutValue) const;

    UFUNCTION(BlueprintCallable, Category="GOAP|World State")
    bool SetBoolFact(FName FactName,bool Value,FName Source=NAME_None,
        float Confidence=1.0f,float Lifetime=0.0f);

    UFUNCTION(BlueprintCallable, Category="GOAP|World State")
    bool SetFloatFact(FName FactName,float Value,FName Source=NAME_None,
        float Confidence=1.0f,float Lifetime=0.0f);

    UFUNCTION(BlueprintCallable, Category="GOAP|World State")
    bool SetVectorFact(FName FactName,FVector Value,FName Source=NAME_None,
        float Confidence=1.0f,float Lifetime=0.0f);

    UFUNCTION(BlueprintCallable, Category="GOAP|World State")
    bool SetObjectFact(FName FactName,UObject* Value,FName Source=NAME_None,
        float Confidence=1.0f,float Lifetime=0.0f);

    UFUNCTION(BlueprintPure, Category="GOAP|Debug")
    FGOAPBrainDebugSnapshot GetDebugSnapshot() const;

    bool BuildPlanningState(FGOAPPlanningState& OutState) const;
    const FGOAPCompiledDomain& GetCompiledDomain() const { return CompiledDomain; }

    /** Called only by the budgeted planning subsystem. */
    void ExecuteQueuedReplan();

    /** Exact shared-state invalidation called by the world-state subsystem. */
    void NotifySharedFactChanged(EGOAPFactScope Scope, FName ChangedSquad,
        const FGuid& FactId, EGOAPRuntimeEventType EventType,
        const FGOAPWorldFactRecord* OldRecord,
        const FGOAPWorldFactRecord* NewRecord, const FString& Reason);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    void TickSensors(double Now);
    void Replan(double Now);
    void BeginNextAction();
    void FinishActiveAction(EGOAPTaskStatus Status, const FString& Reason,
        bool bRequestAnotherPlan=true);
    FGOAPRuntimeEvent MakeRuntimeEvent(EGOAPRuntimeEventType Type,
        const FString& Reason=FString());
    void EmitRuntimeEvent(FGOAPRuntimeEvent Event);
    void FillPlanEvent(FGOAPRuntimeEvent& Event,
        const FGOAPPlanResult& Plan) const;
    void EmitFactEvent(EGOAPRuntimeEventType Type,
        const FGOAPCompiledFact& Fact,
        const FGOAPWorldFactRecord* OldRecord,
        const FGOAPWorldFactRecord* NewRecord, const FString& Reason);
    bool ConditionsHold(const TArray<FGOAPCondition>& Conditions) const;
    bool ValuesDiffer(const FGOAPCompiledFact& Fact,
        const FGOAPValue& A, const FGOAPValue& B) const;
    void ExpireAgentFacts(double Now);
    const FGOAPCompiledFact* FindFact(FName Name, int32* OutIndex=nullptr) const;
    const FGOAPCompiledAction* FindAction(const FGuid& Id) const;

    FGOAPCompiledDomain CompiledDomain;
    UPROPERTY(Transient)
    TMap<FGuid,FGOAPWorldFactRecord> AgentFacts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGOAPSensor>> Sensors;
    UPROPERTY(Transient)
    TObjectPtr<UGOAPActionTask> ActiveTask;

    FGOAPPlanResult CurrentPlan;
    TArray<FGuid> RemainingActions;
    TArray<FGOAPGoalScore> LastGoalScores;
    FGuid ActiveGoalId;
    FGuid ActiveActionId;
    FString LastReplanReason;
    double LastPlanTime=-BIG_NUMBER;
    double GoalCommitUntil=0.0;
    double ActionStartedAt=0.0;
    double RecoveryReplanAt=-BIG_NUMBER;
    TMap<FGuid,double> ActionRetryAfter;
    TMap<FGuid,int32> ConsecutiveActionFailures;
    FGOAPRuntimeEventNative RuntimeEvent;
    static FGOAPRuntimeEventNative AnyRuntimeEvent;
    FGuid RuntimeAgentId;
    int32 LocalRevision=0;
    bool bRunning=false;
    bool bReplanRequested=true;
    bool bPlanQueued=false;
};
