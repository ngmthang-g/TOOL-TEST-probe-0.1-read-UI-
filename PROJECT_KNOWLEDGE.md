# Project Knowledge — ThanLong UI Internal Probe

## Current

- Version: `0.1.4-probe`.
- Repo: `ngmthang-g/TOOL-TEST-probe-0.1-read-UI-`.
- Scope: runtime UI discovery + EventSystem target mapping + direct callback invocation only.
- Build: **PASS** on Windows/MSVC x64. Verified run: `34669951178`.
- Runtime inherited: **SCAN PASS** on client thật; F8 remains selection-only.
- Runtime for the three named targets remains `NAMED_TARGETS_UNTESTED` until live evidence exists.

## v0.1.4 exact named-target contract

1. `MỞ TAY NẢI` uses the exact DATA-222 identity `ButBag` + `ButBagClick`.
2. `CHUYỂN → SKILL` and `CHUYỂN → TAY NẢI` both resolve the same physical SkillBar button: `ButtonOriginalSwitchSite` + `ButtonOriginalSwitchSiteClicked`.
3. Direction is guarded by fresh live state from `ToggleFirstTab` / `ToggleSecondTab` using `UIToggle.get_Selected`.
4. If already in requested state, return `TargetAlreadyInState` and perform no callback.
5. Missing exact target, duplicate exact target, or unreadable switch state fails closed; no mutation occurs.
6. `NHẬN DIỆN` is read-only.
7. `TEST DIRECT TARGET` re-enumerates and re-resolves both target and switch state on every request; no live UI pointer is cached across requests.
8. Direct dispatch uses the existing `InvokeControl` path, not Bag Semantic or Windows mouse input.

## Generic F8 resolver contract

1. Controller captures DPI-correct client point with F8.
2. Bridge builds Unity screen point.
3. EventSystem live pointer/raycast data maps current GameObject hits to fresh `UIObject.instances`.
4. F8 never dispatches action.
5. `TEST DIRECT` repeats target resolution and invokes only the current live target.
6. Ties fail closed.

## Protocol

Visible workflow uses `ScanUi`, `PickAtPoint`, `DirectInvokeAtPoint`, `RecognizeTarget`, `DirectInvokeTarget`. `InputSyncClickAtPoint` remains source-reference compatibility only and has no controller button. No semantic bag command exists.

## Verification evidence

Run `34669951178` completed successfully with:
- Source contract: PASS.
- CMake x64 configure: PASS.
- MSVC Release build: PASS.
- Native CTest: PASS.
- Runtime/source package staging: PASS.
- Artifact upload: PASS.
- `dist/` publication: PASS.

## Runtime acceptance

Build/CI success is not runtime proof. Live acceptance requires the client to show the intended transition for each named target, with no wrong-direction toggle and no game diss/timeout.
