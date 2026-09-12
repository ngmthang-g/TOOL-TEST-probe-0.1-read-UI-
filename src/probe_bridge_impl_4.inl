bool ExecutorInstance(Il2CppObject*& instance, const MethodInfo*& execute, wchar_t* detail, std::size_t cap) {
    instance = nullptr; execute = nullptr;
    if (!EnsureUiLua(false, detail, cap)) return false;
    const MethodInfo* getInstance = ExactMethod(g_ui.executor, "get_Instance", 0, true);
    if (!getInstance || !InvokeObject(getInstance, nullptr, instance, detail, cap) || !instance) {
        SetText(detail, cap, L"MonoBehaviourExecutor.Instance chưa sẵn sàng"); return false;
    }
    execute = FindMethod(g_ui.executor, "ExecuteScriptFunction", 3);
    if (!execute) { SetText(detail, cap, L"Thiếu ExecuteScriptFunction"); return false; }
    return true;
}

bool ExecuteLuaHandler(Il2CppObject* uiObject, const std::wstring& handler,
                       wchar_t* detail, std::size_t cap) {
    if (!EnsureUiLua(false, detail, cap)) return false;
    const std::string utf8 = Utf8FromWide(handler);
    if (utf8.empty()) { SetText(detail, cap, L"PointerClickHandler rỗng"); return false; }
    Il2CppString* function = g_api.string_new(utf8.c_str());
    Il2CppObject* argsArray = g_api.array_new(g_ui.systemObject, 3);
    if (!function || !argsArray || !WriteLocal(argsArray, 0x20, uiObject)) {
        SetText(detail, cap, L"Không tạo được Lua callback arguments"); return false;
    }
    Il2CppObject* executor = nullptr; const MethodInfo* execute = nullptr;
    if (!ExecutorInstance(executor, execute, detail, cap)) return false;
    void* args[] = {&uiObject, &function, &argsArray};
    return InvokeVoid(execute, executor, args, detail, cap);
}

bool InvokeControl(UiControl& control, wchar_t* detail, std::size_t cap) {
    if (control.kind == LocalKind::Button) {
        const MethodInfo* click = ExactMethod(control.klass, "HandleClickEvent", 0, false);
        if (!click) { SetText(detail, cap, L"UIButton thiếu HandleClickEvent"); return false; }
        return InvokeVoid(click, control.object, nullptr, detail, cap);
    }
    if (control.kind == LocalKind::Toggle) {
        std::uint8_t yes = 1; void* args[] = {&yes};
        const MethodInfo* setSelected = ExactMethod(control.klass, "set_Selected", 1, false, "System.Boolean");
        if (setSelected && InvokeVoid(setSelected, control.object, args, detail, cap)) return true;
        const MethodInfo* selectEvent = ExactMethod(control.klass, "HandleSelectEvent", 1, false, "System.Boolean");
        if (!selectEvent) { SetText(detail, cap, L"UIToggle thiếu select callback"); return false; }
        return InvokeVoid(selectEvent, control.object, args, detail, cap);
    }
    if (control.labels.handler.empty()) { SetText(detail, cap, L"UIRect thiếu PointerClickHandler"); return false; }
    return ExecuteLuaHandler(control.object, control.labels.handler, detail, cap);
}

bool PickAtPoint(int x, int y, ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    UiControl selected{}; EventRaycastStats stats{}; bool ambiguous = false;
    if (!FindEventSystemControlAtPoint(x, y, false, selected, ambiguous, stats, detail, cap)) {
        if (ambiguous) response.resultCode = static_cast<std::int32_t>(ResultCode::Ambiguous);
        return false;
    }
    FillRow(selected, response.picked);
    response.resultCode = static_cast<std::int32_t>(ResultCode::Picked);
    SetText(detail, cap, L"F8 PICK PASS • EventSystem target ");
    Append(detail, cap, response.picked.className);
    Append(detail, cap, L" • Name="); Append(detail, cap, response.picked.name);
    Append(detail, cap, response.picked.directCallable ? L" • DIRECT READY" : L" • mapped visual/non-callable");
    AppendRaycastStats(detail, cap, stats);
    return true;
}

