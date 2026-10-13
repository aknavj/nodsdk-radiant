# StarCraft Ghost / NOD authoring profile

This is the first editor-only milestone. It uses GtkRadiant's existing Quake 3
`.map` reader/writer, classic brush planes, shader preview and `.def` entities.
Those are source-editing facilities, not evidence that NOD uses the same map
format or compiler. No proprietary game assets are included.

## Setup

Build the modified GtkRadiant editor using its normal build instructions.
The default gamepack setup copies this bundled pack into
`install/installs/SCGhostPack`; it never downloads this pack.

To stage only this pack without fetching other packs or downloading Windows
build dependencies, run this from the GtkRadiant source directory (Python and
SCons are required by the existing build configuration):

```powershell
python -c "import config; c = config.Config(); c.setup_packs = ['SCGhostPack']; c.FetchGamePaks('install')"
```

`--no-packs` skips all pack staging, including this pack. For a Visual Studio
build, use the staging command above before launching the editor.

1. Launch the modified editor and open **Configure games**.
2. Select **StarCraft Ghost (NOD authoring)**.
3. Set **Game directory** to a separate, writable authoring folder, for example
   `C:\SCGhostMapping`. Do **not** select the extracted `game_files` directory.
4. Optionally set **NOD assets (optional, read-only)** to the original game asset
   root containing `Materials`, not to the `Materials` directory itself.
5. Confirm, then select the new profile in the game selector.
6. Open `C:\SCGhostMapping\nod\maps\test_room.map`.

The game installer copies the pack's `install` contents into the selected
authoring folder and creates `scghost.game` in Radiant's games configuration
directory. Reconfiguring reinstalls the supplied files: save your own maps with
different names and keep backups of edited project/shader/entity files.

## Native DDS and material previews

The optional asset directory is stored as `nod_assetpath` in `scghost.game`.
For an existing Windows profile, close the editor and add this attribute to
`install/scghost.game`, substituting your own path:

```xml
  nod_assetpath="C:\ExtractedGhost\game\StarCraft Ghost"
```

Keep `enginepath_win32` pointing to the separate writable authoring workspace.
The external root is mounted after the authoring roots for VFS reads; startup
does not create directories there, convert textures, or modify its assets.
An invalid asset directory produces an explicit error. No asset-root change is
required in `default_project.proj`.

The profile reads `Materials/shaderlist.txt` and loads the listed `.nsa`
archives directly. Missing archives and unresolved images are reported in the
console. Material categories and raw DDS directories appear in the texture
browser. Level-specific `.nsa` archives not listed in that shader list are not
automatically loaded.

Previews prefer `qer_editorimage`, then the first `texture` or `diffuseMap`
reference, including a texture inside a nested dynamic shader definition.
References accept slash/backslash paths, case-insensitive names, and optional
image extensions. Bare names resolve only when unambiguous; DDS is preferred
over another supported image format with the same path. Masked alpha testing,
two-sided surfaces, and supported editor surface flags are recognized.
Missing previews use the existing missing-image marker.

Materials retain their native names in the editor's `textures/nod/<name>`
namespace. The Quake source-map writer stores them as `nod/<name>`. A future
NOD compiler must translate this namespace back to the original material name;
these source-map tokens are not binary engine material identifiers.

The image module decodes DXT1 through DXT5, including DXT2/DXT4 premultiplied
alpha, and masked 16/24/32-bit RGB DDS with optional alpha and row pitch.
Only the top mip is previewed; cube and volume textures show their first
face/slice. DX10, palettes, and other unsupported encodings are explicitly
rejected. Dimensions are limited to 16384 per axis and 64 Mi pixels.
The preview does not reproduce NOD's multipass shaders, animation, lighting,
detail/glow/environment stages, or full cube/volume rendering.

Windows native testing decoded all 893 supplied DDS files and verified real
material thumbnails, 3D previews, and a native save/reopen round-trip. All 36
faces, material/UV fields, and `ghost` properties were preserved. The 1006 files in the
tested original `Materials` tree remained byte-for-byte unchanged. Native
asset lookup has not been validated on case-sensitive platforms.

## Native decoration props

Run `python dependencies.py` from the GtkRadiant source directory before a
Visual Studio build. Normal SCons `target=setup` also restores these dependencies.
The manifest pins the published `nodsdk-scghost` parser revision and GLM;
only the three model parser sources are compiled into the model module.
The dependency viewer and other SDK tools are not required. Repeated setup
reuses the pinned checkout; a dirty checkout or unexpected origin produces an
error without discarding local changes.

After staging the updated pack, create a `misc_model` entity and set:

