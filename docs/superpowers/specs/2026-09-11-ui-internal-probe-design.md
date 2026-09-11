# Thần Long UI Internal Probe v0.1 — Design

## Goal
Create a minimal Windows x64 probe derived from the proven v9.9 runtime bridge. It must discover currently active Thần Long UI objects even when they have no visible text, select the control under the mouse without clicking it, invoke the same control through two independent internal paths, and record before/after UI evidence.

## Scope
The probe keeps only game-window discovery, bridge attachment, UI discovery, point hit-testing, direct callback invocation, InputSync invocation, semantic bag-open validation, and logs. It does not include automation, trade, sell, treatment, train, Telegram, configuration import/export, licensing, item filtering, macros, or background workflows.

## Runtime architecture
`ProbeController.exe` enumerates visible Thần Long windows by checking for `GameAssembly.dll`. It creates one PID-scoped shared-memory block and installs `ProbeBridge.dll` into the selected game's UI thread using the proven `WH_GETMESSAGE` hook pattern from v9.9.

`ProbeBridge.dll` executes requests only when the hook callback runs on the target game window thread. The bridge resolves IL2CPP metadata by declaring type + method signature at runtime rather than storing live UIButton pointers across UI transitions.

The bridge exposes exactly five mutable/read commands:

1. `ScanUi` — enumerate active `UIObject.instances`, gather class/name/text/tag/handler/ancestors/descendants, click capabilities, depth, and geometry area.
2. `PickAtPoint` — hit-test the current UI at a normalized point and return exactly one best candidate; equal-rank candidates fail closed.
3. `DirectInvokeAtPoint` — re-scan and re-hit-test at action time, then invoke `UIButton.HandleClickEvent`, `UIToggle` select callback, or `UIRectTransform.PointerClickHandler` Lua callback.
4. `InputSyncClickAtPoint` — call `InputSyncManager.TryClickUI(Left, point)` followed by `EndUIDrag(point)`, refusing to overlap an existing internal drag and cancelling owned drag state on failure.
5. `SemanticOpenBag` — use `LuaSystemAPI_GUI.MainCallUI/CallUI` for `RoleInfo` then `RoleInfo_BagTab`; a separate verify mode checks `FindUI/MainFindUI("RoleInfo_BagTab")`.

## UI
The controller contains only:
- client combo box + Refresh/Attach;
- `SCAN ACTIVE UI` button;
- status line showing selected PID and bridge state;
- list view with columns `#`, `Kind/Class`, `Name`, `Text`, `Tag`, `Handler`, `Ancestors`, `Descendants`, `Depth`, `Area`, `Direct`;
- selected-object detail box;
- buttons `TEST DIRECT`, `TEST INPUTSYNC`, `TEST BAG SEMANTIC`;
- scrolling log.

F8 registers a global hotkey while the probe window exists. F8 only captures the mouse position, converts it to the selected game client area, normalizes it to scale 100000, and issues `PickAtPoint`. F8 itself never sends a mouse click.

## Candidate selection
Only active controls with usable geometry participate in point picking. Controls are ranked by:
1. smallest positive RectTransform local area;
2. deepest UI parent depth;
3. stable runtime identity only as deterministic ordering.

If the top two candidates have effectively equal area and equal depth but are different objects, the probe reports `AMBIGUOUS` and performs no mutation.

## Pointer lifetime rule
The controller stores only descriptive identity and the normalized point. It never stores or reuses an `Il2CppObject*`. `TEST DIRECT` resolves the current candidate again from the point immediately before invocation.

## Evidence model
Before each test action the controller runs `ScanUi` and builds an identity set. After dispatch, the controller schedules a fresh scan and reports added/removed identities and whether the picked identity is still present. Observation delay is not treated as success; logs label it as evidence only.

`TEST DIRECT` success means callback dispatch returned without managed exception plus the after-snapshot evidence. `TEST INPUTSYNC` success means TryClickUI accepted a UI drag lifecycle and EndUIDrag cleared it, plus after-snapshot evidence. `TEST BAG SEMANTIC` additionally performs semantic verify of `RoleInfo_BagTab` after dispatch.

## Safety / failure behavior
- No cached UI pointers.
- No mutation when bridge callback thread differs from target window thread.
- No mutation on ambiguous hit-test.
- No overlapping InputSync drag.
- On InputSync exception/unfinished drag, call `CancelUIDragState` and report failure.
- All shared-memory requests are single-flight; timed-out requests cannot be overwritten by a newer request until the old sequence completes.
- Maximum scan rows: 160; if more active UI objects exist, response reports truncation explicitly.

## Build
Windows x64, C++17, Win32/ComCtl32, CMake. GitHub Actions runs unit/contract tests, configures MSVC x64 Release, builds `ProbeController.exe` and `ProbeBridge.dll`, and uploads both with source metadata as artifact `ThanLong-UI-Internal-Probe-v0.1-win-x64`.

## Runtime status labels
Build success is `BUILD PASS`, not runtime proof. Until tested inside the user's game client, the release is `RUNTIME UNTESTED`. Runtime success for a target control is recorded separately as `DIRECT PASS`, `INPUTSYNC PASS`, or `SEMANTIC PASS` only from live evidence.
