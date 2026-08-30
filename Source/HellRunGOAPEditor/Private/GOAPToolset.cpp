#include "GOAPToolset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "GOAPActionTask.h"
#include "GOAPDomain.h"
#include "GOAPSimulation.h"
#include "Modules/ModuleManager.h"

namespace
{
    UGOAPDomain* LoadDomain(const FString& AssetPath)
    {
        return LoadObject<UGOAPDomain>(nullptr, *AssetPath);
    }

    FString JoinNames(const TArray<FName>& Names)
    {
        TArray<FString> Strings;
        for (const FName Name : Names) Strings.Add(Name.ToString());
        return FString::Join(Strings, TEXT(" -> "));
    }

    TArray<FAssetData> FindDomainAssets()
    {
        TArray<FAssetData> Assets;
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
            .GetAssetsByClass(UGOAPDomain::StaticClass()->GetClassPathName(), Assets, true);
        Assets.Sort([](const FAssetData& A, const FAssetData& B)
        {
            return A.GetObjectPathString() < B.GetObjectPathString();
        });
        return Assets;
    }
}

FString UGOAPToolset::ListDomains(const int32 MaxResults)
{
    const int32 Limit = FMath::Clamp(MaxResults, 1, 200);
    const TArray<FAssetData> Assets = FindDomainAssets();
    FString Result = FString::Printf(TEXT("GOAP Domains: %d total; returning up to %d\n"), Assets.Num(), Limit);
    for (int32 Index = 0; Index < FMath::Min(Limit, Assets.Num()); ++Index) Result += Assets[Index].GetObjectPathString() + TEXT("\n");
    return Result;
}

FString UGOAPToolset::InspectDomain(const FString& AssetPath, const int32 MaxItemsPerSection)
{
    const UGOAPDomain* Domain = LoadDomain(AssetPath);
    if (!Domain) return FString::Printf(TEXT("ERROR: GOAP Domain not found: %s"), *AssetPath);
    const int32 Limit = FMath::Clamp(MaxItemsPerSection, 1, 250);
    FString Result = FString::Printf(TEXT("Domain: %s\nFacts=%d Actions=%d Goals=%d Sensors=%d SimulationCases=%d MaxExpandedNodes=%d MinimumReplanInterval=%.3f\n"),
        *Domain->GetPathName(), Domain->Facts.Num(), Domain->Actions.Num(), Domain->Goals.Num(), Domain->Sensors.Num(),
        Domain->SimulationCases.Num(), Domain->MaximumExpandedNodes, Domain->MinimumReplanInterval);
    Result += TEXT("Facts:\n");
    for (int32 Index = 0; Index < FMath::Min(Limit, Domain->Facts.Num()); ++Index)
        Result += FString::Printf(TEXT("- %s planning=%s triggersReplan=%s\n"), *Domain->Facts[Index].Name.ToString(),
            Domain->Facts[Index].bPlanningFact ? TEXT("true") : TEXT("false"), Domain->Facts[Index].bTriggersReplan ? TEXT("true") : TEXT("false"));
    Result += TEXT("Goals:\n");
    for (int32 Index = 0; Index < FMath::Min(Limit, Domain->Goals.Num()); ++Index)
        if (const UGOAPGoalDefinition* Goal = Domain->Goals[Index]) Result += FString::Printf(TEXT("- %s priority=%.2f desired=%d activation=%d\n"),
            *Goal->Name.ToString(), Goal->BasePriority, Goal->DesiredState.Num(), Goal->ActivationConditions.Num());
    Result += TEXT("Actions:\n");
    for (int32 Index = 0; Index < FMath::Min(Limit, Domain->Actions.Num()); ++Index)
        if (const UGOAPActionDefinition* Action = Domain->Actions[Index]) Result += FString::Printf(TEXT("- %s cost=%.2f timeout=%.2f preconditions=%d effects=%d task=%s\n"),
            *Action->Name.ToString(), Action->Cost, Action->Timeout, Action->Preconditions.Num(), Action->Effects.Num(), *GetPathNameSafe(Action->TaskTemplate.Get()));
    Result += TEXT("SimulationCases:\n");
    for (int32 Index = 0; Index < FMath::Min(Limit, Domain->SimulationCases.Num()); ++Index) Result += TEXT("- ") + Domain->SimulationCases[Index].Name.ToString() + TEXT("\n");
    return Result;
}

