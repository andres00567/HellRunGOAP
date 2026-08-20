#include "GOAPEditorGraph.h"
#include "GOAPDomain.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"

namespace
{
    UGOAPDomain* FindDomain(const UObject* Object)
    {
        for(const UObject* Outer=Object;Outer;Outer=Outer->GetOuter())
            if(UGOAPDomain* Domain=const_cast<UGOAPDomain*>(Cast<UGOAPDomain>(Outer)))
                return Domain;
        return nullptr;
    }

    FString FactLabel(const UGOAPDomain& Domain,const FGuid& FactId,
        const FGOAPValue& Value,const FName FactName=NAME_None)
    {
        const FGOAPFactDefinition* Fact=!FactName.IsNone()
            ?Domain.FindFact(FactName):Domain.FindFact(FactId);
        return FString::Printf(TEXT("%s = %s"),
            Fact?*Fact->Name.ToString():TEXT("Missing Fact"),*Value.ToString());
    }

    bool EffectMaySatisfy(const FGOAPEffect& Effect,
        const FGOAPCondition& Condition)
    {
        if(Effect.FactId!=Condition.FactId) return false;
        if(Effect.Operation==EGOAPEffectOperation::Clear)
            return Condition.Comparison==EGOAPComparison::IsNotSet;
        if(Condition.Comparison==EGOAPComparison::IsSet) return true;
        if(Effect.Operation!=EGOAPEffectOperation::Set) return true;
        FGOAPCompiledDomain Fake;
        Fake.FactIndices.Add(Condition.FactId,0);
        FGOAPPlanningState State; State.Values.Add(Effect.Value);
        return GOAPEvaluateCondition(Condition,Fake,State);
    }

    UEdGraphPin* AddPin(UGOAPEditorGraphNode& Node,
        const EEdGraphPinDirection Direction,const FString& Name,
        const FString& Friendly)
    {
        UEdGraphPin* Pin=Node.CreatePin(Direction,TEXT("GOAP"),FName(Name));
        Pin->PinFriendlyName=FText::FromString(Friendly);
        return Pin;
    }
}

UObject* UGOAPEditorGraphNode::ResolveDefinition() const
{
    const UGOAPDomain* Domain=FindDomain(this);
    if(!Domain)return nullptr;
    if(Kind==EGOAPEditorNodeKind::Action)
        for(UGOAPActionDefinition* Action:Domain->Actions)
            if(Action&&Action->Id==DefinitionId)return Action;
    for(UGOAPGoalDefinition* Goal:Domain->Goals)
        if(Goal&&Goal->Id==DefinitionId)return Goal;
    return nullptr;
}

void UGOAPEditorGraphNode::AllocateDefaultPins()
{
    UGOAPDomain* Domain=FindDomain(this);
    if(!Domain)return;
    if(UGOAPActionDefinition* Action=Cast<UGOAPActionDefinition>(ResolveDefinition()))
    {
        for(int32 I=0;I<Action->Preconditions.Num();++I)
            AddPin(*this,EGPD_Input,FString::Printf(TEXT("P_%d"),I),
                FactLabel(*Domain,Action->Preconditions[I].FactId,
                    Action->Preconditions[I].Value,Action->Preconditions[I].FactName));
        for(int32 I=0;I<Action->Effects.Num();++I)
            AddPin(*this,EGPD_Output,FString::Printf(TEXT("E_%d"),I),
                FactLabel(*Domain,Action->Effects[I].FactId,
                    Action->Effects[I].Value,Action->Effects[I].FactName));
    }
    else if(UGOAPGoalDefinition* Goal=Cast<UGOAPGoalDefinition>(ResolveDefinition()))
        for(int32 I=0;I<Goal->DesiredState.Num();++I)
            AddPin(*this,EGPD_Input,FString::Printf(TEXT("D_%d"),I),
                FactLabel(*Domain,Goal->DesiredState[I].FactId,
                    Goal->DesiredState[I].Value,Goal->DesiredState[I].FactName));
}

FText UGOAPEditorGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    if(const UGOAPActionDefinition* Action=Cast<UGOAPActionDefinition>(ResolveDefinition()))
        return FText::Format(NSLOCTEXT("HellRunGOAP","ActionTitle","ACTION  {0}"),
            FText::FromName(Action->Name));
    if(const UGOAPGoalDefinition* Goal=Cast<UGOAPGoalDefinition>(ResolveDefinition()))
        return FText::Format(NSLOCTEXT("HellRunGOAP","GoalTitle","GOAL  {0}"),
            FText::FromName(Goal->Name));
    return NSLOCTEXT("HellRunGOAP","MissingNode","Missing Definition");
}

FLinearColor UGOAPEditorGraphNode::GetNodeTitleColor() const
{
    return Kind==EGOAPEditorNodeKind::Goal
        ? FLinearColor(0.70f,0.22f,0.12f) : FLinearColor(0.05f,0.42f,0.38f);
}

