# Changelog

Notable changes to this fork, following [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Derived from upstream [GtkRadiant 1.6.7](https://github.com/TTimo/GtkRadiant).

## 0.6.8-preview - T.B.D

### Fixed

- Fit classic SC Ghost MAP textures from the actual face projection at any rotation, avoiding collapsed scales at 90 degrees, preserving mirrored axes and retaining fractional shifts on meter-scale brushes.
- Preserve full brush-primitive UV mappings when rotating textures in the SC Ghost Surface Inspector or through rotation shortcuts, retaining shear, mirroring and relative scale instead of rebuilding a lossy shift/scale/rotation approximation.
- Keep Surface Inspector commit/undo snapshots normalized to the same texture size across repeated edits. Preserve fitted mappings while keeping axial/reset/captured-UV presets explicit replacements.
- Preserve fractional fixed-size entity origins when dragging, transforming and snapping to the grid, keeping saved MAP keys synchronized with viewport positions.
- Draw fine grids using indexed coordinates instead of accumulated float steps, avoiding drift at the new 0.1-unit spacing.
- Convert editor pitch/yaw/roll to native ZXY degree angles when initializing owner-oriented mover frames.
- Retain and validate SDK `allow-empty` resource metadata so optional native portrait/shader fields and required resource fields follow their declared constraints.
- Preserve fractional-meter world brushes in the SC Ghost profile instead of
  rounding their planes to whole units or removing them as tiny brushes on load.
  Use the native thin-brush clipping tolerance for these brushes as well.
- Keep SC Ghost shader preview flags out of saved MAP surface flags when applying
  materials or creating brushes, preventing masked/culling previews from causing
  `MAPtoNIL` authoring-flag failures.

### Added

- Automatically preview a resolvable worldspawn `nod_sky_model` behind geometry in the SC Ghost textured camera, using the existing native model resolver and per-map bundle. Keep the sky camera-centred and editor-only; NAD animation and native sky-surface masking are not previewed.
- Isolated Windows Release packaging from explicit editor, SDK and runtime inputs, without requiring game assets or copying personal profiles/user maps. Validate PE architectures, nondebug runtime dependencies and ZIP integrity; include redistributables, license notices, release metadata and first-launch instructions.
- Grid0.1 with menu and previous/next-grid navigation, plus an orthographic zoom limit of 64 pixels/unit for meter-scale detail work.
- Source-backed export with matching `.nodsrc` selection, preserving imported NIL geometry/lighting while applying supported NSD/NPD edits; disable incompatible rebuild options.
- Optional `.nods` selection in level export, persisted through the map's script reference, with staged source snapshots and `NODScript` expansion before native compilation.
- Imported waypoint connections through Ctrl+K, respecting native record indices rather than treating them as newly authored waypoint IDs.
- Native console model previews and selectable interactive NOT presets, plus managed-spawn actor choices derived from the active NOT catalog.
- SDK-driven native handler selection and initialization in the N panel, with typed fields, owner-compatible choices, model-frame/region setup, Ctrl+K trigger connections and undo/redo.
- Support for 31 SDK-published gameplay workflows, including effects/particles, advanced translation/rotation, collision/visibility, shader-frame selection, removal, physics activation, localized help, popup conversations and mission/level transitions.
- Native resource/key field validation with explicit optionality, plus SDK-provided fixed-field and connection restrictions; removal/exemption/container references are not treated as trigger outputs.
- Gameplay authoring guidance and supported trigger semantics, including payloads, delayed/one-shot behavior and no-restart limitations. Native callback/editor conformance does not establish live rendering, physics, UI/audio or successful mission transitions.
- Per-map scene environment dialog for SDK worldspawn music, ambient loop, sky,
  linear fog, far clip and native reverb settings, with validation and undo/redo.
- Optional automatic physics-margin override in NOD level export, retaining the
  SDK's one-unit default when disabled.
- Authoring guidance for environment limits and scriptless startup/collision risks.

### Changed

- Consume SDK version-2 behavior metadata instead of hardcoding native handler layouts; retain version-1 catalog support for ordinary template metadata.
- Clarify unmasked sky, native music-reference and ambient-cue behavior in scene environment controls.
- Refresh application icons, logo and custom-build about text.
- Advance the SDK source dependency from `004c7e1` to `5bd1c0e` for base authoring/compiler fixes. New native gameplay features require matching current SDK binaries; updating the pinned source dependency does not rebuild or replace external executables.

## 0.6.7-preview - 2026-10-24

### Added

- Installer-ready Windows staging folder and ZIP via `build.sh --package`,
  including SDK tools, runtime dependencies, and license notices.
- SC Ghost profile, project, entity definitions, and test-room MAP.
- Live NOT template placement, effective properties, and instance overrides.
- NOD project paths, layers, view options, and surface tools.
- Native DDS/NSA material previews and NOD/NMB props with scale/rotation controls.
- SDK scene/template editor integration and NIL/NSD/NPD level conversion.
- Windows Git Bash/Linux build scripts with SC Ghost setup and NOT catalog validation.
- MSBuild/SCons cleanup that preserves dependencies, profiles, preferences, and maps.
- Pinned SDK and GLM source dependencies.

### Changed

- Aligned the editor version with OpenNOD SDK 0.6.7.
- Kept shared NOT definitions read-only in Radiant; shared edits use NOT Editor.
- Replaced upstream documentation with fork-specific setup, usage, scope, and licensing.

### Fixed

- Surface shader-path handling.
