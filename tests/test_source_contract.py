from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def text(rel: str) -> str:
    p = ROOT / rel
    assert p.exists(), f"missing {rel}"
    return p.read_text(encoding="utf-8")


def translation_unit(main_rel: str, glob_pattern: str) -> str:
    main = text(main_rel)
    parts = "".join(p.read_text(encoding="utf-8") for p in sorted((ROOT / "src").glob(glob_pattern)))
    return main + parts


def test_protocol_probe_only():
    s = text("src/probe_protocol.h")
    for token in [
        "ScanUi", "PickAtPoint", "DirectInvokeAtPoint",
        "InputSyncClickAtPoint",
        "kMaxUiRows = 160", "UiRow", "UiSnapshot", "SharedBlock",
        "rectX", "rectY", "rectWidth", "rectHeight",
    ]:
        assert token in s, token
    for forbidden in [
        "StartAutoFight", "StopAutoFight", "BeginBackgroundSell",
        "AdvanceBackgroundTreatment", "SellBagItem", "DropBagItem",
        "ClickNpc", "Revive", "ReadBagPage", "Telegram", "License",
    ]:
        assert forbidden not in s, forbidden


def test_bridge_contains_only_probe_runtime_paths():
    s = translation_unit("src/probe_bridge.cpp", "probe_bridge_impl_*.inl")
    for token in [
        "UIObject.instances", "HandleClickEvent", "PointerClickHandler",
        "TryClickUI", "EndUIDrag", "CancelUIDragState",
        "EventSystem", "PointerEventData", "RaycastAll",
        "RaycastUiObjectsAtPoint", "MapRaycastGameObject",
        "GetLastPointerEventData", "m_RaycastResultCache", "get_currentInputModule", "LivePointerRaycastRuntime",
        "ChooseRaycastCandidate", "PickStatus::Ambiguous", "TlcProbeGetMessageHook",
    ]:
        assert token in s, token
    assert "if (!control.directCallable || !control.hasGeometry) continue;" not in s
    assert "SemanticOpenBag" not in s
    assert "RoleInfo_BagTab" not in s
    assert "TrySemanticCallUi" not in s
    assert "CollectPointHits" not in s
    for forbidden in [
        "BackgroundSell", "BackgroundTreatment", "AutoFightAction",
        "SellBagItem", "DropBagItem", "ClickNpc", "Revive(",
        "Telegram", "ThanLongLicense", "trade_v", "sell_filter",
    ]:
        assert forbidden not in s, forbidden


def test_controller_is_probe_only_and_f8_is_selection():
    s = translation_unit("src/probe_controller.cpp", "probe_controller_impl_*.inl")
    for token in [
        "RegisterHotKey", "VK_F8", "MOD_NOREPEAT",
        "Command::PickAtPoint", "Command::ScanUi",
        "Command::DirectInvokeAtPoint",
        "SCAN ACTIVE UI", "TEST DIRECT",
        "SetWindowsHookExW",
        "WH_GETMESSAGE", "TlcProbeGetMessageHook", "DiffIdentities",
        "GameAssembly.dll", "SetProcessDpiAwarenessContext",
        "DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2",
    ]:
        assert token in s, token
    # F8 is a selector; only explicit test buttons dispatch mutation commands.
    f8_pos = s.find("case WM_HOTKEY")
    direct_pos = s.find("Command::DirectInvokeAtPoint")
    assert f8_pos >= 0 and direct_pos >= 0
    f8_block = s[f8_pos:f8_pos + 1800]
    assert "Command::PickAtPoint" in f8_block
    assert "Command::DirectInvokeAtPoint" not in f8_block
    assert "Command::InputSyncClickAtPoint" not in f8_block
    assert "F8 POINT CAPTURED" in s
    assert "TEST BAG SEMANTIC" not in s
    assert "TEST INPUTSYNC" not in s
    assert "Command::SemanticOpenBag" not in s
    for forbidden in [
        "ThanLongLicense", "Telegram", "BackgroundSell", "BackgroundTreatment",
        "SellBagItem", "DropBagItem", "StartAutoFight", "trade_v", "sell_filter",
    ]:
        assert forbidden not in s, forbidden


def test_build_and_ci_publish_probe_artifact():
    cmake = text("CMakeLists.txt")
    for token in [
        "ProbeController", "ProbeBridge", "probe_logic_test",
        "enable_testing", "add_test", "CMAKE_CXX_STANDARD 17",
    ]:
        assert token in cmake, token
    workflow = text(".github/workflows/build-windows.yml")
    for token in [
        "windows-latest", "cmake -S . -B build -A x64",
        "ctest --test-dir build -C Release --output-on-failure",
        "ThanLong-UI-Internal-Probe-v0.1.3-win-x64", "actions/upload-artifact@v4",
        "dist/ProbeController.exe", "dist/ProbeBridge.dll", "contents: write",
    ]:
        assert token in workflow, token
    readme = text("README.md")
    for token in [
        "SCAN PASS", "F8", "SCAN ACTIVE UI", "TEST DIRECT",
        "ProbeBridge.dll", "0.1.3-probe",
    ]:
        assert token in readme, token
