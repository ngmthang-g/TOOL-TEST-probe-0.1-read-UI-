# Thần Long UI Internal Probe v0.1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a minimal v9.9-derived Windows probe that discovers unnamed active UI, picks the exact control under F8, tests direct callback versus InputSync, and records UI snapshot evidence.

**Architecture:** A minimal Win32 controller attaches a minimal IL2CPP bridge to the selected Thần Long UI thread through the proven v9.9 WH_GETMESSAGE/shared-memory pattern. The bridge resolves current UI objects per request and never exposes live object pointers to the controller. Pure ranking/snapshot logic lives in a platform-neutral header with native unit tests.

**Tech Stack:** C++17, Win32, Common Controls, IL2CPP runtime exports, CMake, MSVC/GitHub Actions, Python source-contract tests.

**Spec:** `docs/superpowers/specs/2026-09-11-ui-internal-probe-design.md`

## Global Constraints
- Windows x64 only.
- F8 selects but never clicks.
- No stale `Il2CppObject*` may cross requests.
- Direct action must re-resolve the control immediately before mutation.
- InputSync path is `TryClickUI -> EndUIDrag`, with drag-state cleanup on failure.
- Exactly one outstanding bridge request per PID.
- No automation/trade/sell/treatment/train/Telegram/license feature in the mini controller or protocol.
- A build/test pass is not labeled runtime pass.

---

### Task 1: Platform-neutral probe selection/evidence logic

**Files:**
- Create: `src/probe_logic.h`
- Create: `tests/probe_logic_test.cpp`

**Interfaces:**
- Produces: `probe_logic::ChooseHit(std::vector<HitRank>) -> PickResult`
- Produces: `probe_logic::DiffIdentities(before, after) -> SnapshotDiff`
- Produces: `probe_logic::Fnv1a64(std::wstring_view) -> uint64_t`

- [ ] Write tests for smallest-area selection, deeper-node tie break, equal-rank ambiguity, and added/removed snapshot evidence.
- [ ] Compile before creating `probe_logic.h`; verify expected missing-header failure.
- [ ] Implement only the functions required by those tests.
- [ ] Compile and run the test; require all assertions to pass.

### Task 2: Probe-only IPC contract

**Files:**
- Create: `src/probe_protocol.h`
- Create: `tests/test_source_contract.py`

**Interfaces:**
- Produces commands `ScanUi`, `PickAtPoint`, `DirectInvokeAtPoint`, `InputSyncClickAtPoint`, `SemanticOpenBag`.
- Produces fixed-size `UiRow`, `UiSnapshot`, `ProbeRequest`, `ProbeResponse`, `SharedBlock`.

- [ ] Write source-contract tests that require the five commands and reject legacy automation command names.
- [ ] Run contract test before protocol creation and verify failure.
- [ ] Implement compact protocol with 160-row maximum and mapping helper.
- [ ] Re-run contract test and require pass.

### Task 3: Minimal IL2CPP UI bridge

**Files:**
- Create: `src/probe_bridge.cpp`
- Update: `tests/test_source_contract.py`

**Interfaces:**
- Consumes normalized coordinates in `[0, 99999]`.
- Produces scan/pick/action responses through `SharedBlock`.

- [ ] Extend contract test to require `UIObject.instances`, `HandleClickEvent`, `PointerClickHandler`, `TryClickUI`, `EndUIDrag`, `CancelUIDragState`, `MainCallUI`, `RoleInfo_BagTab`, and explicit ambiguous fail-closed logic.
- [ ] Run contract test and verify it fails because bridge is absent.
- [ ] Port only the v9.9 IL2CPP resolver/UI discovery/geometry/InputSync/Lua execution donors needed by the spec.
- [ ] Implement snapshot row serialization, point picking via `RectangleContainsScreenPoint`, direct re-resolution, InputSync lifecycle, semantic bag action/verify, shared-memory hook and single-flight processing.
- [ ] Run contract tests and compile syntax in the Windows CI build.

### Task 4: Minimal controller UI

**Files:**
- Create: `src/probe_controller.cpp`
- Update: `tests/test_source_contract.py`

**Interfaces:**
- Consumes `ProbeResponse` rows and action evidence.
- Provides Refresh/Attach/Scan/F8/Direct/InputSync/Bag controls.

- [ ] Extend contract test to require `RegisterHotKey(...VK_F8)`, `PickAtPoint`, buttons `TEST DIRECT`, `TEST INPUTSYNC`, `TEST BAG SEMANTIC`, and to reject legacy feature labels.
- [ ] Run contract test and verify failure because controller is absent.
- [ ] Implement game discovery, bridge attach/single-flight calls, normalized cursor conversion, list view, selected detail, pre/post snapshot diff, timers for evidence rescan, and semantic bag verify.
- [ ] Run contract tests.

### Task 5: Build system, docs and CI artifact

**Files:**
- Create: `CMakeLists.txt`
- Create: `README.md`
- Create: `VERSION.txt`
- Create: `.github/workflows/build-windows.yml`

**Interfaces:**
- Produces `ProbeController.exe` and `ProbeBridge.dll` in the same output directory.

- [ ] Add source-contract checks for target names and workflow artifact name, then verify red.
- [ ] Implement CMake with `ProbeController`, `ProbeBridge`, and `probe_logic_test` targets; link `comctl32` for controller.
- [ ] Add GitHub Actions steps: Python contract test, C++ unit test, MSVC x64 Release build, upload artifact.
- [ ] Document runtime usage, F8 semantics, result labels, and `RUNTIME UNTESTED` status.
- [ ] Run all local platform-neutral tests.

### Task 6: Repository publication and verification

**Files:** all project files above.

- [ ] Publish text source to `ngmthang-g/TOOL-TEST-probe-0.1-read-UI-` main.
- [ ] Inspect the triggered GitHub Actions run and fix compile/test failures from logs until green.
- [ ] Download the successful workflow artifact.
- [ ] Verify artifact contains `ProbeController.exe` and `ProbeBridge.dll`.
- [ ] Produce a source ZIP locally and report commit/build/runtime status without claiming runtime pass.
