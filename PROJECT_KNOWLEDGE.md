# PROJECT KNOWLEDGE — ThanLong UI Internal Probe

## Project identity
- Version: 0.1.1-probe
- Platform: Windows x64
- Purpose: runtime discovery and controlled internal activation tests for unnamed Thần Long UI controls.
- Donor: v9.9 V20 controller/bridge architecture, intentionally stripped to probe-only scope.

## Current state
- Source contract tests: PASS locally.
- Platform-neutral hit ranking/snapshot diff test: PASS locally.
- Windows MSVC build: v0.1.0 BUILD PASS (`34611065539`); v0.1.1 BUILD PENDING.
- Published binaries: `dist/ProbeController.exe`, `dist/ProbeBridge.dll`, `dist/ThanLong-UI-Internal-Probe-v0.1-win-x64.zip`.
- Binary publication commit: `858b2a4d9b7b0dc2fba5d2f1b8bb58c07a882d28`.
- Runtime on real game: SCAN PASS; v0.1.0 F8 failed; v0.1.1 F8 retest required.

## Architecture
`Controller -> shared memory -> WH_GETMESSAGE game thread hook -> ProbeBridge -> current UI re-scan -> one probe action -> fresh snapshot / semantic verify`.

Five commands only: `ScanUi`, `PickAtPoint`, `DirectInvokeAtPoint`, `InputSyncClickAtPoint`, `SemanticOpenBag`.

## Hard rules
- F8 selects only; it never dispatches a click/action.
- Never cache a live UI pointer across UI transitions.
- Direct and InputSync tests re-resolve the current control at the saved normalized point.
- Equal-rank hit candidates fail closed as AMBIGUOUS.
- Time delay is not success evidence; use fresh UI/semantic state.
- Runtime cannot be marked PASS until tested against the game.

## Runtime evidence needed
1. SCAN ACTIVE UI returns meaningful active controls.
2. F8 over a visible X/icon resolves the expected object without acting.
3. TEST DIRECT causes the intended UI transition and evidence reflects it.
4. TEST INPUTSYNC causes the intended UI transition with drag state clean.
5. TEST BAG SEMANTIC opens/verifies `RoleInfo_BagTab`.

## v0.1.1 F8 diagnosis
- Runtime evidence: `SCAN ACTIVE UI total=221 rows=160 TRUNCATED` proves UI discovery is live.
- v0.1.0 controller had no explicit DPI-awareness setup; cursor/client coordinate virtualization can cause outside-client and scaled normalized points.
- v0.1.0 F8 filtered `directCallable` before geometry hit-test, excluding visible `UIText/UIImage` children.
- v0.1.1 separates visual selection from mutation target resolution. InputSync uses the client's own EventSystem raycast and is no longer blocked by direct-callback discovery.
