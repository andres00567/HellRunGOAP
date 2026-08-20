#include "GOAPDomainEditorToolkit.h"
#include "GOAPDomain.h"
#include "GOAPEditorGraph.h"
#include "GOAPWorldStateSubsystem.h"
#include "GOAPBrainComponent.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "GraphEditor.h"
#include "PropertyEditorModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

const FName FGOAPDomainEditorToolkit::GraphTabId(TEXT("GOAP_Graph"));
const FName FGOAPDomainEditorToolkit::DetailsTabId(TEXT("GOAP_Details"));
const FName FGOAPDomainEditorToolkit::ValidationTabId(TEXT("GOAP_Validation"));
const FName FGOAPDomainEditorToolkit::SimulationTabId(TEXT("GOAP_Simulation"));
const FName FGOAPDomainEditorToolkit::DebuggerTabId(TEXT("GOAP_Debugger"));

void FGOAPDomainEditorToolkit::Initialize(UGOAPDomain* InDomain,
    TSharedPtr<IToolkitHost> InToolkitHost)
{
    Domain=InDomain;
    HellRunGOAPEditorGraph::Rebuild(*Domain);
    FPropertyEditorModule& PropertyEditor=FModuleManager::LoadModuleChecked<
        FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs DetailsArgs; DetailsArgs.NotifyHook=this;
    DetailsView=PropertyEditor.CreateDetailView(DetailsArgs);
    DetailsView->SetObject(Domain);
    BindCommands();

    const TSharedRef<FTabManager::FLayout> Layout=FTabManager::NewLayout(
        "HellRunGOAPDomainEditor_v1")
        ->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
        ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.08f)
            ->SetHideTabWell(true))
        ->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
            ->SetSizeCoefficient(0.70f)
            ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.72f)
                ->AddTab(GraphTabId,ETabState::OpenedTab))
            ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.28f)
                ->AddTab(DetailsTabId,ETabState::OpenedTab)))
        ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.22f)
            ->AddTab(ValidationTabId,ETabState::OpenedTab)
            ->AddTab(SimulationTabId,ETabState::OpenedTab)
            ->AddTab(DebuggerTabId,ETabState::OpenedTab)));
    InitAssetEditor(EToolkitMode::Standalone,InToolkitHost,
        TEXT("HellRunGOAPDomainEditor"),Layout,true,true,Domain);
    RegenerateMenusAndToolbars();
    RefreshAll();
}

FName FGOAPDomainEditorToolkit::GetToolkitFName() const
{ return TEXT("HellRunGOAPDomainEditor"); }
FText FGOAPDomainEditorToolkit::GetBaseToolkitName() const
{ return NSLOCTEXT("HellRunGOAP","EditorName","GOAP Domain"); }
FString FGOAPDomainEditorToolkit::GetWorldCentricTabPrefix() const
{ return TEXT("GOAP "); }
FLinearColor FGOAPDomainEditorToolkit::GetWorldCentricTabColorScale() const
{ return FLinearColor(0.05f,0.42f,0.38f); }

void FGOAPDomainEditorToolkit::RegisterTabSpawners(
    const TSharedRef<FTabManager>& Manager)
{
    FAssetEditorToolkit::RegisterTabSpawners(Manager);
    WorkspaceMenuCategory=Manager->AddLocalWorkspaceMenuCategory(
        NSLOCTEXT("HellRunGOAP","Workspace","GOAP Domain"));
    Manager->RegisterTabSpawner(GraphTabId,FOnSpawnTab::CreateSP(this,
        &FGOAPDomainEditorToolkit::SpawnGraphTab)).SetDisplayName(FText::FromString("Plan Graph"));
    Manager->RegisterTabSpawner(DetailsTabId,FOnSpawnTab::CreateSP(this,
        &FGOAPDomainEditorToolkit::SpawnDetailsTab)).SetDisplayName(FText::FromString("Details"));
    Manager->RegisterTabSpawner(ValidationTabId,FOnSpawnTab::CreateSP(this,
        &FGOAPDomainEditorToolkit::SpawnValidationTab)).SetDisplayName(FText::FromString("Validation"));
    Manager->RegisterTabSpawner(SimulationTabId,FOnSpawnTab::CreateSP(this,
        &FGOAPDomainEditorToolkit::SpawnSimulationTab)).SetDisplayName(FText::FromString("Simulation"));
    Manager->RegisterTabSpawner(DebuggerTabId,FOnSpawnTab::CreateSP(this,
        &FGOAPDomainEditorToolkit::SpawnDebuggerTab)).SetDisplayName(FText::FromString("Live Debugger"));
}