const FPinConnectionResponse UGOAPEditorGraphSchema::CanCreateConnection(
    const UEdGraphPin* A,const UEdGraphPin* B) const
{
    return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
        NSLOCTEXT("HellRunGOAP","DerivedEdges",
            "Planning edges are derived from typed preconditions and effects."));
}

void UGOAPEditorGraphSchema::GetGraphContextActions(
    FGraphContextMenuBuilder& Builder) const
{
    // Definitions are added with the editor toolbar so their transactional
    // instanced objects and stable ids are always created together.
}

UEdGraph* HellRunGOAPEditorGraph::EnsureGraph(UGOAPDomain& Domain)
{
    if(!Domain.EditorGraph)
    {
        Domain.Modify();
        Domain.EditorGraph=NewObject<UEdGraph>(&Domain,TEXT("GOAPGraph"),RF_Transactional);
        Domain.EditorGraph->Schema=UGOAPEditorGraphSchema::StaticClass();
    }
    return Domain.EditorGraph;
}

void HellRunGOAPEditorGraph::Rebuild(UGOAPDomain& Domain)
{
    UEdGraph* Graph=EnsureGraph(Domain);
    TMap<FGuid,FVector2D> PreviousPositions;
    for(UEdGraphNode* Node:Graph->Nodes)
        if(const UGOAPEditorGraphNode* GOAPNode=Cast<UGOAPEditorGraphNode>(Node))
            PreviousPositions.Add(GOAPNode->DefinitionId,
                FVector2D(GOAPNode->NodePosX,GOAPNode->NodePosY));
    Graph->Modify(); Graph->Nodes.Reset();
    TMap<FGuid,UGOAPEditorGraphNode*> Nodes;
    for(int32 I=0;I<Domain.Actions.Num();++I)
    {
        UGOAPActionDefinition* Action=Domain.Actions[I]; if(!Action)continue;
        UGOAPEditorGraphNode* Node=NewObject<UGOAPEditorGraphNode>(Graph);
        Node->Kind=EGOAPEditorNodeKind::Action; Node->DefinitionId=Action->Id;
        if(const FVector2D* P=PreviousPositions.Find(Action->Id))
        {Node->NodePosX=P->X;Node->NodePosY=P->Y;}
        else{Node->NodePosX=(I%3)*340;Node->NodePosY=(I/3)*240;}
        Graph->AddNode(Node,false,false); Node->CreateNewGuid();
        Node->AllocateDefaultPins(); Nodes.Add(Action->Id,Node);
    }
    for(int32 I=0;I<Domain.Goals.Num();++I)
    {
        UGOAPGoalDefinition* Goal=Domain.Goals[I]; if(!Goal)continue;
        UGOAPEditorGraphNode* Node=NewObject<UGOAPEditorGraphNode>(Graph);
        Node->Kind=EGOAPEditorNodeKind::Goal; Node->DefinitionId=Goal->Id;
        if(const FVector2D* P=PreviousPositions.Find(Goal->Id))
        {Node->NodePosX=P->X;Node->NodePosY=P->Y;}
        else{Node->NodePosX=1100;Node->NodePosY=I*280;}
        Graph->AddNode(Node,false,false); Node->CreateNewGuid();
        Node->AllocateDefaultPins(); Nodes.Add(Goal->Id,Node);
    }
    for(UGOAPActionDefinition* Source:Domain.Actions)
    {
        UGOAPEditorGraphNode* SourceNode=Source?Nodes.FindRef(Source->Id):nullptr;
        if(!SourceNode)continue;
        for(int32 E=0;E<Source->Effects.Num();++E)
        {
            UEdGraphPin* Output=SourceNode->FindPin(FName(FString::Printf(TEXT("E_%d"),E)));
            if(!Output)continue;
            for(UGOAPActionDefinition* Target:Domain.Actions)
                if(Target&&Target!=Source)
                    for(int32 P=0;P<Target->Preconditions.Num();++P)
                        if(EffectMaySatisfy(Source->Effects[E],Target->Preconditions[P]))
                            Output->MakeLinkTo(Nodes.FindRef(Target->Id)->FindPin(
                                FName(FString::Printf(TEXT("P_%d"),P))));
            for(UGOAPGoalDefinition* Goal:Domain.Goals)
                if(Goal)
                    for(int32 D=0;D<Goal->DesiredState.Num();++D)
                        if(EffectMaySatisfy(Source->Effects[E],Goal->DesiredState[D]))
                            Output->MakeLinkTo(Nodes.FindRef(Goal->Id)->FindPin(
                                FName(FString::Printf(TEXT("D_%d"),D))));
        }
    }
    Graph->NotifyGraphChanged();
}
