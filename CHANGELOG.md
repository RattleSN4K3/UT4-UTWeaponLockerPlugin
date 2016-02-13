Version 1.0
--------------------------------------
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
--------------------------------------
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
--------------------------------------
- Basic weapon locker
- Ported meshes, materials, effects
- Working network code
- Per-player weapon respawn
- Map Error check to show undesired weapons in a locker
- State system
- Extended Blueprint support
