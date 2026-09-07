using UnrealBuildTool;
public class METSETarget : TargetRules { public METSETarget(TargetInfo Target) : base(Target) { Type = TargetType.Game; DefaultBuildSettings = BuildSettingsVersion.V5; IncludeOrderVersion = EngineIncludeOrderVersion.Latest; ExtraModuleNames.Add("METSE"); } }
