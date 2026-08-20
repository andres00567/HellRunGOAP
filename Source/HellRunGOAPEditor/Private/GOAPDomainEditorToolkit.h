#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Misc/NotifyHook.h"
#include "GOAPSimulation.h"

class IDetailsView;
class SGraphEditor;
class UGOAPDomain;

class FGOAPDomainEditorToolkit final : public FAssetEditorToolkit,
    public FNotifyHook, public FGCObject
{
public:
    void Initialize(UGOAPDomain* InDomain,
        TSharedPtr<IToolkitHost> InToolkitHost);

    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual FString GetWorldCentricTabPrefix() const override;
    virtual FLinearColor GetWorldCentricTabColorScale() const override;
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override { return TEXT("FGOAPDomainEditorToolkit"); }
    virtual void NotifyPostChange(const FPropertyChangedEvent& Event,
        FProperty* PropertyThatChanged) override;

private:
    TSharedRef<SDockTab> SpawnGraphTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnValidationTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnSimulationTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnDebuggerTab(const FSpawnTabArgs& Args);
    void BindCommands();
    void RefreshAll();
    void ValidateDomain();
    void RunSimulations();
    void AddFact();
    void AddAction();
    void AddGoal();
    void DeleteSelectedNodes();
    void HandleSelectionChanged(const TSet<UObject*>& Selection);
    EActiveTimerReturnType RefreshDebugger(double CurrentTime,float DeltaTime);

    static const FName GraphTabId;
    static const FName DetailsTabId;
    static const FName ValidationTabId;
    static const FName SimulationTabId;
    static const FName DebuggerTabId;

    TObjectPtr<UGOAPDomain> Domain;
    TSharedPtr<SGraphEditor> GraphEditor;
    TSharedPtr<IDetailsView> DetailsView;
    TSharedPtr<SVerticalBox> ValidationBox;
    TSharedPtr<SVerticalBox> SimulationBox;
    TSharedPtr<SVerticalBox> DebuggerBox;
    TArray<FText> ValidationErrors;
    TArray<FText> ValidationWarnings;
    TArray<FGOAPSimulationResult> SimulationResults;
};
