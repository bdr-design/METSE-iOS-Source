using UnrealBuildTool;
public class METSEEditorTarget : TargetRules { public METSEEditorTarget(TargetInfo Target) : base(Target) { Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.V5; IncludeOrderVersion = EngineIncludeOrderVersion.Latest; ExtraModuleNames.Add("METSE"); } }
