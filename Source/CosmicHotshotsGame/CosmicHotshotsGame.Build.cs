using UnrealBuildTool;

public class CosmicHotshotsGame : ModuleRules
{
	public CosmicHotshotsGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets code include headers by module-relative path, e.g. "Vehicle/RaceVehicle.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Chaos", "PhysicsCore", "DeveloperSettings", "UMG", "Slate", "SlateCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
