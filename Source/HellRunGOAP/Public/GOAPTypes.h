// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "GameplayTagContainer.h"
#include "GOAPTypes.generated.h"

class UGOAPActionTask;

UENUM(BlueprintType)
enum class EGOAPValueType : uint8
{
    None,
    Bool,
    Integer,
    Float,
    Name,
    Vector,
    Object,
};

UENUM(BlueprintType)
enum class EGOAPFactScope : uint8
{
    Agent,
    Squad,
    World,
};

UENUM(BlueprintType)
enum class EGOAPComparison : uint8
{
    Equal,
    NotEqual,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    IsSet,
    IsNotSet,
};

UENUM(BlueprintType)
enum class EGOAPEffectOperation : uint8
{
    Set,
    Add,
    Subtract,
    Clear,
};

UENUM(BlueprintType)
enum class EGOAPTaskStatus : uint8
{
    Inactive,
    Running,
    Succeeded,
    Failed,
    Aborted,
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    EGOAPValueType Type = EGOAPValueType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP",
        meta=(EditCondition="Type == EGOAPValueType::Bool", EditConditionHides))
    bool BoolValue = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP",
        meta=(EditCondition="Type == EGOAPValueType::Integer", EditConditionHides))
    int32 IntegerValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP",
        meta=(EditCondition="Type == EGOAPValueType::Float", EditConditionHides))
    float FloatValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP",
        meta=(EditCondition="Type == EGOAPValueType::Name", EditConditionHides))
    FName NameValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP",
        meta=(EditCondition="Type == EGOAPValueType::Vector", EditConditionHides))
    FVector VectorValue = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP",
        meta=(EditCondition="Type == EGOAPValueType::Object", EditConditionHides))
    TObjectPtr<UObject> ObjectValue = nullptr;

    static FGOAPValue MakeBool(bool Value);
    static FGOAPValue MakeInteger(int32 Value);
    static FGOAPValue MakeFloat(float Value);
    static FGOAPValue MakeName(FName Value);
    static FGOAPValue MakeVector(const FVector& Value);
    static FGOAPValue MakeObject(UObject* Value);

    bool IsSet() const;
    bool Equals(const FGOAPValue& Other) const;
    double AsNumber(double Fallback = 0.0) const;
    FString ToString() const;
    friend uint32 GetTypeHash(const FGOAPValue& Value);
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPCondition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName FactName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP",
        meta=(AdvancedDisplay))
    FGuid FactId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    EGOAPComparison Comparison = EGOAPComparison::Equal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FGOAPValue Value;
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPEffect
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName FactName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP",
        meta=(AdvancedDisplay))
    FGuid FactId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    EGOAPEffectOperation Operation = EGOAPEffectOperation::Set;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FGOAPValue Value;

    /** Runtime lifetime of the produced fact. Zero is persistent. Useful for
     *  facts such as Suppressed, RecentlyFired, and Exposed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP",
        meta=(ClampMin="0.0",Units="s"))
    float Lifetime=0.0f;
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPUtilityConsideration
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName FactName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP",
        meta=(AdvancedDisplay))
    FGuid FactId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FRuntimeFloatCurve ResponseCurve;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    bool bInvert = false;
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPWorldFactRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FGOAPValue Value;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FName Source;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    float Confidence = 1.0f;

    double UpdatedAt = 0.0;
    double ExpiresAt = -1.0;

    bool IsExpired(double Now) const
    {
        return ExpiresAt >= 0.0 && Now >= ExpiresAt;
    }
};

struct HELLRUNGOAP_API FGOAPCompiledFact
{
    FGuid Id;
    FName Name;
    EGOAPValueType Type = EGOAPValueType::None;
    EGOAPFactScope Scope = EGOAPFactScope::Agent;
    FGOAPValue DefaultValue;
    bool bPlanningFact = true;
    bool bTriggersReplan = true;
    float ChangeTolerance = 0.01f;
};

struct HELLRUNGOAP_API FGOAPCompiledAction
{
    FGuid Id;
    FName Name;
    TArray<FGOAPCondition> Preconditions;
    TArray<FGOAPEffect> Effects;
    TSubclassOf<UGOAPActionTask> TaskClass;
    TObjectPtr<UGOAPActionTask> TaskTemplate;
    FGameplayTagContainer Tags;
    float Cost = 1.0f;
    float Timeout = 8.0f;
    bool bInterruptible = true;
};

struct HELLRUNGOAP_API FGOAPCompiledGoal
{
    FGuid Id;
    FName Name;
    TArray<FGOAPCondition> ActivationConditions;
    TArray<FGOAPCondition> DesiredState;
    TArray<FGOAPUtilityConsideration> Considerations;
    float BasePriority = 1.0f;
    float CommitmentSeconds = 0.5f;
};

struct HELLRUNGOAP_API FGOAPCompiledDomain
{
    TArray<FGOAPCompiledFact> Facts;
    TArray<FGOAPCompiledAction> Actions;
    TArray<FGOAPCompiledGoal> Goals;
    TMap<FGuid, int32> FactIndices;
    TMap<FGuid, int32> ActionIndices;
    TMap<FGuid, int32> GoalIndices;

    void Reset();
    bool IsValid() const
    { return !Facts.IsEmpty() && !Actions.IsEmpty() && !Goals.IsEmpty(); }
};

struct HELLRUNGOAP_API FGOAPPlanningState
{
    TArray<FGOAPValue> Values;

    bool operator==(const FGOAPPlanningState& Other) const;
    friend uint32 GetTypeHash(const FGOAPPlanningState& State);
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPGoalScore
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FGuid GoalId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FName GoalName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    float Score = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    bool bEligible = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FString Reason;
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPPlanResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    bool bSucceeded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FGuid GoalId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    TArray<FGuid> ActionIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    float Cost = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    int32 ExpandedNodes = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    int32 VisitedStates = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FString FailureReason;
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPFactDebugEntry
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FName Name;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    EGOAPFactScope Scope = EGOAPFactScope::Agent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FGOAPValue Value;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FName Source;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    float Confidence = 1.0f;

    /** Negative means the fact has no expiry. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    float ExpiresIn = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    bool bUsingDefault = true;
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPBrainDebugSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FName DomainName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FName ActiveGoal;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FName ActiveAction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    EGOAPTaskStatus ActionStatus = EGOAPTaskStatus::Inactive;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    TArray<FName> RemainingPlan;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    TArray<FGOAPGoalScore> GoalScores;

    /** Complete typed state used by the last/next solve, including provenance.
     *  This is deliberately copied only on a debug request, never per tick. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    TArray<FGOAPFactDebugEntry> Facts;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FGOAPPlanResult LastPlan;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FString LastReplanReason;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    int32 WorldStateRevision = 0;
};

HELLRUNGOAP_API bool GOAPEvaluateCondition(
    const FGOAPCondition& Condition, const FGOAPCompiledDomain& Domain,
    const FGOAPPlanningState& State);
HELLRUNGOAP_API void GOAPApplyEffect(
    const FGOAPEffect& Effect, const FGOAPCompiledDomain& Domain,
    FGOAPPlanningState& State);