void FGOAPDomainEditorToolkit::UnregisterTabSpawners(
    const TSharedRef<FTabManager>& Manager)
{
    Manager->UnregisterTabSpawner(GraphTabId); Manager->UnregisterTabSpawner(DetailsTabId);
    Manager->UnregisterTabSpawner(ValidationTabId); Manager->UnregisterTabSpawner(SimulationTabId);
    Manager->UnregisterTabSpawner(DebuggerTabId);
    FAssetEditorToolkit::UnregisterTabSpawners(Manager);
}

void FGOAPDomainEditorToolkit::BindCommands()
{
    const TSharedRef<FUICommandList> Commands=GetToolkitCommands();
    Commands->MapAction(FGenericCommands::Get().Delete,
        FExecuteAction::CreateSP(this,&FGOAPDomainEditorToolkit::DeleteSelectedNodes));
    TSharedPtr<FExtender> Extender=MakeShared<FExtender>();
    Extender->AddToolBarExtension("Asset",EExtensionHook::After,Commands,
        FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& B)
        {
            B.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this,&FGOAPDomainEditorToolkit::AddFact)),
                NAME_None,FText::FromString("Add Fact"),FText::FromString("Add a typed world-state fact"));
            B.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this,&FGOAPDomainEditorToolkit::AddAction)),
                NAME_None,FText::FromString("Add Action"),FText::FromString("Add an action definition"));
            B.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this,&FGOAPDomainEditorToolkit::AddGoal)),
                NAME_None,FText::FromString("Add Goal"),FText::FromString("Add a utility-scored goal"));
            B.AddSeparator();
            B.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this,&FGOAPDomainEditorToolkit::ValidateDomain)),
                NAME_None,FText::FromString("Validate"),FText::FromString("Validate types and reachability"));
            B.AddToolBarButton(FUIAction(FExecuteAction::CreateSP(this,&FGOAPDomainEditorToolkit::RunSimulations)),
                NAME_None,FText::FromString("Simulate"),FText::FromString("Run deterministic planning cases"));
        }));
    AddToolbarExtender(Extender);
}

TSharedRef<SDockTab> FGOAPDomainEditorToolkit::SpawnGraphTab(const FSpawnTabArgs&)
{
    FGraphAppearanceInfo Appearance; Appearance.CornerText=FText::FromString("GOAP / A*");
    SGraphEditor::FGraphEditorEvents Events;
    Events.OnSelectionChanged=SGraphEditor::FOnSelectionChanged::CreateSP(this,
        &FGOAPDomainEditorToolkit::HandleSelectionChanged);
    SAssignNew(GraphEditor,SGraphEditor).GraphToEdit(Domain->EditorGraph)
        .Appearance(Appearance).IsEditable(true).GraphEvents(Events)
        .AdditionalCommands(GetToolkitCommands());
    return SNew(SDockTab)[GraphEditor.ToSharedRef()];
}

TSharedRef<SDockTab> FGOAPDomainEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs&)
{ return SNew(SDockTab)[DetailsView.ToSharedRef()]; }

TSharedRef<SDockTab> FGOAPDomainEditorToolkit::SpawnValidationTab(const FSpawnTabArgs&)
{
    return SNew(SDockTab)[SNew(SScrollBox)+SScrollBox::Slot()[
        SAssignNew(ValidationBox,SVerticalBox)]];
}

TSharedRef<SDockTab> FGOAPDomainEditorToolkit::SpawnSimulationTab(const FSpawnTabArgs&)
{
    return SNew(SDockTab)[SNew(SScrollBox)+SScrollBox::Slot()[
        SAssignNew(SimulationBox,SVerticalBox)]];
}

