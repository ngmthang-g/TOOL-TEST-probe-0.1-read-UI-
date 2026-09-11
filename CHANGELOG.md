# CHANGELOG

## [0.1.0-probe] - 2026-09-11

### Requested
- Build a mini probe from the v9.9 UI base to discover/call unnamed game UI controls such as close X and bag icon.
- Remove unrelated automation and publish source + Windows executable through the dedicated GitHub repository.

### Added
- Active `UIObject.instances` scanner with semantic labels, hierarchy, handler, depth and geometry.
- F8 non-mutating control picker with ambiguity fail-close.
- Direct `UIButton` / `UIToggle` / Lua pointer-handler test.
- InputSync `TryClickUI -> EndUIDrag` test with drag cleanup guards.
- Semantic bag test using `RoleInfo_BagTab`.
- Before/after UI identity evidence.
- Windows build workflow and `dist/` publication step.

### Build
- Local platform-neutral tests: PASS.
- Windows CI: PENDING.

### Runtime
- RUNTIME UNTESTED.
