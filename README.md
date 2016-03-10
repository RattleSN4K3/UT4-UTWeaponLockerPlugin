UTWeaponLockerPlugin
==========

## Description:
Weapon lockers contain certain weapons and ammo and refill themselves
individually for each player. A player can use a specific weapon locker only
every 30 seconds or after respawning.

Lockers are mainly used in vehicle gametypes, like Onslaught, Assault,Vehicle
Capture the Flag and Warfare, where players shouldn't have to worry too much
about getting weapons and ammo.


In UT2004, a weapon locker is a round stand with all contained weapons
attached to it. There's a light at the top, which will glow green if the
player can reload at the locker.

The weapons and the amount of extra ammo per weapon can be specified for
each locker by the mapper.


In UT3, a weapon locker has a solid base and top connected by an energy beam.
Available Weapons in the locker are only displayed when the player is close
enough and allowed to reload at that locker. The green energy cloud is also
visible from further away if the locker has ammo available for the player.

The available weapons can be specified for each locker by the mapper, the
ammo amount is hard-coded for each weapon.

## Features
- Support for dynamically spawning Locker
- Support for round-based locker
- Enable and disable Locker on runtime dynamically
- Add/Replace weapons on runtime (e.g. in Level Blueprint)
- Customizable state system
- Bind-able event for level scripting (pickup status)
- Mutator to respawn with nearby locker weapons
- Map Error check to show undesired weapons in a locker

# Compiling

This plugin is created with the code-base of the latest release of the _Unreal Tournament_ source code. In order to fully support using this plugin with the [Launcher versions of Unreal Tournament](https://www.unrealtournament.com/download), a specific commit/version/tag needs to be checked out. You can always find the used commit in the [CHANGELIST file](CHANGELIST.editor) which directs to the specific commit in the release branch of the UT repository.

Before the code can be compiled, the engine must be aware of the installed source files and the source files must be placed into the correct folder.

## Setup

For easy referencing, `%basedir%` would be the root folder of your local [GitHub repository of Unreal Tournament](https://github.com/EpicGames/UnrealTournament).

- Download the [latest source files](/../../archive/master.zip)
- Unpack the file into the *Plugins* folder located at:  
  `%basedir%\UnrealTournament\Plugins\UTWeaponLockerPlugin`
- Generate the project files with the specific batch file:  
  - Windows: `%basedir%\GenerateProjectFiles.bat`
  - Linux: `%basedir%\GenerateProjectFiles.command`
  - Mac: `%basedir%\GenerateProjectFiles.sh`

## Make

The source files should be compiled by the Unreal Build System. Each specific platform and version has to be compiled with the specific command line arguments.

Solution configuration list for each game version:
- Editor: Development Editor
- Client: Shipping
- Server: Shipping Server

Compile the solution with either configuration for the desired version and target platform.

## Testing


With the current build, _Unreal Tournament Editor_ has the same network version as the client build. You don't need to use a workaround.

Otherwise, you need to compile the source code additionally with a different code-base. The compiled version for the editor cannot be used with the client build. You need to compile the source code for each specific version or use the workaround in order o run the compiled editor version with the client build.

### Editor

If you are using the specific changelist, as specified in the [CHANGELIST.editor](CHANGELIST.editor) file, you can use the compiled source with the editor by just copying the binary file and the content into `Plugins` of the Launcher editor (_Editor configuration_ required): `UnrealTournamentEditor\UnrealTournament\Plugins`

### Client/Server  

0. \[SKIP IF CHANGELISTS [[1](CHANGELIST.editor)][[2](CHANGELIST.client)] ARE THE SAME\]. Hex-modify the specific binary file with the changelist specified in [CHANGELIST.editor](CHANGELIST.editor) to the changelist specified in [CHANGELIST.client](CHANGELIST.client)
0. Browse to `Engine\Build\BatchFiles`
0. Run the build automation tool with:  
     - Windows (64-bit): `RunUAT.bat MakeUTDLC -DLCName=UTWeaponLockerPlugin -platform=Win64 -version=3008043`
     - Windows (32-bit): `RunUAT.bat MakeUTDLC -DLCName=UTWeaponLockerPlugin -platform=Win32 -version=3008043`
     - Linux: `RunUAT.bat MakeUTDLC -DLCName=UTWeaponLockerPlugin -platform=Linux -version=3008043`
     - Mac: `RunUAT.bat MakeUTDLC -DLCName=UTWeaponLockerPlugin -platform=Mac -version=3008043`
0. Wait until cooking process is done (could take a while)
0. After cooking is done, the cooked and staged content package of this plugin can be found under:
   - Windows: `UnrealTournament\Saved\StagedBuilds\UTWeaponLockerPlugin\WindowsNoEditor\UnrealTournament\Content\Paks`
   - Linux: `UnrealTournament\Saved\StagedBuilds\UTWeaponLockerPlugin\LinuxNoEditor\UnrealTournament\Content\Paks`
   - Mac: `UnrealTournament\Saved\StagedBuilds\UTWeaponLockerPlugin\MacNoEditor\UnrealTournament\Content\Paks`
0. Copy the files the the launcher plugin folder. %plugindir% would be `UnrealTournamentDev\UnrealTournament\Plugins\UTWeaponLockerPlugin`.
   - Copy the plugin descriptor file `UTWeaponLockerPlugin.uplugin` to `%plugindir%\`
   - Copy the binary files to `%plugindir%\Binaries\`
   - Copy the cooked PAK file to `%plugindir%\Content\Paks\`

## License

Licensed under the terms of the latest version of the **Unreal® Engine End User License Agreement**, obtainable at [unrealengine.com/eula](//unrealengine.com/eula).

A copy of the license text, as obtained on 2016-01-04, is included in the [UE4EULA.pod](UE4EULA.pod) file (for reference purposes only).
