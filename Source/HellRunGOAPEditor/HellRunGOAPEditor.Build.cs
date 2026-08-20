using UnrealBuildTool;

public class HellRunGOAPEditor : ModuleRules
{
    public HellRunGOAPEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "HellRunGOAP",
            "UnrealEd", "AssetTools", "GraphEditor", "PropertyEditor",
            "Slate", "SlateCore", "ToolMenus", "EditorFramework",
            "ApplicationCore", "InputCore"
        });
    }
}
