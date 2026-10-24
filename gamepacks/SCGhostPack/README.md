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

- **S:** NOD surface material/UV tools.
- **View > NOD Layers and View Options:** entity layers and visibility controls.
- **View > NOD Template Archive Editor / NOD Scene Property Editor:** external
  SDK editors. Save native edits to workspace copies, not original assets.
- **nod_static_light:** baked lighting helper. `_color` is RGB 0..1,
  `light` is intensity, and `radius` is falloff distance in world units.
  A NOT template named `light` remains a separate native entity.

## Navigation

Place `nod_npd_waypoint` nodes on walkable ground. Select two nodes, source first,
then **Ctrl+K** to create a directed `nod_npd_link`. Reverse travel needs a reverse
link. Keep node IDs unique and remove incident links when deleting nodes.
Automatic waypoint generation and walkability validation are not implemented.

## Export

Use **Bsp > Convert NOD level...** with same-build SDK executables
`MAPtoNIL`, `MAPtoNSD`, and `MAPtoNPD`.

- Select NIL geometry/lighting, NSD placements, and NPD navigation as needed.
- Output must be inside the workspace; default: `nod/compiled/Levels`.
- **Save and Export** saves the MAP, then converts one shared temporary snapshot.
- **Override Map Name** changes the output filename stem, not the source MAP.
- Unselected outputs are preserved. Conversion failures are reported; retain
  any recovery directory identified after a publication/rollback failure.

## Limits

- MAP is Radiant source data, not the original NOD authoring format.
- Current geometry export uses one sector. Lighting has no GI or model-cast
  shadows; point lights can cross walls. Maximum: 64 point lights.
- Surface previews and animation support are incomplete; editor-only settings
  do not imply verified native engine semantics.
- Use existing runtime types and supported level entry points. Keep backups
  and validate exports in a disposable game runtime.
