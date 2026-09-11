# Project Knowledge — ThanLong UI Internal Probe

## Current

- Version: `0.1.2-probe`
- Repo: `ngmthang-g/TOOL-TEST-probe-0.1-read-UI-`
- Scope: runtime UI discovery + EventSystem target mapping + direct callback invocation only.
- Build: **PASS** on Windows/MSVC x64. Final successful build run: `34621857069`.
- Live evidence inherited from v0.1.1: scan PASS; F8 coordinate conversion PASS; old RectTransform geometry resolver failed with `hits=0`; donor InputSync works but is not a v0.1.2 test objective.
- `TEST BAG SEMANTIC` is retired after live bridge timeout/game diss.
- Runtime remains `EVENTSYSTEM_DIRECT_RETEST_REQUIRED` until live F8 + Direct evidence is collected.

## v0.1.2 resolver contract

1. Controller captures DPI-correct client point with F8.
2. Bridge builds Unity screen point.
3. `EventSystem.current.RaycastAll(PointerEventData, List<RaycastResult>)` obtains actual Unity raycast GameObjects.
4. Probe enumerates fresh `UIObject.instances`, resolves each UI object's GameObject, and maps raycast GameObject/Transform ancestors to those live objects.
5. Prefer direct-callable target by raycast order then nearest ancestor distance; ties fail closed.
6. F8 never dispatches action.
7. `TEST DIRECT` repeats the raycast/map and invokes only the fresh target:
   - UIButton -> `HandleClickEvent()`
   - UIToggle -> selected/select callback
   - UIRectTransform -> Lua PointerClickHandler
8. Never persist a UI pointer between scans/transitions.

## Protocol

Visible workflow uses `ScanUi`, `PickAtPoint`, `DirectInvokeAtPoint`. `InputSyncClickAtPoint` remains internal source compatibility/reference only and has no controller button. No semantic bag command exists.

## Verification evidence

- Source contract tests: PASS on Windows runner.
- CMake x64 configure: PASS.
- MSVC Release build: PASS.
- Native CTest: PASS.
- Artifact upload: PASS.
- `dist/` publication: PASS.
- Final successful workflow run: `34621857069`.

## Runtime acceptance for this iteration

A successful live target test should show:
- F8: `raycastHits > 0`, `mapped > 0`, and preferably `callableMapped > 0` with correct target identity.
- Direct: `DIRECT DISPATCH PASS • EventSystem re-raycast -> UIObject -> callback` and the intended UI transition occurs.

Build/CI success alone is not runtime proof.
