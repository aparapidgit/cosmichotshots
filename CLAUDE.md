# CLAUDE.md: Cosmic Hotshots (arcade hovercar racing, Unreal Engine 5.7)

## Project summary
Arcade hovercar racing game for PC (Windows .exe), gamepad-first, keyboard supported.
- Modes: Single Race (vs AI and/or local split-screen 2-4), Adventure Mode (singleplayer hub + progression), online rooms LATER (not in first version).
- Grid: up to 8 vehicles per race. Each slot is a human or an AI.
- 8 playable characters, each with a different hovercar and different PASSIVE driving stats.
- Solo developer. Scope discipline matters more than features.
- The developer writes in Serbian/Montenegrin or English. Reply in the language the developer is currently using; write all code, comments and identifiers in English.

## Tech
- Unreal Engine: 5.7. Do not use Early Access or Preview builds. Do not use APIs/features that only exist in 5.8.
- Editor integration: community MCP plugins that support 5.7 (e.g. UnrealMCP, ClaudeUnreal). Epic's official MCP plugin is a 5.8 feature, not available here. Features marked Experimental must NOT be used in production code.
- C++ for core gameplay; Blueprints only for tuning, UI wiring, quick prototypes.
- Input: Enhanced Input, gamepad first (Xbox/PlayStation), fully remappable.
- UI: UMG with full gamepad focus navigation.
- VFX: Niagara. Audio: MetaSounds or FMOD (decide later).
- Online (future): Steam/EOS listen server. Not now.

## Vehicles are HOVERCARS (no wheels)
Custom arcade hover physics on ONE rigid body. Not Chaos Vehicle.
- Hover: 4 (or more) downward traces at the corners. Each applies a spring-damper force to keep ride height. Align body to ground normal smoothly.
- Thrust: forward force from throttle; reverse/brake force.
- Steering: yaw torque/rate from steering input, reduced at very high speed for stability.
- Grip/drift: lateral velocity damping. Low damping = slidey hover feel. Drift button reduces grip temporarily and builds boost charge.
- Boost: short forward impulse + FOV/VFX; fed by drifting and pickups.
- Air control: small pitch/roll correction in the air, extra gravity so it feels snappy, not floaty.
- Everything tunable in data assets, never hardcoded.

## Characters and passive abilities (race-state driven)
Game: Cosmic Hotshots. Precise steering with mild drifting. Passives adapt the car in real time and each should encourage a recognisable driving style. All numbers (bonuses, durations, thresholds) are PLACEHOLDERS to be tuned through playtesting. Everything must live in data assets.

### Agreed passives
| Character | Colour | Style | Passive |
|---|---|---|---|
| Spark | Blue | Comeback from behind | While in LAST place: slightly higher max speed. Bonus ends when no longer last. |
| Sulgwen | Purple | Chain overtaking | First overtake gives a short bonus to max speed AND acceleration. Each further overtake before expiry strengthens both bonuses and refreshes duration. When time runs out the bonuses decay gradually to base values. |

Sulgwen tuning to determine by testing: bonus duration, increase per overtake, max total bonus, and when re-overtaking the SAME rival counts again (per-rival cooldown).

### Proposed passives (to be validated by playtesting)
| Character | Colour | Style | Passive |
|---|---|---|---|
| Freya | Red | Aggressive overtaking | After a CLOSE overtake, the overtaken car briefly loses a little speed. |
| Killjoke | Green | Chasing the runaway leader | When the leader is far ahead of 2nd place, Killjoke gets a speed bonus until the gap shrinks. |
| Glick | Yellow | Fighting for space | In side contact, pushes rivals away more easily and loses less speed. |
| Wilqis | White | Catching the next rival | Gets a mild speed bonus when the gap to the nearest car AHEAD is large enough. |

### Decided rules for threshold passives
- **Killjoke:** the leader (1st place) gets NO bonus from this passive. When Killjoke is 2nd or lower, the bonus depends on the gap between the leader and 2nd place, with hysteresis. (Behaviour for 2nd place is a default and can be tuned.)
- **Wilqis:** when in 1st place there is no car ahead, so NO bonus. Otherwise the bonus depends on the gap to the nearest car directly ahead.
- **Spark:** bonus only while in last place.