TSharedRef<SDockTab> FGOAPDomainEditorToolkit::SpawnDebuggerTab(const FSpawnTabArgs&)
{
    TSharedRef<SDockTab> Tab=SNew(SDockTab)[SNew(SScrollBox)+SScrollBox::Slot()[
        SAssignNew(DebuggerBox,SVerticalBox)]];
    Tab->RegisterActiveTimer(0.25f,FWidgetActiveTimerDelegate::CreateSP(this,
        &FGOAPDomainEditorToolkit::RefreshDebugger));
    return Tab;
}

void FGOAPDomainEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{ Collector.AddReferencedObject(Domain); }

void FGOAPDomainEditorToolkit::NotifyPostChange(const FPropertyChangedEvent&,
    FProperty*)
{ RefreshAll(); }

void FGOAPDomainEditorToolkit::RefreshAll()
{
    HellRunGOAPEditorGraph::Rebuild(*Domain);
    if(GraphEditor)GraphEditor->NotifyGraphChanged();
    ValidateDomain();
}

void FGOAPDomainEditorToolkit::ValidateDomain()
{
    Domain->Validate(ValidationErrors,ValidationWarnings);
    if(!ValidationBox)return;
    ValidationBox->ClearChildren();
    ValidationBox->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock)
        .Text(FText::Format(FText::FromString("{0} errors, {1} warnings"),
            ValidationErrors.Num(),ValidationWarnings.Num()))];
    for(const FText& Error:ValidationErrors)ValidationBox->AddSlot().AutoHeight().Padding(6,2)[
        SNew(STextBlock).ColorAndOpacity(FLinearColor(1,0.18f,0.12f)).Text(Error)];
    for(const FText& Warning:ValidationWarnings)ValidationBox->AddSlot().AutoHeight().Padding(6,2)[
        SNew(STextBlock).ColorAndOpacity(FLinearColor(1,0.65f,0.08f)).Text(Warning)];
}

void FGOAPDomainEditorToolkit::RunSimulations()
{
    SimulationResults.Reset();
    for(const FGOAPSimulationCase& TestCase:Domain->SimulationCases)
        SimulationResults.Add(FGOAPSimulator::Run(*Domain,TestCase));
    if(!SimulationBox)return;
    SimulationBox->ClearChildren();
    if(SimulationResults.IsEmpty())SimulationBox->AddSlot().AutoHeight().Padding(6)[
        SNew(STextBlock).Text(FText::FromString("No simulation cases. Add them in Domain Details."))];
    for(const FGOAPSimulationResult& Result:SimulationResults)
    {
        const FString Plan=FString::JoinBy(Result.Plan,TEXT(" -> "),[](FName N){return N.ToString();});
        SimulationBox->AddSlot().AutoHeight().Padding(6)[SNew(SBorder)[SNew(STextBlock)
            .ColorAndOpacity(Result.bPassed?FLinearColor(0.25f,1,0.35f):FLinearColor(1,0.2f,0.12f))
            .Text(FText::FromString(FString::Printf(TEXT("%s: %s | Goal %s | %s | cost %.2f / %d nodes\n%s"),
                Result.bPassed?TEXT("PASS"):TEXT("FAIL"),*Result.CaseName.ToString(),
                *Result.SelectedGoal.ToString(),*Plan,Result.Cost,Result.ExpandedNodes,*Result.Message)))]];
    }
}

void FGOAPDomainEditorToolkit::AddFact()
{
    Domain->Modify(); FGOAPFactDefinition& Fact=Domain->Facts.AddDefaulted_GetRef();
    Fact.EnsureId(); Fact.Name=*FString::Printf(TEXT("NewFact_%d"),Domain->Facts.Num());
    Domain->PostEditChange(); DetailsView->SetObject(Domain,true); RefreshAll();
}

void FGOAPDomainEditorToolkit::AddAction()
{
    Domain->Modify(); UGOAPActionDefinition* Action=NewObject<UGOAPActionDefinition>(
        Domain,NAME_None,RF_Transactional); Action->EnsureId();
    Action->Name=*FString::Printf(TEXT("NewAction_%d"),Domain->Actions.Num()+1);
    Domain->Actions.Add(Action); Domain->PostEditChange();
    DetailsView->SetObject(Action,true); RefreshAll();
}

