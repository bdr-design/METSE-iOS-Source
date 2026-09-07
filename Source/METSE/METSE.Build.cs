using UnrealBuildTool;
public class METSE : ModuleRules
{
    public METSE(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "AIModule", "NavigationSystem", "PhysicsCore", "HTTP", "Json", "JsonUtilities" });
    }
}
