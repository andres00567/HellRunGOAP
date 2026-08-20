#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "GOAPAssetTypeActions.h"

class FHellRunGOAPEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        IAssetTools& AssetTools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            "AssetTools").Get();
        AssetActions=MakeShared<FGOAPAssetTypeActions>();
        AssetTools.RegisterAssetTypeActions(AssetActions.ToSharedRef());
    }

    virtual void ShutdownModule() override
    {
        if(AssetActions.IsValid()&&FModuleManager::Get().IsModuleLoaded("AssetTools"))
            FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools")
                .Get().UnregisterAssetTypeActions(AssetActions.ToSharedRef());
        AssetActions.Reset();
    }

private:
    TSharedPtr<IAssetTypeActions> AssetActions;
};

IMPLEMENT_MODULE(FHellRunGOAPEditorModule,HellRunGOAPEditor)
