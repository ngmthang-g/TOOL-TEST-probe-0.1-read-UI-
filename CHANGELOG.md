# Changelog

## [0.1.2-probe] - 2026-09-11

- Thay F8/direct target resolver từ `RectangleContainsScreenPoint(camera=null)` sang `EventSystem.current.RaycastAll`.
- Map `RaycastResult.gameObject` qua Transform parent về live `UIObject.instances` rồi chọn callable target.
- `TEST DIRECT` re-raycast/re-resolve target trước mỗi dispatch; không cache pointer UI cũ.
- Loại bỏ `TEST BAG SEMANTIC` khỏi protocol/UI vì live test gây bridge timeout/game diss.
- Gỡ nút test InputSync khỏi UI; InputSync donor chỉ còn baseline/reference nội bộ.
- Protocol bump `0x00010200` để EXE/DLL v0.1.2 không trộn với bản cũ.
- Fix compiler: bổ sung resolver `il2cpp_object_new` dùng cho EventSystem `PointerEventData/List` construction.
- Windows/MSVC x64 final PASS — run `34621857069`: configure, Release build, native CTest, artifact upload và `dist/` publication đều thành công.
- Binary publication commit: `5b2e135cd5d850d4b78473f1f25588fd5f47a5b8`.
- Runtime status: `EVENTSYSTEM_DIRECT_RETEST_REQUIRED`.

## [0.1.1-probe] - 2026-09-11

### Runtime evidence from v0.1.0
- `SCAN ACTIVE UI`: PASS on real game (`total=221`, `rows=160`, truncated).
- F8: FAIL with both `cursor outside client area` and `no callable control at F8`.

### Fixed
- Enable per-monitor-v2 DPI awareness before creating the controller window.
- F8 geometry hit-test now considers visual UI objects, not only direct-callable controls.
- Resolve nearest callable parent for direct action; fall back to callable overlap with ambiguity fail-closed.
- Preserve valid F8 point even when visual identity lookup fails so InputSync can test the client EventSystem raycast.
- InputSync no longer requires direct-control discovery before calling `TryClickUI -> EndUIDrag`.
- Add point/Unity hit-count diagnostics.

### Status
- Local source contract: PASS.
- Local native logic test: PASS.
- Windows CI: PASS — run `34614089454`: source-contract, CMake x64 configure, MSVC Release build, native CTest, artifact upload and dist publication all succeeded.
- Runtime: SCAN PASS; F8 RETEST REQUIRED.

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
- Windows CI: PASS — source contract, CMake x64 configure, MSVC Release build, native CTest, artifact upload and `dist/` publication all succeeded.
- Build workflow run: `34611065539`; binary publication commit: `858b2a4d9b7b0dc2fba5d2f1b8bb58c07a882d28`.

### Runtime
- RUNTIME UNTESTED.
