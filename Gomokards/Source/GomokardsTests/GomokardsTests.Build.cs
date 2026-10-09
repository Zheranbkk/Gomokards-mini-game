using UnrealBuildTool;
public class GomokardsTests : ModuleRules
{
    public GomokardsTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "UnrealEd", "Gomokards" });
    }
}
