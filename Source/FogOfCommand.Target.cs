using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class FogOfCommandTarget : TargetRules
{
	public FogOfCommandTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("FogOfCommand");

		WindowsPlatform.Compiler = WindowsCompiler.VisualStudio2022;
		WindowsPlatform.CompilerVersion = "14.44.35229";
		WindowsPlatform.ToolchainVersion = "14.44.35229";
		WindowsPlatform.WindowsSdkVersion = "10.0.22621.0";
	}
}
