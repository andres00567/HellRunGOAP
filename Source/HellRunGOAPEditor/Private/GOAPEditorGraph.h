#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "GOAPEditorGraph.generated.h"

class UGOAPDomain;

UENUM()
enum class EGOAPEditorNodeKind : uint8
{
    Action,
    Goal,
};

UCLASS()
class UGOAPEditorGraphNode final : public UEdGraphNode
{
    GENERATED_BODY()
public:
    UPROPERTY()
    EGOAPEditorNodeKind Kind=EGOAPEditorNodeKind::Action;

    UPROPERTY()
    FGuid DefinitionId;

    virtual void AllocateDefaultPins() override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FLinearColor GetNodeTitleColor() const override;
    virtual bool CanUserDeleteNode() const override { return true; }
    virtual bool CanDuplicateNode() const override { return false; }

    UObject* ResolveDefinition() const;
};

UCLASS()
class UGOAPEditorGraphSchema final : public UEdGraphSchema
{
    GENERATED_BODY()
public:
    virtual const FPinConnectionResponse CanCreateConnection(
        const UEdGraphPin* A,const UEdGraphPin* B) const override;
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& Builder) const override;
};

namespace HellRunGOAPEditorGraph
{
    UEdGraph* EnsureGraph(UGOAPDomain& Domain);
    void Rebuild(UGOAPDomain& Domain);
}
