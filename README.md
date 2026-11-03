# OpenNOD Radiant

![logo](install/bitmaps/logo.png)

GtkRadiant fork for StarCraft: Ghost / NOD map restoration and authoring.
Supports brush MAP editing, live NOT entity placement and instance properties,
DDS/NSA material previews, NOD/NMB props, and SDK-backed level conversion.

## Requirements

- Git, Bash, Python 3.9+.
- **Windows:** Git Bash, Visual Studio 2022 C++ workload, Windows SDK.
  The editor builds as **Win32**; an external x64 SDK is supported.
- **Linux:** C/C++ compiler, pkg-config, SCons, and development packages for
  GTK2, GtkGLExt, libxml2, JPEG, PNG, zlib, and OpenGL.
- [OpenNOD SDK](https://github.com/aknavj/nodsdk-scghost) binaries:
  `NODTemplates`, `NOTEditor`, `NSDEditor` (`.exe` on Windows).
- Extracted game assets; the asset root must contain `Materials`.
  Original assets remain read-only and are not included.

## Build and run

From the repository root:

```sh
bash build.sh \
  --sdk-dir "/path/to/OpenNOD-SDK" \
  --asset-root "/path/to/game_files" \
  --workspace "/path/to/authoring"

cd install
./radiant.exe  # Windows; use ./radiant.bin on Linux
```

Windows paths such as `'C:\SDK\OpenNOD-SDK-0.6.7-windows-x64'` are accepted.

| Option | Environment variable | Default |
| --- | --- | --- |
| `--sdk-dir DIR` | `NOD_SDK_DIR` | Existing `install/scghost.game`, then a single sibling SDK package. |
| `--asset-root DIR` | `NOD_ASSET_ROOT` | Existing `install/scghost.game`. |
| `--workspace DIR` | `NOD_WORKSPACE` | Existing profile workspace, then `install/workspace`. |
| `--configuration Release\|Debug` | - | Release |
| `--jobs N` | - | CPU count |

The root [build_scghost.py](build_scghost.py) helper restores dependencies,
builds the editor and required modules into `install`, configures the SC Ghost
profile, and verifies NOT templates. BSPC and Quake map compilers are not built.

## Distribution build (Windows)

```sh
bash build.sh --package --sdk-dir 'C:\SDK\OpenNOD-SDK-0.6.7-windows-x64'
```

Creates `dist/OpenNOD-Radiant-0.6.7-windows-x64/` and its ZIP for an installer
or portable distribution. Requires Release configuration, but no game assets.
Includes SDK editors/converters, GTK and MSVC runtimes, bundled profile, and
license notices. Excludes personal configuration, user maps, assets, and debug files.
Move/remove existing release outputs before repackaging.

Install to a writable per-user folder and set the shortcut's working directory
to it. First launch uses Configure games to select the workspace and asset root.
Publish corresponding source for the editor, SDK, and GPL/LGPL dependencies
alongside binary releases; packaging alone does not fulfill source obligations.

## Usage

- Open `<workspace>/nod/maps/test_room.map` or create a new MAP.
- **Grid > Grid0.1:** snap and nudge in 0.1-unit steps (10 cm in meter-scale NOD
  maps). `[` / `]` step between 0.1, 0.25, 0.5 and the existing larger grids.
  Zoom in to see the fine grid; orthographic views now support up to 64 pixels/unit.
  Entity dragging, nudging, transforms and snap-to-grid preserve fractional
  origins in both the viewport and saved MAP keys.
- **N > Entities > Place entity:** browse and place NOT templates.
- **File > Project Settings:** configure asset and SDK paths. Missing paths
  can leave the placement list empty; restart after changing the asset root.
- **Bsp > Convert NOD level...:** requires same-build SDK executables
  `MAPtoNIL`, `MAPtoNSD`, and `MAPtoNPD`.
- Radiant edits instance overrides; shared NOT edits belong in NOT Editor.
- **Entities > Map resources and metadata > Scene environment...:** per-map
  sky/audio references, linear fog, far clip and reverb preset.
- NOD export offers an optional **Physics margin** override; disabled retains
  the SDK's one-unit automatic padding. This is not a player-spawn/collision fix.

Environment export, physics-margin overrides, DDS-backed NSA material compilation
and improved diagnostics require SDK binaries built from revision `5bd1c0e`
or newer (the unreleased 0.6.8 changes). The pinned source dependency is used by
the editor's native model integration; it does **not** rebuild or replace external
SDK executables. Rebuild the SDK and select its output folder in Project Settings.
See the [profile workflow](gamepacks/SCGhostPack/README.md#scene-environment)
for native limits and startup caveats.

## Clean

From the repository root:

```sh
bash clean.sh
```

Close the editor first. Cleanup uses MSBuild/SCons to remove compiler outputs
for the selected configuration. Dependencies, profiles, preferences, maps, and
original assets are kept. Use `--configuration Debug` to clean a Debug build.
Do not build and clean concurrently.

## Status and documentation

Experimental; not the original editor or complete NOD engine support.
Windows build/startup and clean/rebuild are verified. Native Linux compilation
remains unverified. Keep map backups and validate exports in a disposable runtime.

- [StarCraft Ghost workflow](gamepacks/SCGhostPack/README.md)
- [Changelog](CHANGELOG.md)
- [Issues](https://github.com/aknavj/nodsdk-radiant/issues)
- Based on [GtkRadiant](https://github.com/TTimo/GtkRadiant); fork version
  **0.6.7**, aligned with OpenNOD SDK.

## Project scope and license

Tools for StarCraft: Ghost map restoration and preservation, not a game remake.
No proprietary game assets are included; their rights and licensing remain separate.

Based on GtkRadiant (**GPL-2.0-or-later**), with **GPL-3.0** fork contributions.
Some components use BSD or LGPL licenses. Preserve original copyright and license
notices. See [LICENSE](LICENSE), [GPLv2](GPL), [GPLv3](GPL-3.0), [LGPL](LGPL),
[BSD](BSD), and source headers.
GPL permits commercial use subject to its terms.

---
This project uses AI-assisted development with collaborative human/AI oversight
and human review of output.