### Slots 7 and 8 (APPROVED by developer, values still to be tuned)
Both passives below are approved as design direction. Implement them together with the other six in Phase 2 (still behind the per-character toggle). Names are not final; use working names "Slot7" and "Slot8" in code/data.
| Slot | Colour | Style | Passive |
|---|---|---|---|
| 7 | Orange | Drift master / clean line | Consecutive well-executed drifts without any collision give a growing small max-speed bonus. A wall or vehicle hit resets the chain. Rewards skill, independent of race position. |
| 8 | Pink or cyan | Defending the lead | While in the top 3, recovers faster from hits and loses less speed. Keep it mild because most other passives already help trailing cars. |
Possible future characters: slipstream (benefit from driving behind another car) or an item-box specialist (only after power-ups exist).

### Testing switches
- Global console variable / dev setting that disables ALL passives, to test base driving feel and AI baseline.
- Per-character toggle (`bPassiveEnabled` on the passive definition, or a `NullPassive` class) so any passive can be switched off without code changes.

Active abilities (ENERGY CORE) for the PC version are NOT designed yet. Do not invent them.

### Passive system architecture
- `UCharacterDefinition` (PrimaryDataAsset): name, portrait, hovercar mesh, colour, passive definition reference, optional small base-stat modifiers. Base tuning stays in `UVehicleTuningData`.
- `UPassiveBase` (UObject or component, one per vehicle) with subclasses per passive: `USparkPassive`, `USulgwenPassive`, `UFreyaPassive`, `UKilljokePassive`, `UGlickPassive`, `UWilqisPassive`. Tunables come from a per-passive data asset.
- **Modifier/status-effect stack on the vehicle (shared system):** `UVehicleModifierComponent` holds modifiers with: source id, stat (MaxSpeed, Acceleration, ...), additive/multiplicative value, duration, fade-in and fade-out curves. Used by passives, item power-ups and later ENERGY CORE abilities. Needed for Sulgwen's gradual decay and for Freya affecting ANOTHER vehicle.
- **Race event source:** `ARaceManager` / GameState exposes events and queries that passives subscribe to: `OnOvertake(Overtaker, Overtaken, ClosenessMetric)`, current rank per vehicle, `GetGapToCarAhead(Vehicle)`, `GetLeaderToSecondGap()`, `OnSideContact(A, B, Info)`.
- Gaps are measured along the track spline (distance or time gap), never by straight-line distance.
- **Overtake detection:** a rank swap between two vehicles confirmed with hysteresis/debounce (must hold for a short time) so that jitter and collisions do not create fake overtakes. Track per-rival cooldown. A "close" overtake uses a lateral-distance and gap threshold.
- **Threshold passives (Killjoke, Wilqis, Spark):** use hysteresis (activation threshold higher than deactivation threshold) to avoid flickering on/off. Define explicitly what happens when the character is itself the leader, 2nd, or last (e.g. no bonus).
- **Glick** plugs into the impact system: multiplies outgoing lateral impulse and reduces his own speed loss on side contact only.
- Passives work identically for human and AI drivers.
- **Debug HUD (dev only):** show active passive state, active modifiers with remaining time, and measured gaps, so tuning is observable. Expose all tunables in data assets and console variables for live tweaking.
- Beware stacking: passives are mostly catch-up mechanics. Keep AI rubber banding mild and make sure passives + item boxes + rubber banding do not over-compensate. Add a way to disable rubber banding for testing.

## Collisions between vehicles (impact system)
- On vehicle-vehicle contact compute impulse from relative velocity along the contact normal.
- Scale outgoing impulse by attacker RamPower and incoming impulse by defender Stability, and account for Mass (heavier gets knocked back less).
- Stronger hit = short stun/wobble or spin-out, always short and readable (never frustrating).
- Impact feedback: hit-stop of a few frames, camera shake, controller rumble, sparks/shockwave VFX, impact sound scaled by force.
- Wall collisions: bounce with partial speed loss, scrape sparks.

