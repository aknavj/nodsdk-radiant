# SC Ghost / NOD authoring profile

## Setup

Build and launch using the [root instructions](../../README.md#build-and-run).
Select **StarCraft Ghost (NOD authoring)**.

In **File > Project Settings**:

| Setting | Value |
| --- | --- |
| Asset root | Original game directory containing `Materials`; read-only. |
| SDK editor binaries | Folder containing `NODTemplates`, `NOTEditor`, `NSDEditor`. |
| Map folder | Optional override for `<workspace>/nod/maps`. |

Keep the writable workspace separate from original assets. Restart after changing
the asset root. Reinstalling the profile through Configure games can reset paths.
The starter map is `<workspace>/nod/maps/test_room.map`.

## Entities and assets

- **N > Entities > Place entity:** search and place existing NOT templates.
  Global loads automatically; a new MAP does not require an NSD.
- **Selection:** edit local properties and effective inherited values.
  **Apply** creates/changes an instance override; **Remove** reveals its default.
  Shared NOT definitions remain read-only in Radiant.
- **Map resources and metadata > NOT archives:** up to nine additional archive
  names, separated by semicolons; order matters. Set context before placement.
- **Reload NOT metadata:** refresh after external NOT edits.
- **Map model bundle:** explicitly select the map's NMB in `3D/Models`;
  no bundle is inferred from the MAP filename.
- **Flush & Reload Models:** refresh meshes after external model changes.
- Material previews use `Materials/shaderlist.txt` and its listed NSA archives.
  Missing materials/models produce diagnostics and editor placeholders.

An empty placement list usually means missing asset/SDK paths or an incompatible
`NODTemplates` executable. Unresolved templates cannot be placed.

## Editor tools

SC Ghost geometry uses **1 MAP unit = 1 meter**. Fractional world-brush
coordinates are preserved during loading and rebuilding, independently of the
legacy coordinate-clamping and tiny-brush cleanup preferences. Thin brushes use
the native clipping tolerance. Other game profiles retain their existing rules.
If an older editor displayed missing walls or details, reopen the original MAP
after updating; saving the collapsed geometry cannot recover the lost planes.

NOD material alpha-test/culling/transparency preview settings remain on the
shader; applying a material does not copy them into MAP surface flags.
`MAPtoNIL` still rejects nonzero authoring contents/flags/value fields rather
than assuming native semantics. Maps saved by older editor builds may contain
leaked preview flags; clear those fields on affected faces without changing
their materials or UVs. Restart the editor after installing an updated build.

- **S:** NOD surface material/UV tools.
- **View > NOD Layers and View Options:** entity layers and visibility controls.
- **View > NOD Template Archive Editor / NOD Scene Property Editor:** external
  SDK editors. Save native edits to workspace copies, not original assets.
- **nod_static_light:** baked lighting helper. `_color` is RGB 0..1,
  `light` is intensity, and `radius` is falloff distance in world units.
  A NOT template named `light` remains a separate native entity.

For brush-primitive surfaces, texture rotation in the Surface Inspector and
rotation shortcuts preserve the full affine UV mapping, including existing
shear and mirroring. Repeated apply/close/reopen cycles retain texture-size
normalization. Shift and scale edits operate relative to the original mapping;
**Axial**, **Reset UV** and captured UV presets deliberately replace it.
**Fit** commits its fitted mapping as a separate undo step before further edits.
For classic MAP faces, **Fit** measures the actual rotated face projection,
including at 90 degrees, preserves mirrored scales and keeps fractional shifts.
Set the desired rotation first, then **Fit** to apply the requested width/height
repeat counts in that orientation. Rotating an already fitted texture preserves
its scale; it does not automatically fit the new orientation.
Install both the updated editor and Surface plugin, then restart.
This does not enable material/UV edits in source-backed native scene export.

## Sky preview

The textured 3D camera automatically previews a worldspawn `nod_sky_model`
when the referenced asset resolves through the editor's configured asset roots
and optional `nod_model_bundle`. The static sky stays centred on the camera and
appears behind map geometry; it is not selectable and is not added to the MAP.
Wireframe/solid views and other game profiles are unchanged. Missing or invalid
sky models are reported in the console; after adding assets, use **Flush &
Reload Models** to retry loading them. This is an editor-only background preview:
it does not animate `nod_sky_animation` or reproduce the native sky-surface
stencil, and does not change `nod_sky_unmasked` or runtime asset requirements.

## Navigation

Place `nod_npd_waypoint` nodes on walkable ground. Select two nodes, source first,
then **Ctrl+K** to create a directed `nod_npd_link`. Reverse travel needs a reverse
link. Keep node IDs unique and remove incident links when deleting nodes.
Automatic waypoint generation and walkability validation are not implemented.

This also works in imported source-backed scenes: move existing nodes, add new
nodes, or select existing/new endpoints and connect them. Imported node IDs are
fixed. Imported link fields are native **record indices**; newly authored link
fields are **waypoint IDs**. Use selection-based Ctrl+K instead of guessing
indices. Normal v2 graph topology is rebuilt by the updated `NODscene`; unsupported
non-normal layouts fail without publishing outputs.

## Consoles, sound cues and enemy spawning

### NOT-first native authoring (recommended)

Place the actual NOT template, then open **N > Selection**. Select a
**Native handler** and click **Initialize / reset**:

| Resolved NOT capabilities (examples) | Available native handlers |
| --- | --- |
| Interactive modeled props, e.g. wall/floor/Protoss console variants | `cTriggerOnAction`, `cButton` |
| Nonmodeled, noncolliding helpers, e.g. `script_object`, `script_target` | `cSoundStinger`, `cAIFightPath`, `cTriggerRelay`, `cCutscenePlay`, `cTimerTrigger`, `cFogSet`, `cSoundLoop`, `cEffectAdd`, `cObjectExplosion`, `cRain`, `cObjectRemove`, `cHelpText`, `cPopupConversation`, `cMissionNext` |
| Modeled colliding props, e.g. doors and platforms | `cDoorMove` (translating model, two stops), `cObjectSetCollision` |
| Modeled type-2 props | `cObjectMove`, `cObjectRotate`, `cObjectSetColor` (shader frames, not RGB) |
| Unmodeled, noncolliding emitters with property 10, e.g. `steamJetBig` | `cParticleToggle` |
| Modeled, colliding MOVETYPE-3 props, e.g. `crate_25kg` | `cPhysicsActivate` |
| Native type-3 regions, e.g. `trigger_region` | `cRegionTrigger`, `cObjectiveUpdate`, `cCutscenePlay`, `cRegionSound`, `cRegionSave`, `cRegionStealth`, `cRegionKillZerg`, `cRegionDamage`, `cFogSet`, `cPopupConversation`, `cMissionNext`, `cLevelChange` |

This keeps the NOT entity, its model, inherited defaults and local overrides.
The SDK supplies parameter types/defaults, labels, constraints, frame hints and
connection layouts. Compatibility uses resolved NOT properties, not asset-name
lists. The editor creates a unique
`targetname` and the required NSD parameter/frame fields. Unsupported native
classes are not offered: NOT object properties alone do not describe every
C++ handler's variable-length NSD layout.

The registry now contains 31 workflows. New native resource fields accept
1..31 ASCII name characters, not paths; explicitly optional portrait/shader
fields may be blank. Names are not checked against deployed resources.
Ordinary connections send native trigger type 0; relay data is not a type
selector. Particle input toggles, rain input stops only, collision uses
data0=1 for on/visible and other data for off/hidden. Removal references are
objects to remove, not Ctrl+K outputs. Popup's output fires on its last briefing
removal notification. Help shares a global UI and a 120-second consumption timer.
Mission controls can leave/reload the level: next-mission delay truncates to
whole seconds; level input always toggles and data0=99 additionally transitions.
Translation startup adds one second to its authored delay. Rotation frames
use native degrees, and disabling its chain does not interrupt the current turn.
Fixed advanced modes remain read-only. Pickups, unrelated regions and complex
lightning/constraint/sliding/frame-mover workflows are not added.

For a native mission objective, place `trigger_region`, choose
**cObjectiveUpdate**, and initialize it. Set **Complete deployed objective (key)**
and/or **Show deployed objective (key)** using existing keys from the active
mission's deployed `OBJECTIVE` entries. These are plain key fields, not new
objective declarations or a mission picker; the SDK validates their syntax but
does not check mission membership. Set the local region bounds and initial enable
flag. Select the region then a door/cue and press **Ctrl+K** to link its verified
output. Player entry updates the objectives and triggers that output once;
nonplayers and repeat entry do not. Advanced briefing/savepoint/logbook fields
are intentionally read-only. Switching between region handlers preserves custom
bounds; clearing the authored handler removes them and supports Undo/Redo.

For sound, set **Sound cue** to a deployed bank cue. For enemies, choose
**Actor template**, click **Choose NOT template...**, then set
**Lifetime spawn limit** (1..25) and both frame positions.
The searchable picker uses active ordered NOT archives; additional actor types
(including firebats and variants) do not require editor changes. Candidates need
native actor type 0, actor movement, a model and AI. This excludes abstract helpers
and AI-less props, but mission-specific resources and navigation still need
in-game testing. New spawners require an explicit actor selection.
Spawn frames initialize at the placement and two meters along +Y, with native
rotation zero; edit frame rotations separately from the owner.

For an existing cutscene, initialize **cCutscenePlay** on a region or compatible
helper. Enter **Deployed cutscene (NCS name)** as a basename with optional `.ncs`
suffix (1..31 ASCII characters; no path). Region player entry requests playback;
helpers need an incoming trigger. **Cutscene one-shot** defaults to 1 and counts
attempts, not confirmed playback. Startup, preload, video and actor-removal
options remain protected. Select the owner then a receiver and press **Ctrl+K**
to connect **On cutscene finish or stop: linked target**. This output sends
`p0=0` for normal finish and explicit stop; it is not success-only.
The compiler checks name syntax, not deployed resources or mission dependencies.
NCS files can request a level change; use resources verified for the target map.

For automatic periodic outputs, initialize **cTimerTrigger** on a compatible
helper. Set intervals A and B (default 5/5 seconds, range 0.001..599.999), then
select the timer before a receiver and press **Ctrl+K** to append up to eight
outputs. A runs first; B/A alternate thereafter. All targets receive `p0=0`
on every expiry. This handler starts with the scene and has no trigger input,
one-shot or start/stop control. Timer owners share the native 128-slot pool
with other scripts; leave headroom. Reload NOT metadata after updating the SDK.

For area ambience/reverb, initialize **cRegionSound** on `trigger_region`.
Set **Region reverb preset** to -1 (unchanged) or 0..25, and optionally enter
**Region ambient cue** (at most 31 ASCII characters, no path). Primary-player
entry applies the settings. Player exit restores world defaults after 0.5s
only when no sound region remains occupied. There is no settings stack,
trigger input or linked output. Use a nonempty world ambient baseline:
an empty baseline does not stop the old loop. New and imported typed edits
share validation; existing opaque geometry and unchanged payloads are preserved.

For native checkpoints, initialize **cRegionSave** on `trigger_region`.
Keep **Objective marker** at -1 unless deliberately authoring a numbered
0..15 marker route; set **Checkpoint initially enabled** to 0 or 1.
Ctrl+K from a console/button/relay enables a disabled checkpoint but does not
save immediately or check current overlap: leave and re-enter if already inside.
Savable controlled-player entry consumes one attempt before requesting native
checkpoint capture. Triggering again cannot rearm it; there are no linked outputs.
The save hint also appears after rejected requests and does not prove persistence.
Use valid mission context and verify actual reloads with backed-up test saves.
Fresh regions have no respawn frames; imported zero/two-frame payloads are retained.

For native location-stealth state, initialize **cRegionStealth** on `trigger_region`.
Set a finite **Location stealth value (native)** (default 0.5) and
**Stealth region initially disabled** (0/1, default 0). Each incoming trigger
toggles the entry gate, ignoring numeric data. Toggling does not refresh a
player already inside. Controlled-player entry writes the value; exit always
resets zero, even when disabled or another stealth region remains occupied.
There are no outputs or restoration stack. This is verified raw native state,
not a proven invisibility/AI/HUD effect: the tested visibility query is unchanged.
Use instrumented conformance checks, and prefer disjoint regions.

For native Zerg-kill volumes, initialize **cRegionKillZerg** on `trigger_region`.
Set up to four **Exempt actor** fields to placed actors' `targetname` or imported
`nsd:OFFSET` identities; leave unused slots at -2. These are exclusions, not
outputs, so edit the fields rather than using Ctrl+K. Native actor kinds 0/7
with actor-type bit 0x8 request death on entry unless exempted. Original death
guards still apply. Ordinary Marine/Nova types are unaffected. There are no
trigger inputs, enable/disable controls, frames or outputs. Exemptions apply
only to their own region; an actor-spawn helper does not identify its children.
Native death-state checks pass; live overlap tracking and death presentation
remain unverified.

For native player damage, initialize **cRegionDamage** on `trigger_region`.
Set finite nonnegative **Damage per tick** and initial enable 0/1. Keep both
phase durations zero for manual control, or set both positive to cycle.
Native code adds 0.1 seconds to each phase; the editor/SDK cap them at 599.899.
Primary-player entry starts immediate and quarter-second damage ticks.
Ctrl+K from a console/button/relay toggles the gate; it does not interpret
float data as explicit enable/disable. Rapid re-enable while occupied can
duplicate pending tick chains: let disabled ticks drain or leave first.
Keep other-actor kill/container options off for normal player damage.
The other-actor kill mode ignores the enable gate, and container references
are not outputs. Native kill modifier/damage type remain read-only for this
workflow; unchanged imported legacy values are retained. Native health/timing
and exact roundtrips pass; live tracking/HUD/gameplay remain unverified.

For native environment fog, initialize **cFogSet** on `trigger_region`, or on
an unmodeled/noncolliding `script_object` helper. Region entry accepts any
type-7 player; exit behavior 1 restores its captured fog, 2 keeps it.
Ctrl+K to a fog controller toggles apply/restore; trigger ID/data are ignored.
Set integer RGB 0..255, mode 0/1, finite nonnegative start/end distances
(linear end must exceed start in binary32), transition 0..599.999 seconds and
smoke 0/1. Bounded integer edits require canonical digits without leading zeros.
Positive fades require matching old/new fog modes; a mode change applies
immediately. Fog is global, not volume-clipped, and overlapping regions are
not a restoration stack. Leave smoke at zero for fog-only tests: restoration
does not cancel pending smoke callbacks. Native scene state/interpolation and
exact triples pass; live rendering remains unverified.

For native owner-bound looping playback, initialize **cSoundLoop** on an
unmodeled/noncolliding helper such as `script_object`. Enter a deployed
**Loop cue**; the native wrapper strips its final extension. Playback is
requested automatically after 0.1 seconds. Ctrl+K to the helper supplies a
stop request after startup, not a reusable toggle: the native stop branch
retains its cached handle, so further triggers cannot restart. Restart the
scene to repeat. Trigger IDs/data are ignored. Three legacy floats stay at
read-only shipped defaults 5/10/1, not range/volume settings. New loop startup
timers share the 128-slot guard with timer triggers/cycling damage and weighted
gameplay-control startup timers. Collision initialization uses one slot plus
another when initially hidden; shader setup, automatic translation/explosions
and timed initially enabled emitters each use one. Leave runtime headroom.
Native callbacks, normal mixer stop flags and exact triples pass; audible
looping, spatialization and stop timing still need in-game confirmation.

Select the console or wall button first, then a cue, relay or model mover, and
press **Ctrl+K**. Wall-button links append into eight free target slots;
reconnecting an existing target does not duplicate it. Select a
relay first and a cue/relay second to append an action. Connecting a relay to
a native `cAIFightPath` adds the verified start/stop pulse in two free actions;
connect the console/button to that relay, not directly to the AI path. You can edit
the relay's target, delay and trigger data fields afterward. References use
`targetname` (new objects) or `nsd:OFFSET` (imported objects), never guessed GUIDs.

For a door or platform, place one of the modeled templates above, set its
origin/angles, and initialize `cDoorMove`. Edit **Mover stop 1 (open / upper)**
and **Mover speed (world units/second)**. Stop 0 must match the placement origin;
both native frame rotations must match the placement orientation. Resetting
initializes these correctly: stop 1 defaults to +2Z for any model.
Choose the desired door-travel direction by editing stop 1.
If you subsequently move/rotate the owner, realign its frames or reset them.
The compiler applies per-placement tracker `MOVETYPE=2`, `PHYSICSFLAGS=1024`,
preserving the model, collision model and shared NOT defaults. Conflicting
explicit movement/physics overrides are rejected.

Each activation toggles between stop 0 and stop 1, with no startup motion.
This is a two-stop translating mover, **not** a floor-selecting elevator,
automatic proximity door, portal controller or mission-specific elevator script.
Pressing again during travel can reverse it. Set platform stop 1 above stop 0
and link a button at either landing for a basic toggle lift. Wall buttons fire
after the native 0.5-second delay and have a 1.5-second reuse interval. Their
**one-shot** and **initially locked** fields accept 0/1; the shader reference
is optional and model-specific. Other unverified settings
are read-only. Arbitrary models, pivot/jitter modes and linked door callbacks
are intentionally not exposed. Original callbacks and tracker interpolation
are tested; player riding, collision clearance and final interaction placement
still need in-game testing.

Changing/resetting a handler replaces its parameters/frames after confirmation
and supports Undo. **None (template default)** removes the authored handler
body without changing NOT defaults. Existing imported handlers retain their
original schemas; edit their typed fields rather than replacing their class.
New NOT objects may also be added to source-backed scenes, using the scene's
existing archive order. Export preserves original objects, geometry and NPD
unless those supported properties/navigation fields were explicitly edited.

### Optional preset shortcuts

Use the entity placement list or right-click in a 2D view to place:

| Entity | Settings in **N > Selection** |
| --- | --- |
| `nod_console` | Position, rotation and target. Optional `nod_console_template` selects a compatible NOT model. |
| `nod_sound_cue` | Unique `targetname` and `nod_cue`, for example `music_puzzle`, `music_stealth`, `music_combat` or `music_stinger`. |
| `nod_enemy_spawn` | Unique `targetname`, catalog-backed `nod_enemy`, `nod_destination` and `nod_max_spawns` (1..25). |

Place helpers on clear ground. A new spawn helper requires an actor choice, with cap25,
and a destination two meters along world +Y; adjust that destination manually.
Console presets preview the real `hackConsole_wall_activate` NOT model, with
its native scale and editor bounds. This also applies to existing MAPs after
reopening them. The SDK and asset paths must be configured; unavailable assets
produce a diagnostic and leave a selection marker. Sound/spawn helpers remain
markers rather than model previews. Editor bounds are not collision bounds.
The console's interactive front must face into accessible space: zero editor
angles face +Y; editor yaw180 faces -Y.

Select the **console first**, then the sound/spawn helper, and press **Ctrl+K**.
This sets a symbolic target reference; do not enter guessed native GUIDs.
The compiler resolves object indices and emits the verified native script
parameter arrays. An enemy helper includes a hidden start/stop pulse relay.
The generated relay is co-located with its spawner, and helper rotations follow
the preset's AI frames. These invisible helper transforms can differ from an
imported scene without changing console placement or AI path frames.
There is no automatic sound playback or enemy wave. Leave at least0.5 seconds
between spawn presses; the lifetime limit is not replenished by kills.

For imported scenes exported with the updated `NODscene export-properties`,
**N > Selection** exposes typed script parameters and existing path frames:

- `cSoundStinger`: **Sound cue** (`nod_script_param_6`).
- `cTriggerOnAction`: linked object (`nod_script_param_16`); Ctrl+K can wire
  this console to a new helper or an existing `cSoundStinger`/`cTriggerRelay`.
- `cAIFightPath`: **Enemy template** (`nod_script_param_2`), **Lifetime spawn
  limit** (`nod_script_param_3`) and **Spawn path destination**
  (`nod_frame_origin_1`).
- References use `nsd:OFFSET` source identities or new helper names. `-1`/`-2`
  are empty references. Counts, parameter types and source identities are immutable.
- Frame rotations are **native** Euler values; unlike editor `angles`, the
  corrected arena-facing rotation is `nod_frame_rotation_N = 0 0 180`.
  Edited frame sectors currently support0 only.

Do not connect a console directly to an imported `cAIFightPath`: connect to its
`cTriggerRelay` pulse helper. Add new controls using the three presets rather
than cloning source-bound NSD identities. Legacy source MAP/sidecar pairs still
compile unchanged; re-export to expose the new fields.

Cue labels do not force alarm/score modes. Enemy AI supplies actual alarm
transitions. Deploy the matching level XSB **and** XWB; storing a cue does not
install its sound bank. Independent cues may overlap; no repeat timer or
stop-all control is added.

## Scene environment

Open **Entities > Map resources and metadata > Scene environment...**. Changes
are saved as worldspawn keys in the MAP, with one undo step; Cancel changes
nothing. Updated property exports also expose supported environment fields for
imported scenes; their unrecognized native words are preserved.
Unchanged fields preserve their original values, including omitted settings.
Clear a field to remove it. SDK binaries built from `5bd1c0e` or newer are
required to export these settings through `MAPtoNSD`; NIL/NPD output ignores them.
The unmasked-sky control additionally requires the updated `MAPtoNSD` built
with `nod_sky_unmasked` support; older SDK binaries reject that key.

| Setting | Worldspawn key | Native limits |
| --- | --- | --- |
| Music reference | `nod_music` | Stored reference; does not override native score selection. |
| Ambient loop cue | `nod_ambient_loop` | Friendly cue in the loaded level XSB; last extension is stripped. |
| Sky model | `nod_sky_model` | Optional `.nod` filename. |
| Sky animation | `nod_sky_animation` | Optional `.nad` filename; independent of model. |
| Unmasked sky | `nod_sky_unmasked` | `1`: full-viewport sky; `0`/omitted: original surface stencil. |
| Fog mode | `nod_fog_mode` | Case-sensitive `none` or `linear`; omitted means none. |
| Fog RGB | `nod_fog_color` | Three finite values in **0..255**, not 0..1. |
| Fog start/end | `nod_fog_start`, `nod_fog_end` | Finite MAP-unit distances; `0 <= start < end` after float32 rounding. |
| Far clip | `nod_far_clip` | Nonnegative float32 MAP-unit distance; omitted/0 uses the native default. |
| Reverb | `nod_reverb` | Canonical decimal preset ID **0..25**, no signs or leading zeros. |

Linear fog requires color, start and end together. To disable it, clear those
three fields and clear mode or enter `none`. Reverb IDs have no assumed named
aliases or "off" meaning for 0. Far clip is independent of fog and collision
bounds; choose explicit values beyond the runtime camera's near plane.

Nonempty references allow at most 31 ASCII bytes: letters, digits, `_`, `-`,
and `.`; no folders. Audio names cannot start/end with a dot. Sky extensions are
case-insensitive. Names are preserved, not deployed or checked for existence.
Native reader/state handoffs are verified by the SDK, not live audio playback,
sky rendering/animation, visible fog or audible reverb. NSD volume/effects fields
remain unsupported; do not infer percentage or float semantics for them.

An open room without sky-mask surfaces needs **Unmasked sky = 1** as well as
the deployed sky model/animation. The SDK verifies native scene flag `0x2`
and the original full-viewport stencil path; this is not an effects flag.
Defaults retain existing masked rendering.

Test Room (`1_0_0_Test_Room`) loads `Sounds/1_0_0.xsb` and `.xwb`.
An ambient cue in another mission's bank will not start merely because its
name is stored in NSD. Native alarm/score playback requests `music_stealth`,
`music_combat`, `music_puzzle` and `music_stinger` from the same loaded bank.
Use matching resource copies in the authoring workspace; do not edit originals.
Deploy the environment-bearing **NSD**, not only NIL geometry.

## Export

### Writing native-compatible script code

The `NODScript` executable compiles versioned `.nods` source into native control
graphs. It is not a Lua/C++ compiler or a new game VM: only the verified native
actions below are supported. Unsupported statements, invalid values, duplicate
names, dangling targets and sequence cycles report source file/line/column
errors and stop export before any runtime files are replaced.

Start with [trigger_sequence.nods](install/nod/scripts/trigger_sequence.nods).
For the existing test arena, open its **layout-only** `arena_layout.map`, choose
that script in **Bsp > Convert NOD level... > Script source**, then **Save and
Export**. This adds one north-wall console which requests `music_stinger`, waits
two seconds, then pulses one marine spawn. Do not combine the example with the
full `arena_authoring.map` unless you move its additional console/spawn positions.

Edit the `.nods` file in your text editor. The dialog remembers it in the MAP's
`nod_script_file` key, relative to the authoring workspace when possible. **Clear**
removes the reference. Export snapshots the source, MAP and optional sidecar
before running the script compiler; all selected outputs use the same expansion.
Source-created entities are build products, not automatically inserted into the
open editor MAP. Import an explicitly generated expanded MAP if you need to
inspect their markers/models. Do not attach the script again to that expansion.

Declarations are case-sensitive and may reference later declarations or named
native helpers already in the MAP:

```text
nodscript 1;
sound alarm { origin (0, 0, 1); cue "music_stinger"; }
sequence alert {
    trigger alarm;
    wait 2;
    trigger marine_pad;
}
```

- `console NAME { origin (...); angles (...); target NAME; }` creates a reusable
  wall console. Origin and target are required; angles default to zero.
- `sound NAME { cue "CUE"; origin (...); }` creates a manually triggered cue.
- `enemy NAME { origin (...); template "Marine"; destination (...); limit 25; }`
  creates a start/stop spawn pulse; template names resolve through the active NOT catalog.
- `sequence NAME { trigger NAME; wait SECONDS; trigger NAME; }` schedules actions
  relative to activation. Waits accumulate; they are not a blocking runtime thread.
  Position/angles are optional. A sequence supports 1..12 actions, at most 12
  transitive actions including nested sequence activations, and no cycles.
- Existing imported receivers use quoted `nsd:OFFSET` identities and must be
  verified `cSoundStinger` or `cTriggerRelay` objects, not direct AI spawners.
- Source is ASCII, supports `//` comments and printable quoted strings without
  escapes, and is limited to 1 MiB. Vectors use three finite comma-separated
  coordinates. Statements end in semicolons. Declaration blocks do not.

No `if`, variables, arbitrary loops, event polling, forced alarm modes or new
runtime classes are invented. Console activation starts the sequence; repeated
activations can overlap. Preserve at least 0.5 seconds between spawn presses and
the native 1..25 lifetime cap. Sound playback still needs the deployed XSB/XWB.
Only sector 0 is supported. Native scene compilation enforces its final
1024-object limit, including hidden spawn relays.

Standalone workflow, using Windows paths:

```powershell
NODScript.exe --check scripts\trigger_sequence.nods compiled\trigger_arena_runtime\source\arena_layout.map
NODScript.exe --workspace . scripts\trigger_sequence.nods compiled\trigger_arena_runtime\source\arena_layout.map expanded.map
MAPtoNSD.exe --workspace . --asset-root "C:\path\to\game_files" expanded.map expanded.nsd
```

`--check` checks syntax, supported preset schemas and named references against
the input MAP. Full scene/layout/material limits are checked by the subsequent
native compiler. For imported scenes, pass the expanded MAP to `NODscene compile`
with the original matching sidecar instead of `MAPtoNSD`. Source MAP geometry is
preserved, and the `nod_script_file` build-only key is removed from the expansion.

### Level conversion

Use **Bsp > Convert NOD level...** with same-build SDK executables
`MAPtoNIL`, `MAPtoNSD`, and `MAPtoNPD`.

- Imported scenes are detected from their `nod_scene` binding. Choose the matching
  `.nodsrc` sidecar in the export dialog; these use the updated `NODscene` compiler.
  Save As does not copy the sidecar. Geometry and baked lighting remain unchanged;
  lighting/physics overrides are disabled for imported scenes.
- Select NIL geometry/lighting, NSD placements, and NPD navigation as needed.
- Output must be inside the workspace; default: `nod/compiled/Levels`.
- **Save and Export** saves the MAP, then converts one shared temporary snapshot.
- **Override Map Name** changes the output filename stem, not the source MAP.
- Unselected outputs are preserved. Conversion failures are reported; retain
  any recovery directory identified after a publication/rollback failure.
- New MAPs can use the three native control presets without an NSD sidecar or
  NOT-template registration. Keep existing runtime startup/entry-point constraints;
  this does not create new missions or patch the game executable.
- Script source requires the same-build `NODScript` executable alongside the
  selected native compilers. Sequences emit `nod_trigger_relay` presets accepted
  by both ordinary and imported scene compilers.
- **Override automatic physics margin:** positive MAP-unit padding on all six
  sides of automatically computed collision bounds. Disabled retains one unit.
  Applies whenever NIL is rebuilt, including lighting-only rebuilds; ignored for
  NSD/NPD-only exports. Requires updated `MAPtoNIL`. The SDK's explicit
  `--physics-bounds` alternative remains CLI-only and is mutually exclusive.
- DDS-backed archived NSA materials now compile through the updated SDK,
  preserving native names and editor-image UV dimensions. Missing/ambiguous
  resources still fail explicitly. Lighting limit errors report a partial
  subdivision count and active `nod_light_spacing`; increase spacing to reduce
  subdivision, rather than treating that count as a final mesh estimate.

Updating the pinned SDK **source** does not update the executables configured
in Project Settings. Rebuild the SDK from `5bd1c0e` or newer and select its output
folder; keep the three converters from the same build.

## Scriptless startup and collision

A compiled player-template placement is not a guaranteed active-player spawn.
Native routing depends on placement flag `0x8` or save restoration; missing
sectors/starts can fall back to the origin. Startup scripts/sequences are not
generated, and `info_player_start` is not a native placement shortcut.

Keep the origin in clear playable space with room for the controller above
the floor. Do not start embedded in brush surfaces. Native sphere tests reproduce
first-step NaNs at embedded starts with both 1- and 100-unit margins; increasing
physics bounds is not a fix. Clear-start sphere controls are not full
player/controller validation. Test startup, movement and scene effects in a
disposable target runtime before relying on an export.

## Limits

- MAP is Radiant source data, not the original NOD authoring format.
- Current geometry export uses one sector. Lighting has no GI or model-cast
  shadows; point lights can cross walls. Maximum: 64 point lights.
- Surface previews and animation support are incomplete; editor-only settings
  do not imply verified native engine semantics.
- Use existing runtime types and supported level entry points. Keep backups
  and validate exports in a disposable game runtime.
