# Changelog

Notable changes to this fork, following [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Derived from upstream [GtkRadiant 1.6.7](https://github.com/TTimo/GtkRadiant).

## 0.6.7-preview - 2026-10-24

### Added

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