## Speed and game feel (high priority)
- Speed lines (Niagara or post-process material), intensity scales with speed and boost.
- Dynamic FOV increase with speed is ADDED on top of the player's chosen base FOV (see Camera section), plus camera pull-back/lag, light camera shake, optional motion blur.
- Wind/engine audio scaling with speed, boost whoosh, controller haptics.
- Hover visuals: thruster glow, ground dust/particles, banking/tilt in turns.

## Camera (third person, player-adjustable FOV)
Default and only gameplay camera for the first versions is a **third-person chase camera** behind the hovercar (`ARaceCameraRig` or a camera component set owned by `ARaceVehicle`, driven by the local player's camera manager). Each local player has their own camera and their own camera settings.

### Behaviour
- Spring-arm style follow: camera sits behind and above the car, with positional lag and smooth rotation lag so speed and turns feel dynamic but never nauseating.
- Look direction blends between the car's facing and its velocity direction, so during drifts the camera shows where the car is actually going (look-ahead into the turn).
- Reacts to game feel: pulls back slightly and widens FOV under boost, light shake on impacts (scaled by impact force), small roll/bank in turns (subtle and optional).
- No clipping into walls or geometry (probe/collision test with smooth recovery). Camera must never end up inside the track hill or a wall.
- Look-back button (hold) shows behind the car. Optional camera reset button.
- Works identically for 1 to 4 local players (split-screen viewports) and is replay/spectator friendly later.

### FOV option (Options menu)
- Player choice between a **narrower/closer view** and a **wider view**, as a slider with named end points and at least 3 snap presets: **Close**, **Normal**, **Wide**. Placeholder values (tune by testing): Close about 80, Normal about 95, Wide about 110 degrees horizontal FOV, with a hard clamp (e.g. 70-120).
- The FOV option sets the BASE FOV. The camera distance/height is tied to it by a small curve (Close = camera nearer and lower, Wide = farther and higher), so the choice changes both FOV and framing in a sensible way. Keep the distance link as a separate data curve so it can be tuned or decoupled.
- Speed-based FOV (speed lines, boost) is an additive offset on top of the base FOV, with a maximum total clamp so Wide + boost never exceeds a sane limit. The speed effect strength should scale so Close and Wide both still feel fast.
- Live preview: while the player drags the slider in Options, the menu shows the new FOV immediately (3D preview of the car/track or the current race if opened from pause).
- Setting is stored per player profile / per local player (default for local player 1 saved in `UGameUserSettings` or a custom `USaveGame`; additional local players may use the default or their own saved value). Persist between sessions.
- In split-screen the viewport aspect ratio differs. Make sure the chosen horizontal FOV is applied sensibly per viewport (axis constraint or aspect-ratio correction) so split-screen does not look badly cropped or stretched. Ultrawide must also look correct.
- All values live in a data asset (`UCameraTuningData`): FOV presets, FOV min/max, distance/height curves, lag values, speed FOV offset, boost FOV offset, shake settings.

### Options menu (gamepad navigable)
- Camera: FOV slider (Close / Normal / Wide), optional "camera shake" intensity (Off/Low/Normal), optional "speed lines" intensity.
- All options must be reachable and changeable with the gamepad only, with a clear focus highlight.

## Race HUD
Clean, readable at speed, gamepad-era layout. Built in UMG, data-driven (icons come from `UCharacterDefinition`, item icons from `UItemDefinition`). In split-screen every viewport gets its own HUD for its local player, in a compact layout.

### Standings (character icons with place)
- A live standings list shows **each racer as a character icon with their place number (1st, 2nd, 3rd ... 8th)**. Order updates live; swaps animate smoothly (slide) so overtakes are noticeable.
- Local human players are highlighted (frame/colour, optional player number badge in local multiplayer).
- Optional per-entry marker showing that a racer currently holds an item (generic "has item" icon, not which item). Default: on. Setting in data so it can be turned off or changed to show the real item.
- Also show the local player's own big place number (e.g. "3rd") and lap counter (e.g. "Lap 2/3") near a screen corner.

### Mechanics indicators (local player)
- **Held power-up slot:** icon of the item the player keeps. Shuffle/roulette animation on pickup, and a distinct swap animation when a new box item replaces the unused one. Empty slot is visibly empty. Pressing use fires the item and clears the slot.
- **Active effect indicators:** small icons with remaining-time rings for active modifiers/status effects (e.g. boost, speed buff/debuff, hit/stun), fed by the modifier stack.
- **Character passive indicator:** shows when the player's passive is currently active and how strong it is (examples: Spark's comeback bonus on/off, Sulgwen's chain level and timer, Killjoke/Wilqis bonus active, Glick contact bonus, Freya close-overtake trigger).
- **Boost / drift charge** meter. **ENERGY CORE** bar is a reserved placeholder slot (hidden until Phase 3).
- All indicators read from existing systems (modifier stack, race events, item slot) through view-model data; the HUD never contains gameplay logic.
- HUD scale and visibility of optional elements go in Options (see next subsection).

### HUD element visibility in Options (independent of the hide button)
- Each mechanics indicator can be **shown or hidden individually in Options**, separately from the show/hide button. Toggles (all default ON): held power-up slot, active effect icons (boost/buffs/debuffs/stuns with timer rings), character passive indicator, boost/drift meter, ENERGY CORE bar (reserved; only listed in Options once Phase 3 exists). Also toggles for standings list, place/lap counter, and the per-entry "racer holds an item" marker.
- **Visibility rule:** an element is drawn only if `bHudVisible` (button toggle) is true AND that element's Options toggle is ON. The button never changes the Options toggles, and the Options toggles never change the button state. Hiding an element in Options hides it even when the HUD is shown with the button.
- Settings are **per local player** and saved with the player's settings/profile (same storage as the FOV option). Changes apply immediately, with live preview if Options is opened from the pause menu.
- Hiding an indicator only hides its display. The underlying mechanic (item slot, effects, passive, meters) keeps working normally. Exception to avoid confusion: if the item slot is hidden, the item pickup/use audio cue still plays.
- Implement as a data-driven list of HUD elements (`EHudElement` enum + settings struct), so adding a new indicator later means adding one enum value and one Options toggle entry.

### Show / hide HUD (button toggle)
- The player can **show or hide the HUD with a button press** at any time during a race (default binding: a spare gamepad button, e.g. D-pad Up or a stick click, plus a keyboard key; remappable in Options via Enhanced Input).
- Toggle is **per local player**: in split-screen each player hides/shows only their own HUD.
- Hiding removes all HUD elements (standings, place/lap, item slot, effect and passive indicators, meters). Gameplay is unaffected, and critical feedback that is part of the game feel (speed lines, camera effects, audio cues for item pickup/use) keeps working.
- Optional "minimal" state (data setting, off by default): hides everything except the held item slot. If implemented, the button cycles Full -> Hidden (-> Minimal).
- State is remembered between races and saved with the player's settings. The HUD is automatically shown again in menus, results and when the race ends.
- Implementation: a `bHudVisible` flag in the local player's settings/profile, driven by an Input Action; the HUD widget only listens to this flag and animates fade in/out (no gameplay logic).

## Options > Controls (view and remap buttons)
- The Options menu has a **Controls** screen where the player can **see every mapped button** for every action, for both gamepad and keyboard, and **change (remap) them**.
- Actions listed (grouped): driving (accelerate, brake/reverse, steer, drift, boost), items (use item, aim forward/back), camera (look back, camera reset), HUD (show/hide HUD), on-foot controls for Adventure hub (move, run, camera, interact), and system (pause, menu navigation/confirm/back).
- Each row shows the action name and its current binding as a **button glyph**. Glyphs match the active device (Xbox, PlayStation, generic gamepad, keyboard/mouse) and update when the player switches device.
- Remapping: select an action, press "Rebind", press the new button/key. Cancel with a dedicated key. Detect **conflicts** (same input used by another action in the same context) and let the player swap or cancel. Menu navigation and the pause button cannot be left unbound (always keep a way to open pause and navigate menus).
- **Reset to defaults** per device and per whole screen. Show which bindings differ from default.
- Gamepad-first: the Controls screen itself must be fully operable with a gamepad.
- Bindings are **per local player** (local multiplayer players can have different layouts) and saved in the player profile / settings, same storage as FOV and HUD options.
- Also available in this screen (optional, small): stick/trigger deadzone and steering sensitivity, vibration (rumble) intensity or on/off, and invert look for the on-foot camera.
- Implementation: Enhanced Input with player-mappable key settings (check the exact API available in UE 5.7 before coding), a data-driven list of mappable actions (name, category, display text, default keys), and a UMG row widget that reads the current mapping. Adding a new action later = one data entry, no UI rewrite.
- Default layout is shown on a simple read-only "controls overview" the player can open from Options or the pause menu.

## Item boxes and power-ups (Crash Team Racing style)
Item boxes are floating boxes placed on the track. The REAL power-ups do not exist yet: build the whole system with EMPTY placeholder items that are filled in later through data assets only.

### Item boxes (`AItemBox`)
- Placed in **clusters of 3 boxes across the track width** at several spots per lap (about 5-7 clusters), never in the first seconds after the start line. Avoid placing them right at a jump/landing or in a tight corner exit.
- A box floats (hover height), slowly rotates and bobs, and has a clear glow/colour so it reads from far away. Placeholder look: simple cube.
- Driving through a box collects it: the box breaks (VFX + sound + short "pop" feedback) and **respawns after a few seconds** (tunable, default about 5 s). Boxes respawn for everybody independently of who took them.
- **A new item from a box REPLACES the currently held item if that item has not been used** (developer decision, differs from original CTR). A vehicle can always break a box; it rolls a new item from the loot table and the old unused one is discarded. Show a clear swap animation in the HUD slot so the player understands the old item was replaced.
- AI vehicles collect and use items exactly like players.
- Box clusters are placed in the level via an `AItemBoxCluster` actor (spline-aware or simple 3-box row) so the track designer can drop them quickly. The test track generator already spawns placeholder boxes with the tag `ItemBox`; replace those with `AItemBox`/cluster actors once the class exists.

### Holding and using items
- Single item slot per vehicle. Using the item empties the slot; collecting from a box while holding an unused item replaces it. Use button fires the item (optionally forward/backward aiming with stick direction, like CTR). Show the held item in the HUD slot with a short roulette/shuffle animation on pickup.
- Item logic is built on the shared modifier/status-effect system and the race events (see passives), so items and passives never conflict silently.

### Loot table (data-driven, position-aware)
- `UItemDefinition` (DataAsset): id, display name, icon, rarity weight, `UItemEffect` class (empty for now), min/max rank allowed.
- `UItemLootTable` (DataAsset): weighted entries per rank band (e.g. 1st, 2nd-3rd, 4th-6th, 7th-8th). Trailing players get stronger/more helpful items, leaders get weaker ones (comeback pressure). Keep this mild because the passives already help trailing cars; make the bias adjustable.
- **For now the table contains ONE placeholder item `Empty`** (no effect, shows a placeholder icon and a log/debug message "Used Empty item"). This lets the full loop work: collect box -> roulette -> HUD slot -> use -> slot clears.
- Adding a real power-up later must need only: a new `UItemDefinition` asset, a new `UItemEffect` subclass, and an entry in the loot table. No changes to boxes, HUD or vehicle code.

### Debug
- Dev setting to force a specific item from the box, and to disable the position bias.
- Console command to give the local player any item.

## ENERGY CORE (LATER, Phase 3)
- A resource that charges while driving/drifting/collecting and is spent on each character's ACTIVE ability. Do NOT implement now, but design for it: keep an `UEnergyCoreComponent` stub and an `IActiveAbility` interface so abilities can be added per character without refactoring.

## Menus and game flow
All menus are UMG (Common UI plugin recommended for gamepad focus/navigation), fully usable with a gamepad, with clear focus highlight. Phase 1 menus are functional and grey; polish later.

### Main menu (start of the game)
1. **Single Race**
2. **Adventure Mode**
3. **Options**
4. **Exit Game** (confirmation dialog, then quit)

Options is also reachable from the pause menu (camera/FOV, controls view and remapping, HUD element toggles, audio, graphics). See the Camera section for the FOV option.

### Single Race (setup flow with options)
Flow: Race setup -> Character select -> Track select -> Race -> Results.
- **Players:** vs Computer AI (1 human) or local multiplayer with 2-4 humans (split-screen). Remaining grid slots are filled by AI.
- **Character select:** each human picks a character (8 characters, preview, stat bars/passive description read from `UCharacterDefinition`). Characters are unique per race by default (a character picked by one racer is not available to another).
- **Number of rivals / grid size:** **8 players maximum in total** (humans + AI), confirmed by the developer. With 1 human the player can choose 0-7 rivals, with 2 humans 0-6, with 3 humans 0-5, with 4 humans 0-4. The limit lives in one constant/data setting `MaxGridSize = 8` used by menu, race setup, grid spawning (8 `PlayerStart` slots) and AI. AI characters are random or chosen from the remaining characters (unique per race, so 8 characters always fit 8 slots).
- **Track select:** list is driven by `UTrackDefinition` data assets (name, preview image, map, lap count default). Only the test track exists for now.
- **Extra options (confirmed):** lap count, AI difficulty (data-driven parameter sets), item boxes on/off.
- Then Race, then Results (positions, times), with Restart / Change setup / Main menu.
- The setup is stored in one struct (`FRaceSessionConfig`) held by a GameInstance subsystem (`UGameFlowSubsystem`), so Single Race and Adventure Mode start races the same way.

### Adventure Mode
Singleplayer only.
1. **Start:** NO character selection. The player character is fixed: **Spark (blue)**, and Adventure Mode is Spark's story/adventure. Spark's hovercar and Spark's passive are used in every Adventure race. Then an **intro cinematic placeholder** (skippable), implemented so a real video can be dropped in later.
2. **Hub map (small open-space level):** Spark walks/runs on foot (third-person on-foot controller: move, run, camera, interact; gamepad first). Placeholder on-foot body (blue capsule or simple mesh) is fine until the real Spark model exists. The player character is a data reference (`UAdventureCampaignDefinition::PlayerCharacter` = Spark's `UCharacterDefinition`), not hard-coded, so another character's story could be added later.
3. **Portals:** 5-10 portals in the hub (count is data-driven). Each portal is one race level. Portal actor: `AAdventurePortal` (overlap + interact prompt).
   - **Progression:** only the first portal is open at the start. Level N+1 opens only after the player **wins level N (finishes 1st place)**. Locked portals are visually locked and show a hint when approached. Finishing 2nd or lower does not unlock the next level; the player can retry.
   - Progress is saved (`USaveGame`, slot-based) and restored on load.
4. **Entering a portal:** plays a **placeholder cinematic** (a "Cinematic placeholder: Level N" screen, skippable) designed for a real video to be inserted later (Media Framework: media player + media source/texture, referenced per level in data; falls back to the placeholder if no video is assigned). Then the race loads using that level's configuration.
5. **After the race:** results screen. If the player won: unlock next portal (short unlock feedback in the hub) and return to the hub. If not: Retry / Return to hub.
- Rivals in Adventure races are chosen per level in data (specific characters or random from the other seven, never Spark).
- Per-level data: `UAdventureLevelDefinition` (portal id, display name, order index, track, lap count, number of rivals, AI difficulty, intro video asset reference, unlock requirement = "win previous level"). Adding or reordering levels must need only data asset changes.
- Debug option: unlock all portals.

### Architecture notes
- Separate maps: `MainMenu`, `AdventureHub`, and one map per track. Use a GameInstance subsystem for session config, save data and level flow; do not put this state in actors that die on level change.
- Menus never hard-code characters/tracks/levels; they read data assets.
- Race results are reported through a single `OnRaceFinished(ResultData)` event consumed by Single Race (results screen) and Adventure Mode (unlock logic).

## Rendering rules (UE 5.7)
- Lumen Lite is a 5.8 feature, so NOT available. Use Lumen with reduced quality settings, or simple/baked lighting where possible. Provide scalability presets.
- MegaLights: check its status in 5.7 (it may still be beta/experimental). Do not rely on it for core visuals until verified in a test scene.
- Split-screen (2-4 players): cheapest rendering path. Reduce or disable GI, lower resolution scale, lighter post-process, fewer particles. Separate scalability presets per player count.
- Speed lines and motion effects must be cheap (Niagara or a simple post-process material); budget per viewport.
- No Experimental features for core gameplay or tracks. Tracks use spline meshes/modular kits and Landscape or static geometry.
- Profile (stat unit, Unreal Insights) after every visual feature. Target 60 FPS on mid-range PC, in 1-player and in split-screen.

## Architecture rules
1. `ARaceVehicle` does not know if a human or an AI drives it. Control comes through a small input struct (throttle, brake, steer, drift, boost, use item). PlayerController and AIController write into it.
2. Replication-ready: use GameMode / GameState / PlayerState properly; race state (laps, checkpoints, positions) lives in GameState/PlayerState. Do NOT implement networking now.
3. Race logic: spline checkpoints, lap counting, live ranking, results.
4. AI: follows a track spline racing line with lateral offsets, rubber banding, simple avoidance; difficulty is data-driven. Uses the same vehicle physics and character stats as players.
5. Performance target: 60 FPS on mid-range PC. Avoid heavy Tick work; support reduced rendering in split-screen.

## Code conventions
- Unreal coding standard (A/U/F/E prefixes, PascalCase, `b` for bools).
- Small single-purpose classes; `UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=...)` for tuning.
- No magic numbers. Short comment block on each new system.
- No deprecated APIs. If unsure about an API for this UE version, check engine source/docs instead of guessing.

## Workflow rules (important)
- Plan first, then implement. Any task larger than one class starts with a written plan and waits for approval.
- Small steps: one feature, build, test, commit.
- After code changes, always run the build and fix compile errors before reporting done.
- Never delete or overwrite assets (.uasset, .umap) without asking. No mass renames/moves.
- Do not modify engine files. Tell the developer before touching Config/ or the .uproject.
- Do not commit unless asked; suggest a commit message when a step is done.
- If a request is ambiguous, ask one short question.

## Phases
- **Phase 1 (now):** grey-box hover prototype on a simple test track. Order: hover + thrust + steering -> drift + boost -> third-person camera (adjustable base FOV + speed FOV), speed lines -> vehicle-vehicle impact -> stats system with 2 test characters.
- **Phase 2:** 8 characters as data assets (8 passives defined, values to tune), modifier stack + race events + all 8 passives with debug HUD, character select menu, item boxes (CTR style clusters) with EMPTY placeholder item + loot table + HUD slot, AI for 7 opponents, laps/positions/results.
- **Phase 2b:** main menu (Single Race setup flow, Options, Exit), Adventure Mode with placeholder intro and portal cinematics, small hub map with on-foot character, portal unlock progression and save/load. Placeholders only (grey hub, placeholder videos).
- **Phase 3:** ENERGY CORE + active abilities, split-screen polish, final art, audio.
- **Later:** online rooms.

## Out of scope for now
Online multiplayer, final models/textures, audio polish, active abilities, real cinematics/videos, final hub art. (Adventure progression is placeholder-level only.)

## Art pipeline (reference only)
2D concept -> ComfyUI image-to-3D -> Blender cleanup (split parts, retopo, UV) -> PBR textures -> UE import with LODs and collision mesh. AI-generated models are raw material, not final assets.
