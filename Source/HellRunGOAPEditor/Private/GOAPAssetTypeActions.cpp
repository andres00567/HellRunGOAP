#include "GOAPAssetTypeActions.h"
#include "GOAPDomain.h"
#include "GOAPDomainEditorToolkit.h"

FText FGOAPAssetTypeActions::GetName() const
{ return NSLOCTEXT("HellRunGOAP","GOAPDomainAsset","GOAP Domain"); }

FColor FGOAPAssetTypeActions::GetTypeColor() const
{ return FColor(47,190,170); }

UClass* FGOAPAssetTypeActions::GetSupportedClass() const
{ return UGOAPDomain::StaticClass(); }

uint32 FGOAPAssetTypeActions::GetCategories()
{ return EAssetTypeCategories::Gameplay; }

void FGOAPAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects,
    TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
    for(UObject* Object:InObjects)
        if(UGOAPDomain* Domain=Cast<UGOAPDomain>(Object))
        {
            TSharedRef<FGOAPDomainEditorToolkit> Editor=
                MakeShared<FGOAPDomainEditorToolkit>();
            Editor->Initialize(Domain,EditWithinLevelEditor);
        }
}