bool DirectInvokeAtPoint(int x, int y, ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    UiControl selected{}; EventRaycastStats stats{}; bool ambiguous = false;
    if (!FindEventSystemControlAtPoint(x, y, true, selected, ambiguous, stats, detail, cap)) {
        if (ambiguous) response.resultCode = static_cast<std::int32_t>(ResultCode::Ambiguous);
        return false;
    }
    FillRow(selected, response.picked);
    if (!InvokeControl(selected, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ResultCode::DirectDispatched);
    SetText(detail, cap, L"DIRECT DISPATCH PASS • EventSystem re-raycast -> UIObject -> callback • ");
    Append(detail, cap, response.picked.className);
    AppendRaycastStats(detail, cap, stats);
    return true;
}


enum class SkillBarSwitchState {
    Unknown = 0,
    BagUi = 1,
    Skills = 2,
};

const wchar_t* TargetLabel(UiTarget target) {
    switch (target) {
        case UiTarget::OpenBag: return L"MỞ TAY NẢI";
        case UiTarget::SwitchToSkills: return L"CHUYỂN → SKILL";
        case UiTarget::SwitchToBagUi: return L"CHUYỂN → TAY NẢI";
        default: return L"UNKNOWN TARGET";
    }
}

const wchar_t* SwitchStateLabel(SkillBarSwitchState state) {
    switch (state) {
        case SkillBarSwitchState::BagUi: return L"TAY NẢI/MENU (Site 1)";
        case SkillBarSwitchState::Skills: return L"SKILL (Site 2)";
        default: return L"UNKNOWN";
    }
}

bool ExactNamedTarget(UiTarget target, const UiControl& control) {
    if (!control.directCallable || control.kind != LocalKind::Button) return false;
    switch (target) {
        case UiTarget::OpenBag:
            // Exact BottomIcon_Layout identity recovered from decrypted DATA-222 Interface.unity3d.
            return control.labels.name == L"ButBag" && control.labels.handler == L"ButBagClick";
        case UiTarget::SwitchToSkills:
        case UiTarget::SwitchToBagUi:
            // Both screenshots are two visual states of this same SkillBar toggle button.
            return control.labels.name == L"ButtonOriginalSwitchSite" &&
                   control.labels.handler == L"ButtonOriginalSwitchSiteClicked";
        default:
            return false;
    }
}

bool ReadToggleSelected(const UiControl& control, bool& selected) {
    selected = false;
    if (!control.object || !control.klass || control.kind != LocalKind::Toggle) return false;
    std::int32_t value = 0;
    wchar_t ignored[128]{};
    if (!ScalarGetter(control.klass, "get_Selected", ManagedThis(control.object), value,
                      ignored, _countof(ignored))) return false;
    selected = value != 0;
    return true;
}

bool ReadSkillBarSwitchState(SkillBarSwitchState& state, wchar_t* detail, std::size_t cap) {
    state = SkillBarSwitchState::Unknown;
    std::vector<UiControl> controls;
    if (!EnumerateActiveUiObjects(controls, detail, cap)) return false;

    const UiControl* first = nullptr;
    const UiControl* second = nullptr;
    int firstCount = 0;
    int secondCount = 0;
    for (const UiControl& control : controls) {
        if (control.kind != LocalKind::Toggle) continue;
        if (control.labels.name == L"ToggleFirstTab") { first = &control; ++firstCount; }
        else if (control.labels.name == L"ToggleSecondTab") { second = &control; ++secondCount; }
    }
    if (firstCount != 1 || secondCount != 1 || !first || !second) {
        SetText(detail, cap, L"TARGET STATE UNKNOWN • SkillBar ToggleFirstTab/ToggleSecondTab không duy nhất");
        return false;
    }

    bool firstSelected = false;
    bool secondSelected = false;
    if (!ReadToggleSelected(*first, firstSelected) || !ReadToggleSelected(*second, secondSelected)) {
        SetText(detail, cap, L"TARGET STATE UNKNOWN • không đọc được UIToggle.get_Selected");
        return false;
    }
    if (firstSelected == secondSelected) {
        SetText(detail, cap, L"TARGET STATE UNKNOWN • trạng thái Site 1/Site 2 không hợp lệ");
        return false;
    }
    state = firstSelected ? SkillBarSwitchState::BagUi : SkillBarSwitchState::Skills;
    return true;
}

bool TargetAlreadyInState(UiTarget target, SkillBarSwitchState state) {
    return (target == UiTarget::SwitchToSkills && state == SkillBarSwitchState::Skills) ||
           (target == UiTarget::SwitchToBagUi && state == SkillBarSwitchState::BagUi);
}

bool TargetDirectionReady(UiTarget target, SkillBarSwitchState state) {
    return (target == UiTarget::SwitchToSkills && state == SkillBarSwitchState::BagUi) ||
           (target == UiTarget::SwitchToBagUi && state == SkillBarSwitchState::Skills);
}

bool FindNamedTarget(UiTarget target, UiControl& selected, bool& ambiguous,
                     int& selectedScore, wchar_t* detail, std::size_t cap) {
    selected = {};
    ambiguous = false;
    selectedScore = 0;
    if (target == UiTarget::None) {
        SetText(detail, cap, L"TARGET NOT FOUND • target id không hợp lệ");
        return false;
    }

    std::vector<UiControl> controls;
    if (!EnumerateActiveUiObjects(controls, detail, cap)) return false;
    int matches = 0;
    for (const UiControl& control : controls) {
        if (!ExactNamedTarget(target, control)) continue;
        selected = control;
        ++matches;
    }
    if (matches == 0) {
        SetText(detail, cap, L"TARGET NOT FOUND • ");
        Append(detail, cap, TargetLabel(target));
        Append(detail, cap, L" • exact DATA-222 Name/Handler không xuất hiện; không dispatch");
        return false;
    }
    if (matches != 1) {
        ambiguous = true;
        SetText(detail, cap, L"TARGET AMBIGUOUS • ");
        Append(detail, cap, TargetLabel(target));
        Append(detail, cap, L" • exact identity xuất hiện nhiều hơn một live control; không dispatch");
        return false;
    }
    selectedScore = 1000; // Exact identity, not heuristic ranking.
    return true;
}

bool ResolveDirectionalState(UiTarget target, ProbeResponse& response,
                             wchar_t* detail, std::size_t cap) {
    if (target == UiTarget::OpenBag) return true;
    SkillBarSwitchState state = SkillBarSwitchState::Unknown;
    if (!ReadSkillBarSwitchState(state, detail, cap)) return false;
    response.value1 = static_cast<std::int32_t>(state);
    if (TargetAlreadyInState(target, state)) {
        response.resultCode = static_cast<std::int32_t>(ResultCode::TargetAlreadyInState);
        SetText(detail, cap, L"TARGET ALREADY IN STATE • ");
        Append(detail, cap, TargetLabel(target));
        Append(detail, cap, L" • current=");
        Append(detail, cap, SwitchStateLabel(state));
        Append(detail, cap, L" • không click để tránh toggle ngược");
        return true;
    }
    if (!TargetDirectionReady(target, state)) {
        SetText(detail, cap, L"TARGET STATE UNKNOWN • hướng toggle không an toàn; không dispatch");
        return false;
    }
    return true;
}

bool RecognizeTarget(UiTarget target, ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    UiControl selected{};
    bool ambiguous = false;
    int score = 0;
    if (!FindNamedTarget(target, selected, ambiguous, score, detail, cap)) {
        if (ambiguous) response.resultCode = static_cast<std::int32_t>(ResultCode::Ambiguous);
        return false;
    }
    FillRow(selected, response.picked);
    response.value0 = score;
    if (!ResolveDirectionalState(target, response, detail, cap)) return false;
    if (response.resultCode == static_cast<std::int32_t>(ResultCode::TargetAlreadyInState)) return true;
    response.resultCode = static_cast<std::int32_t>(ResultCode::TargetRecognized);
    SetText(detail, cap, L"TARGET RECOGNIZED • exact DATA-222 identity • ");
    Append(detail, cap, TargetLabel(target));
    Append(detail, cap, L" • Name="); Append(detail, cap, response.picked.name);
    Append(detail, cap, L" • Handler="); Append(detail, cap, response.picked.handler);
    if (target != UiTarget::OpenBag) {
        Append(detail, cap, L" • direction-safe current=");
        Append(detail, cap, response.value1 == static_cast<int>(SkillBarSwitchState::BagUi)
            ? L"TAY NẢI/MENU (Site 1)" : L"SKILL (Site 2)");
    }
    return true;
}

bool DirectInvokeTarget(UiTarget target, ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    // Every action request resolves both the control and directional state fresh.
    UiControl selected{};
    bool ambiguous = false;
    int score = 0;
    if (!FindNamedTarget(target, selected, ambiguous, score, detail, cap)) {
        if (ambiguous) response.resultCode = static_cast<std::int32_t>(ResultCode::Ambiguous);
        return false;
    }
    FillRow(selected, response.picked);
    response.value0 = score;
    if (!ResolveDirectionalState(target, response, detail, cap)) return false;
    if (response.resultCode == static_cast<std::int32_t>(ResultCode::TargetAlreadyInState)) return true;
    if (!InvokeControl(selected, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ResultCode::TargetDispatched);
    SetText(detail, cap, L"TEST DIRECT TARGET PASS • exact identity + fresh state -> callback • ");
    Append(detail, cap, TargetLabel(target));
    return true;
}

bool EnsureInputSync(wchar_t* detail, std::size_t cap) {
    if (g_ui.inputReady) return true;
    if (!EnsureUiGeometry(detail, cap)) return false;
    g_ui.inputSyncManager = g_api.class_from_name(g_ui.image, "", "InputSyncManager");
    if (g_ui.inputSyncManager) {
        g_ui.inputGetInstance = ExactMethod(g_ui.inputSyncManager, "get_Instance", 0, true);
        g_ui.inputPress = ExactMethod(g_ui.inputSyncManager, "TryClickUI", 2, false, "System.Int32", "UnityEngine.Vector2");
        g_ui.inputRelease = ExactMethod(g_ui.inputSyncManager, "EndUIDrag", 1, false, "UnityEngine.Vector2");
        g_ui.inputCancel = ExactMethod(g_ui.inputSyncManager, "CancelUIDragState", 0, false);
        g_ui.inputDragging = FindField(g_ui.inputSyncManager, "_uiDragging");
    }
    if (!g_ui.inputSyncManager || !g_ui.inputGetInstance || !g_ui.inputPress || !g_ui.inputRelease || !g_ui.inputCancel ||
        !g_ui.inputDragging || !FieldType(g_ui.inputDragging, "System.Boolean") ||
        !ReturnType(g_ui.inputPress, "System.Void") || !ReturnType(g_ui.inputRelease, "System.Void")) {
        SetText(detail, cap, L"InputSyncManager TryClickUI/EndUIDrag/CancelUIDragState chưa resolve"); return false;
    }
    g_ui.inputReady = true;
    return true;
}

bool ReadDragging(Il2CppObject* manager, bool& dragging) {
    dragging = false;
    if (!manager || !g_ui.inputDragging) return false;
    std::uint8_t value = 0; g_api.field_get_value(manager, g_ui.inputDragging, &value);
    dragging = value != 0; return true;
}

void CancelDrag(Il2CppObject* manager) {
    if (!manager || !g_ui.inputCancel) return;
    wchar_t ignored[128]{};
    (void)InvokeVoid(g_ui.inputCancel, manager, nullptr, ignored, _countof(ignored));
}

bool InputSyncClickAtPoint(int x, int y, ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    // Retained only as known-working baseline from donor source; v0.1.3 UI does not expose this test.
    UiControl visual{}; EventRaycastStats visualStats{}; bool ambiguous = false; wchar_t visualDetail[256]{};
    if (FindEventSystemControlAtPoint(x, y, false, visual, ambiguous, visualStats, visualDetail, _countof(visualDetail)))
        FillRow(visual, response.picked);
    if (!EnsureInputSync(detail, cap)) return false;
    UnityVector2 point{};
    if (!BuildUnityScreenPoint(x, y, point, detail, cap)) return false;
    Il2CppObject* manager = nullptr;
    if (!InvokeObject(g_ui.inputGetInstance, nullptr, manager, detail, cap) || !manager) {
        SetText(detail, cap, L"InputSyncManager.Instance chưa sẵn sàng"); return false;
    }
    bool dragging = false;
    if (!ReadDragging(manager, dragging)) { SetText(detail, cap, L"Không đọc được _uiDragging"); return false; }
    if (dragging) { SetText(detail, cap, L"InputSync đang giữ drag; fail-closed"); return false; }
    std::int32_t button = 0;
    void* pressArgs[] = {&button, &point};
    if (!InvokeVoid(g_ui.inputPress, manager, pressArgs, detail, cap)) { CancelDrag(manager); return false; }
    if (!ReadDragging(manager, dragging) || !dragging) {
        SetText(detail, cap, L"TryClickUI raycast không bắt được UI"); return false;
    }
    void* releaseArgs[] = {&point};
    if (!InvokeVoid(g_ui.inputRelease, manager, releaseArgs, detail, cap)) { CancelDrag(manager); return false; }
    if (!ReadDragging(manager, dragging)) { CancelDrag(manager); return false; }
    if (dragging) { CancelDrag(manager); SetText(detail, cap, L"EndUIDrag chưa clear drag; đã CancelUIDragState"); return false; }
    response.resultCode = static_cast<std::int32_t>(ResultCode::InputSyncDispatched);
    SetText(detail, cap, L"INPUTSYNC PASS • TryClickUI -> EndUIDrag • drag cleared");
    return true;
}

bool EnsureShared() {
    if (g_shared) return true;
    wchar_t name[96]{}; MappingName(GetCurrentProcessId(), name, _countof(name));
    g_mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!g_mapping) return false;
    g_shared = reinterpret_cast<SharedBlock*>(MapViewOfFile(g_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBlock)));
    if (!g_shared || g_shared->magic != kMagic || g_shared->protocolVersion != kProtocolVersion ||
        g_shared->targetPid != GetCurrentProcessId()) {
        if (g_shared) UnmapViewOfFile(g_shared);
        if (g_mapping) CloseHandle(g_mapping);
        g_shared = nullptr; g_mapping = nullptr; return false;
    }
    InterlockedExchange(&g_shared->bridgeLoaded, 1);
    return true;
}

