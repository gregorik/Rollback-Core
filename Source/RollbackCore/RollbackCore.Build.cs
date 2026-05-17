// Copyright (c) 2026 GregOrigin. MIT Licensed - see LICENSE for details.

using UnrealBuildTool;

public class RollbackCore : ModuleRules
{
	public RollbackCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DeveloperSettings",
				"InputCore",
				"Json",
				"Sockets",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Networking",
				"Projects",
			}
			);
	}
}
