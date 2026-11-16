using UnrealBuildTool;

public class FateshapedTarget : TargetRules
{
    public FateshapedTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("Fateshaped");
    }
}
