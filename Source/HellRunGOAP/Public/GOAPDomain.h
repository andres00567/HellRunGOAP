// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GOAPTypes.h"
#include "GOAPDomain.generated.h"

class UEdGraph;
class UGOAPSensor;

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPFactDefinition
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    EGOAPValueType Type = EGOAPValueType::Bool;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    EGOAPFactScope Scope = EGOAPFactScope::Agent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FGOAPValue DefaultValue = FGOAPValue::MakeBool(false);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    bool bPlanningFact = true;

    /** Disable for high-frequency presentation data that action tasks read but
     *  which should not invalidate a symbolic plan on every small change. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    bool bTriggersReplan = true;

    /** Equality tolerance for Float and Vector facts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP",
        meta=(ClampMin="0.0"))
    float ChangeTolerance = 0.01f;

    void EnsureId() { if (!Id.IsValid()) Id = FGuid::NewGuid(); }
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class HELLRUNGOAP_API UGOAPActionDefinition : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP", meta=(MultiLine=true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    TArray<FGOAPCondition> Preconditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    TArray<FGOAPEffect> Effects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    TSubclassOf<class UGOAPActionTask> TaskClass;

    /** Instanced action configuration. Prefer this over TaskClass when one task
     *  implementation needs different target facts, ranges, or policies. */
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category="GOAP")
    TObjectPtr<class UGOAPActionTask> TaskTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FGameplayTagContainer Tags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP", meta=(ClampMin="0.001"))
    float Cost = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP", meta=(ClampMin="0.0", Units="s"))
    float Timeout = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    bool bInterruptible = true;

    void EnsureId() { if (!Id.IsValid()) Id = FGuid::NewGuid(); }

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class HELLRUNGOAP_API UGOAPGoalDefinition : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GOAP")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP", meta=(MultiLine=true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    TArray<FGOAPCondition> ActivationConditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    TArray<FGOAPCondition> DesiredState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    TArray<FGOAPUtilityConsideration> Considerations;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP", meta=(ClampMin="0.0"))
    float BasePriority = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP", meta=(ClampMin="0.0", Units="s"))
    float CommitmentSeconds = 0.5f;

    void EnsureId() { if (!Id.IsValid()) Id = FGuid::NewGuid(); }

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};

USTRUCT(BlueprintType)
struct HELLRUNGOAP_API FGOAPSimulationCase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    TMap<FName, FGOAPValue> InitialWorldState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    FName ForcedGoalName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GOAP")
    TArray<FName> ExpectedActionPrefix;
};

UCLASS(BlueprintType)
class HELLRUNGOAP_API UGOAPDomain : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GOAP")
    TArray<FGOAPFactDefinition> Facts;

    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="GOAP")
    TArray<TObjectPtr<UGOAPActionDefinition>> Actions;

    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="GOAP")
    TArray<TObjectPtr<UGOAPGoalDefinition>> Goals;

    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="GOAP")
    TArray<TObjectPtr<UGOAPSensor>> Sensors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GOAP|Planning",
        meta=(ClampMin="8", ClampMax="4096"))
    int32 MaximumExpandedNodes = 256;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GOAP|Planning",
        meta=(ClampMin="0.01", Units="s"))
    float MinimumReplanInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GOAP|Simulation")
    TArray<FGOAPSimulationCase> SimulationCases;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UEdGraph> EditorGraph;
#endif

    bool Compile(FGOAPCompiledDomain& OutDomain,
        TArray<FText>* OutErrors = nullptr) const;
    bool Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;
    const FGOAPFactDefinition* FindFact(const FGuid& Id) const;
    const FGOAPFactDefinition* FindFact(FName Name) const;

#if WITH_EDITOR
    void NormalizeReferences();
#endif

#if WITH_EDITOR
    virtual void PostLoad() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};
