=============================================================================
 WeaponLocker for UT4
 Version: 1.0
 ----------------------------------------------------------------------------
 Weapon Locker is a special pickup base to hold a set of weapons.
=============================================================================
 Game: Unreal Tournament (UE4)
 Size: ~ 2.18 - 4.52 MB
 Version: 1.0
 Compatibility: Build 2926870 (3/31/2016)
 Type: C++
 Credits: Epic Games
-----------------------------------------------------------------------------
 Coded by RattleSN4K3
 Mail: RattleSN4K3@gmail.com
=============================================================================

Description:
--------------------------------------------------------
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


Features:
--------------------------------------------------------
- Support for dynamically spawning Locker
- Support for round-based locker
- Enable and disable Locker on runtime dynamically
- Add/Replace weapons on runtime (e.g. in Level Blueprint)
- Customizable state system
- Bind-able event for level scripting (pickup status)
- Mutator to respawn with nearby locker weapons
- Map Error check to show undesired weapons in a locker



Installation:
--------------------------------------------------------
- Extract the zip file (copy the content) to your Unreal Tournament Server folder:  
  Windows: WindowsServer\
  Linux: LinuxServer/
  Windows: MacServer/
- Done


Manually:
- Copy files/folders from "Binaries\" to
  \UnrealTournament\Binaries
- Copy files/folders from "Content\" to
  \UnrealTournament\Content



Usage:
--------------------------------------------------------
- Add the following line to the command line arguments
  (or your shortcut, server command line, ...):

  ?map=ExampleMapWeaponLocker?game=UnrealTournament.UTDMGameMode

- Start the game


Changelog:
--------------------------------------------------------
v1.0
- Added: Example map with Lockers (incl. scripting)
- Fixed: Spawn protection not remove when picking up weapons from locker
- Fixed: Error message for invalid states always shown
- Fixed: Crash when removing weapons from a 6+ weapon locker
- Fixed: Duplicate weapon meshes after undo/redo
- Changed: Proper positions and rotations for visible weapon meshes
- Changed: Make use of Reset interface and properly reset on Level reset
- Changed: AllowPickupBy now Blueprint-callable
- Changed: Optimized replication of ReplacementWeapons array
- Changed: Locker mesh scale increased
- Added: Touch re-check for spawning lockers
- Added: Mutator 'Start with Locker Weapons'
- Added: Blueprint support for changing respawn time dynamically
- Added: Implementable event when weapons are drawn/hidden
- Added: Level script event (delegate) for pickup status
- Added: Implementable editor event when weapons meshes are created
- Added: Dynamically check if best lockers are really the best

Pre Release 2
- New: Locker ammo per weapon
- New: Clear locker pickup conditions on level reset
- New: Editor meshes preview
- Fixed: Crash with constructed states
- Fixed: Pickup announcement with stacked inventory items
- Fixed: Replication with runtime spawned lockers
- Fixed: Pickup sound replication to clients
- Fixed: Locker collision
- Changed: StateInfo properly initialized
- Removed: InitialState and GlobalState from default properties

Pre Release 1 - Initial public release
- Basic weapon locker
- Ported meshes, materials, effects
- Working network code
- Per-player weapon respawn
- Map Error check to show undesired weapons in a locker
- State system
- Extended Blueprint support

