#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "GOAPToolset.generated.h"

/** Granular GOAP domain inspection and deterministic simulation operations. */
UCLASS(BlueprintType, Hidden)
class HELLRUNGOAPEDITOR_API UGOAPToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /** Lists bounded GOAP Domain asset paths. */
    UFUNCTION(meta=(AICallable), Category="GOAP|Tools")
    static FString ListDomains(int32 MaxResults = 50);

    /** Returns a bounded structural summary of facts, goals, actions, sensors, and simulation cases. */
    UFUNCTION(meta=(AICallable), Category="GOAP|Tools")
    static FString InspectDomain(const FString& AssetPath, int32 MaxItemsPerSection = 100);

    /** Runs the domain's normal validation and reports every error and warning without mutating the asset. */
    UFUNCTION(meta=(AICallable), Category="GOAP|Tools")
    static FString InspectDomainValidation(const FString& AssetPath);

    /** Validates every bounded GOAP Domain in one read-only operation, avoiding a list-then-call round trip. */
    UFUNCTION(meta=(AICallable), Category="GOAP|Tools")
    static FString InspectAllDomainValidations(int32 MaxResults = 50);

    /** Runs one authored deterministic simulation case by exact case name. */
    UFUNCTION(meta=(AICallable), Category="GOAP|Tools")
    static FString RunSimulationCase(const FString& AssetPath, const FString& CaseName);
};