void FGOAPDomainEditorToolkit::AddGoal()
{
    Domain->Modify(); UGOAPGoalDefinition* Goal=NewObject<UGOAPGoalDefinition>(
        Domain,NAME_None,RF_Transactional); Goal->EnsureId();
    Goal->Name=*FString::Printf(TEXT("NewGoal_%d"),Domain->Goals.Num()+1);
    Domain->Goals.Add(Goal); Domain->PostEditChange();
    DetailsView->SetObject(Goal,true); RefreshAll();
}

void FGOAPDomainEditorToolkit::DeleteSelectedNodes()
{
    if(!GraphEditor)return;
    const FGraphPanelSelectionSet Selected=GraphEditor->GetSelectedNodes();
    if(Selected.IsEmpty())return;
    Domain->Modify();
    for(UObject* Object:Selected)
        if(const UGOAPEditorGraphNode* Node=Cast<UGOAPEditorGraphNode>(Object))
        {
            Domain->Actions.RemoveAll([&](const UGOAPActionDefinition* A){return A&&A->Id==Node->DefinitionId;});
            Domain->Goals.RemoveAll([&](const UGOAPGoalDefinition* G){return G&&G->Id==Node->DefinitionId;});
        }
    Domain->PostEditChange(); DetailsView->SetObject(Domain,true); RefreshAll();
}

void FGOAPDomainEditorToolkit::HandleSelectionChanged(const TSet<UObject*>& Selection)
{
    if(Selection.Num()==1)
        if(const UGOAPEditorGraphNode* Node=Cast<UGOAPEditorGraphNode>(*Selection.CreateConstIterator()))
        {DetailsView->SetObject(Node->ResolveDefinition(),true);return;}
    DetailsView->SetObject(Domain,true);
}

EActiveTimerReturnType FGOAPDomainEditorToolkit::RefreshDebugger(double,float)
{
    if(!DebuggerBox)return EActiveTimerReturnType::Continue;
    DebuggerBox->ClearChildren(); int32 Count=0;
    if(GEditor)
        for(const FWorldContext& Context:GEditor->GetWorldContexts())
            if(UWorld* World=Context.World())
                if(UGOAPWorldStateSubsystem* State=World->GetSubsystem<UGOAPWorldStateSubsystem>())
                {
                    TArray<UGOAPBrainComponent*> Brains; State->GetBrains(Brains);
                    for(UGOAPBrainComponent* Brain:Brains)
                    {
                        if(!Brain||Brain->Domain!=Domain)continue; ++Count;
                        const FGOAPBrainDebugSnapshot D=Brain->GetDebugSnapshot();
                        const FString Plan=FString::JoinBy(D.RemainingPlan,TEXT(" -> "),[](FName N){return N.ToString();});
                        FString Facts;
                        for(const FGOAPFactDebugEntry& Fact:D.Facts)
                            Facts+=FString::Printf(TEXT("\n  %s = %s   [%s / %s / %.0f%%]"),
                                *Fact.Name.ToString(),*Fact.Value.ToString(),
                                *UEnum::GetValueAsString(Fact.Scope),
                                Fact.bUsingDefault?TEXT("default"):*Fact.Source.ToString(),
                                Fact.Confidence*100.0f);
                        FString Scores;
                        for(const FGOAPGoalScore& Score:D.GoalScores)
                            Scores+=FString::Printf(TEXT("\n  %s: %.2f%s"),
                                *Score.GoalName.ToString(),Score.Score,
                                Score.bEligible?TEXT(""):TEXT(" (ineligible)"));
                        DebuggerBox->AddSlot().AutoHeight().Padding(6)[SNew(SBorder)[SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("%s\nGoal: %s   Action: %s (%s)\nPlan: %s\nReplan: %s\nGoal utility:%s\nWorld state:%s"),
                                *GetNameSafe(Brain->GetOwner()),*D.ActiveGoal.ToString(),*D.ActiveAction.ToString(),
                                *UEnum::GetValueAsString(D.ActionStatus),*Plan,*D.LastReplanReason,
                                *Scores,*Facts)))]];
                    }
                }
    if(Count==0)DebuggerBox->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock)
        .Text(FText::FromString("No live PIE agents are using this domain."))];
    return EActiveTimerReturnType::Continue;
}