- `model`: a VFS-relative path such as `3D/Models/example.nod`.
- `origin`, `angles`: the existing editor placement controls.
- `modelscale` or `modelscale_vec`: uniform or per-axis scaling.
- `_frame`: `0` for NOD, or a zero-based model-entry index for NMB.
- `_remap0`: optionally `textures/nod/old;textures/nod/new`.

### Resizing props

Select the prop, open the entity inspector (`N`), and click **Scale...**.
The existing Selection Scale command and mouse scale mode also support props.
X/Y/Z are multipliers: `2 2 2` doubles the current size; `0.5 0.5 0.5` halves
it. Each prop resizes at its own origin along its local axes, without changing
its placement or rotation. Ordinary brush scaling retains its selection-center
behavior. The dialog's Apply operations can each be undone.
Mouse scaling respects the enabled X/Y/Z axes; keyboard scale nudges and
direct inspector scale edits are also undoable.

For an absolute size, edit a scale key in the entity inspector's property
list, change the **Value** field, and press Enter (including keypad Enter):

- `modelscale`: one number, such as `32` or `48`.
- `modelscale_vec`: three numbers, such as `32 32 64`.

`modelscale_vec` takes precedence. If it is present, changing `modelscale`
does not change the preview; the inspector displays an explanation rather
than silently hiding the edit. Edit the vector instead, or delete it with
**Del Key/Pair** before using uniform scale. The Scale tool writes one active
scale key, removing the other so stale overrides do not hide later edits.
For example, Nova's initial `modelscale_vec = 32 32 32` can be changed directly
to `16 16 16`, or halved with Scale factors `0.5 0.5 0.5`.
Scale keys require finite, nonzero components; malformed values are rejected
with an error dialog instead of producing a collapsed preview.

### Rotating props

Select a prop, open the entity inspector (`N`), and click **Rotate...**.
The existing rotation commands, mouse rotation mode, and keyboard rotation
nudges also update model orientation. Each prop rotates at its own origin;
its position, scale, model path, and NMB entry are unchanged. Ordinary brushes
and other fixed-size entities retain their existing selection-pivot behavior.

The dialog takes degree increments around world X/Y/Z axes, using the same
rotation directions as the brush tools. Already-tilted models use matrix
composition rather than adding independent Euler components. Each Apply is
one undoable operation, including when several axes are entered together.

For absolute orientation, edit `angles` as `pitch yaw roll`, for example
`0 90 0` for a yaw of 90 degrees, and press Enter. `angle` is a yaw-only
alternative that resets pitch and roll. Inspector edits remove the competing
rotation key; the tools write `angles` and remove `angle`, ensuring a saved map
has one unambiguous orientation. Zero angles are valid; malformed or nonfinite
values are rejected with an error dialog. Direct edits are also undoable.

NMB files are model-resource bundles, often named for a particular level.
Selecting an entry previews that model, not all instances or the entire level.
Instance placement remains a separate NSD concern. Empty bundles and empty
entries cannot produce decoration geometry and report an explicit error.

Previews use LOD 0, triangle lists/strips, source XYZ coordinates without the
SDK viewer's Y-up display rotation, native UVs, and the existing native
SDK viewer's Y-up display rotation, native UVs, and the existing native
material namespace. The texture index also includes `3D/Materials` for model
skins, supporting explicit paths and unambiguous bare references.
Native triangle winding is reversed for Radiant's `GL_FRONT` culling convention;
positions, outward normals and UVs remain unchanged. The reversal applies to
triangle lists and strips, including restarted strips.
Tangents and skinning weights are not rendered; skinned
models retain their stored rest-pose geometry. Version-two packed vertex
colours are available in untextured previews. Missing materials retain the
native missing-image marker rather than the legacy `skin.tga` fallback.

Map saving preserves the model reference, entry selector, and transform keys;
it does not embed geometry or alter the original assets. This is an editor
authoring entity, not a new NSD type. Binary model/scene export, animation
playback, and full NIL/NSD scene import are not implemented.

## Included

- `nod/scripts/default_project.proj`: classic brushes, single-player project,
  no compiler commands.
- `nod/scripts/entities.def`: editor `worldspawn` and the observed NSD player
  type `ghost`, plus the editor-only `misc_model` prop. Their boxes are
  authoring aids, not engine collision specifications.
- `nod/scripts/nod.shader`: explicit placeholder using Radiant's bundled
  missing-texture image, not an extracted or converted NOD material.
- `nod/maps/test_room.map`: six outward-wound wall/floor/ceiling brushes around
  a 512 x 512 x 256 interior, with one `ghost` at `(0, 0, 64)`.