FString UGOAPToolset::InspectDomainValidation(const FString& AssetPath)
{
    const UGOAPDomain* Domain = LoadDomain(AssetPath);
    if (!Domain) return FString::Printf(TEXT("ERROR: GOAP Domain not found: %s"), *AssetPath);
    TArray<FText> Errors;
    TArray<FText> Warnings;
    const bool bValid = Domain->Validate(Errors, Warnings);
    FString Result = FString::Printf(TEXT("Validation: %s Errors=%d Warnings=%d\n"), bValid ? TEXT("valid") : TEXT("invalid"), Errors.Num(), Warnings.Num());
    for (const FText& Error : Errors) Result += TEXT("ERROR: ") + Error.ToString() + TEXT("\n");
    for (const FText& Warning : Warnings) Result += TEXT("WARNING: ") + Warning.ToString() + TEXT("\n");
    return Result;
}

FString UGOAPToolset::InspectAllDomainValidations(const int32 MaxResults)
{
    const int32 Limit = FMath::Clamp(MaxResults, 1, 200);
    const TArray<FAssetData> Assets = FindDomainAssets();
    const int32 Returned = FMath::Min(Limit, Assets.Num());
    int32 ValidCount = 0;
    int32 InvalidCount = 0;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;
    FString Details;
    for (int32 Index = 0; Index < Returned; ++Index)
    {
        const FString AssetPath = Assets[Index].GetObjectPathString();
        const UGOAPDomain* Domain = LoadDomain(AssetPath);
        if (!Domain)
        {
            ++InvalidCount;
            ++ErrorCount;
            Details += FString::Printf(TEXT("INVALID %s Errors=1 Warnings=0\nERROR: Domain could not be loaded.\n"), *AssetPath);
            continue;
        }
        TArray<FText> Errors;
        TArray<FText> Warnings;
        const bool bValid = Domain->Validate(Errors, Warnings);
        bValid ? ++ValidCount : ++InvalidCount;
        ErrorCount += Errors.Num();
        WarningCount += Warnings.Num();
        Details += FString::Printf(TEXT("%s %s Errors=%d Warnings=%d\n"),
            bValid ? TEXT("VALID") : TEXT("INVALID"), *AssetPath, Errors.Num(), Warnings.Num());
        for (const FText& Error : Errors) Details += TEXT("ERROR: ") + Error.ToString() + TEXT("\n");
        for (const FText& Warning : Warnings) Details += TEXT("WARNING: ") + Warning.ToString() + TEXT("\n");
    }
    return FString::Printf(TEXT("GOAP Domain Validation: total=%d returned=%d valid=%d invalid=%d errors=%d warnings=%d truncated=%s\n%s"),
        Assets.Num(), Returned, ValidCount, InvalidCount, ErrorCount, WarningCount,
        Returned < Assets.Num() ? TEXT("true") : TEXT("false"), *Details);
}

FString UGOAPToolset::RunSimulationCase(const FString& AssetPath, const FString& CaseName)
{
    const UGOAPDomain* Domain = LoadDomain(AssetPath);
    if (!Domain) return FString::Printf(TEXT("ERROR: GOAP Domain not found: %s"), *AssetPath);
    const FGOAPSimulationCase* SimulationCase = Domain->SimulationCases.FindByPredicate([&CaseName](const FGOAPSimulationCase& Candidate)
    {
        return Candidate.Name.ToString().Equals(CaseName, ESearchCase::IgnoreCase);
    });
    if (!SimulationCase) return FString::Printf(TEXT("ERROR: Simulation case not found: %s"), *CaseName);
    const FGOAPSimulationResult Result = FGOAPSimulator::Run(*Domain, *SimulationCase);
    return FString::Printf(TEXT("Case=%s Passed=%s Goal=%s Cost=%.3f ExpandedNodes=%d Plan=%s Message=%s"),
        *Result.CaseName.ToString(), Result.bPassed ? TEXT("true") : TEXT("false"), *Result.SelectedGoal.ToString(),
        Result.Cost, Result.ExpandedNodes, *JoinNames(Result.Plan), *Result.Message);
}
