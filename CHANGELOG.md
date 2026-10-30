# Changelog

Notable changes to this fork, following [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Derived from upstream [GtkRadiant 1.6.7](https://github.com/TTimo/GtkRadiant).

## 0.6.8-preview - T.B.D

### Fixed

- Preserve fractional-meter world brushes in the SC Ghost profile instead of
  rounding their planes to whole units or removing them as tiny brushes on load.
  Use the native thin-brush clipping tolerance for these brushes as well.
- Keep SC Ghost shader preview flags out of saved MAP surface flags when applying
  materials or creating brushes, preventing masked/culling previews from causing
  `MAPtoNIL` authoring-flag failures.

### Added

- Per-map scene environment dialog for SDK worldspawn music, ambient loop, sky,
  linear fog, far clip and native reverb settings, with validation and undo/redo.
- Optional automatic physics-margin override in NOD level export, retaining the
  SDK's one-unit default when disabled.
- Authoring guidance for environment limits and scriptless startup/collision risks.

### Changed

- Pin the SDK source dependency to `5bd1c0e` for the latest authoring and compiler
  fixes. External SDK binaries must be rebuilt from this revision or newer;
  updating the editor's source dependency does not replace them.

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