Patches are disabled. Engine executable attributes are empty so Quake is not
used as a stand-in. The Build menu has no compiler entries by design.
Opening a map positions the camera at `ghost` using its source `angles`.
Region saves use a `ghost` start point rather than inserting a Quake
`info_player_start`. Other game profiles retain their existing behavior.

### Windows build status

The Release/Win32 editor and required profile modules build with Visual Studio
2022 and the installed Windows SDK. The unused shell headers that collided with
Radiant's `DeleteKey` macro have been removed from the crash-dump implementation.
See the [Windows editor build instructions](../../README.md#windows-editor-build-visual-studio-2022).

Native startup, profile loading, and a save/reopen round-trip have been tested.
The room renders in the editor with six brushes and one `ghost` entity. Native
saving preserves all 36 brush planes, material/UV flags, and entity properties.
BSPC's legacy `v141_xp` toolset is not required by the editor-only build; it is
still required if building the full solution with the fetched BSPC project.

One shutdown test emitted GTK invalid-widget warnings and did not finish
normally; that test instance was terminated after the map had been saved.
This remaining legacy GTK runtime issue is separate from the resolved compiler
failure. Keep backups of source maps while this experimental profile is tested.

## What is not implemented

- `.nil` geometry, collision, sectors, portals or post-sector data export.
- `.nsd` player/entity serialization or `.npd` AI navigation export.
- Full NOD material rendering or animated model previews.
- Lightmaps, engine boot validation, or additional entity types.

Editor `origin` and `angles` are source conventions. The compiler must define
and verify conversion from the editor's coordinate system to NOD's documented
left-handed Z-up system; do not assume identical winding or angle semantics.
The SDK identifies NSD as the authoritative entity placement source and NPD as
navigation data, not spawn placement.

## Validation

`regression_tests/model_transforms.cpp` tests scale scalar/vector precedence,
per-axis factors, finite/nonzero checks, and exact key round-trip. Its rotation
checks compare reconstructed orientation matrices against the legacy world-axis
rotation directions, including tilted models and positive/negative gimbal lock,
with a maximum per-component error of `0.00003`. Build it with `libs` on the
include path and link the editor's `mathlib` library using the same architecture
and runtime. It has no SDK or GTK dependencies.

`regression_tests/nod_models.cpp` is the native SDK-to-PicoModel regression
driver. Build it with the same Win32 MSVC runtime as the editor (`/MD`),
including `libs`, `plugins/model`, `libs/glm`, and the SDK parser headers.
Compile it with `plugins/model/nodmodel.cpp` and the SDK's `AssetParser.cpp`,
`NODParser.cpp`, and `NMBParser.cpp`, and link `build/Release/libs/picomodel.lib`.
Without arguments it checks all six layouts, base offsets, strip restart
winding, native UV/material names, error paths, both bundle versions, entry
selection, and packed colours. A model path or asset directory argument runs
read-only real-asset conversion, including every entry of each bundle.

From the GtkRadiant source directory:

```powershell
python -m unittest discover -s regression_tests -p test_scghost_pack.py -v
```

These checks cover pack staging, project/module configuration and fixture
geometry. They do not replace opening, editing, saving and reopening the map
in a built editor, or validating future binary exports in the game.

For native DDS pixel, malformed-input, and material texture-index tests, build
the standalone driver from an x64 Visual Studio developer PowerShell. Keep its
outputs outside the source tree:

```powershell
$sourceRoot = (Get-Location).Path
$testDir = Join-Path $env:TEMP "scghost-dds-tests"
New-Item -ItemType Directory -Force $testDir | Out-Null
Push-Location $testDir
try {
    cl /nologo /EHsc /std:c++14 "/I$sourceRoot\libs" `
        "$sourceRoot\regression_tests\dds_decode.cpp" `
        "$sourceRoot\libs\ddslib\ddsimage.cpp" `
        "$sourceRoot\libs\ddslib\ddslib.c" /Fe:dds_decode.exe
    if ($LASTEXITCODE -ne 0) { throw "Native DDS test driver build failed" }
} finally {
    Pop-Location
}
$env:DDS_TEST_DECODER = Join-Path $testDir "dds_decode.exe"
python -m unittest config.TestConfigParse regression_tests.test_scghost_pack regression_tests.test_dds_native -v
```

Without `DDS_TEST_DECODER`, the native suite explicitly skips. The synthetic
tests require no proprietary assets; the x64 driver also exercises the decoder's
independence from native pointer size, although the tested editor is Win32.
