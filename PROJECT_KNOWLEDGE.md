# PROJECT KNOWLEDGE — ThanLong UI Internal Probe

## Project identity
- Version: 0.1.0-probe
- Platform: Windows x64
- Purpose: runtime discovery and controlled internal activation tests for unnamed Thần Long UI controls.
- Donor: v9.9 V20 controller/bridge architecture, intentionally stripped to probe-only scope.

## Current state
- Source contract tests: PASS locally.
- Platform-neutral hit ranking/snapshot diff test: PASS locally.
- Windows MSVC build: BUILD PENDING until GitHub Actions completes.
- Runtime on real game: RUNTIME UNTESTED.

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
