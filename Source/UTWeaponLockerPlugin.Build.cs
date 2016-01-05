using System.IO;

namespace UnrealBuildTool.Rules
{
	public class UTWeaponLockerPlugin : ModuleRules
	{
		public UTWeaponLockerPlugin(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealTournament",
					"InputCore",
					"SlateCore",
				}
			);
		}
	}
}