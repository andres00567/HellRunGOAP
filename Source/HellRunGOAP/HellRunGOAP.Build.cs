using UnrealBuildTool;

public class HellRunGOAP : ModuleRules
{
    public HellRunGOAP(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GameplayTags"
        });
    }
}