void ProcessRequest() {
    if (!EnsureShared()) return;
    const LONG seq = g_shared->requestSeq;
    if (seq <= 0 || seq == g_shared->completedSeq) return;
    if (InterlockedCompareExchange(&g_shared->bridgeBusy, 1, 0) != 0) return;

    ProbeResponse response{};
    wchar_t detail[768]{};
    bool ok = false;
    if (GetCurrentThreadId() != g_shared->targetWindowThreadId) {
        SetText(detail, _countof(detail), L"Sai game UI thread; request bị chặn");
    } else {
        switch (static_cast<Command>(g_shared->request.command)) {
            case Command::ScanUi:
                ok = ScanUi(response, detail, _countof(detail)); break;
            case Command::PickAtPoint:
                ok = PickAtPoint(g_shared->request.arg0, g_shared->request.arg1, response, detail, _countof(detail)); break;
            case Command::DirectInvokeAtPoint:
                ok = DirectInvokeAtPoint(g_shared->request.arg0, g_shared->request.arg1, response, detail, _countof(detail)); break;
            case Command::InputSyncClickAtPoint:
                ok = InputSyncClickAtPoint(g_shared->request.arg0, g_shared->request.arg1, response, detail, _countof(detail)); break;
            case Command::RecognizeTarget:
                ok = RecognizeTarget(static_cast<UiTarget>(g_shared->request.arg0), response, detail, _countof(detail)); break;
            case Command::DirectInvokeTarget:
                ok = DirectInvokeTarget(static_cast<UiTarget>(g_shared->request.arg0), response, detail, _countof(detail)); break;
            default:
                SetText(detail, _countof(detail), L"Probe command không hợp lệ"); break;
        }
    }
    response.ok = ok ? 1 : 0;
    SetText(response.detail, _countof(response.detail), detail);
    g_shared->response = response;
    MemoryBarrier();
    InterlockedExchange(&g_shared->completedSeq, seq);
    InterlockedExchange(&g_shared->bridgeBusy, 0);
}

} // namespace

extern "C" __declspec(dllexport) LRESULT CALLBACK TlcProbeGetMessageHook(int code, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    if (code >= 0 && lParam) {
        const MSG* msg = reinterpret_cast<const MSG*>(lParam);
        if (msg->message == kWakeMessage) ProcessRequest();
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_shared) UnmapViewOfFile(g_shared);
        if (g_mapping) CloseHandle(g_mapping);
        g_shared = nullptr; g_mapping = nullptr;
    }
    return TRUE;
}
