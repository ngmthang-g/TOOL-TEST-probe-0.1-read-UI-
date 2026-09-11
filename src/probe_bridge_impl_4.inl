    return true;
}

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
    UiControl selected{}; bool ambiguous = false;
    if (!FindControlAtPoint(x, y, selected, ambiguous, detail, cap)) {
        if (ambiguous) response.resultCode = static_cast<std::int32_t>(ResultCode::Ambiguous);
        return false;
    }
    FillRow(selected, response.picked);
    response.resultCode = static_cast<std::int32_t>(ResultCode::Picked);
    SetText(detail, cap, L"F8 PICK PASS • ");
    Append(detail, cap, response.picked.className);
    Append(detail, cap, L" • Name="); Append(detail, cap, response.picked.name);
    return true;
}

bool DirectInvokeAtPoint(int x, int y, ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    UiControl selected{}; bool ambiguous = false;
    if (!FindControlAtPoint(x, y, selected, ambiguous, detail, cap)) {
        if (ambiguous) response.resultCode = static_cast<std::int32_t>(ResultCode::Ambiguous);
        return false;
    }
    FillRow(selected, response.picked);
    if (!InvokeControl(selected, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ResultCode::DirectDispatched);
    SetText(detail, cap, L"DIRECT DISPATCH PASS • re-resolved current UI • ");
    Append(detail, cap, response.picked.className);
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
    UiControl selected{}; bool ambiguous = false;
    if (!FindControlAtPoint(x, y, selected, ambiguous, detail, cap)) {
        if (ambiguous) response.resultCode = static_cast<std::int32_t>(ResultCode::Ambiguous);
        return false;
    }
    FillRow(selected, response.picked);
    if (!EnsureInputSync(detail, cap)) return false;
    UnityVector2 point{}; const MethodInfo* ignoredContains = nullptr;
    if (!BuildUnityScreenPoint(x, y, point, ignoredContains, detail, cap)) return false;
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

bool GuiCallReferenceParam(const MethodInfo* method, std::uint32_t index) {
    if (!method || index >= g_api.method_get_param_count(method)) return false;
    const Il2CppType* type = g_api.method_get_param(method, index);
    Il2CppClass* klass = type ? g_api.class_from_type(type) : nullptr;
    return klass && !g_api.class_is_valuetype(klass);
}

bool FindUiByName(const char* uiName, Il2CppObject*& ui, wchar_t* detail, std::size_t cap) {
    ui = nullptr;
    if (!EnsureUiLua(true, detail, cap)) return false;
    Il2CppString* name = g_api.string_new(uiName);
    if (!name) return false;
    for (const char* methodName : {"FindUI", "MainFindUI"}) {
        const MethodInfo* method = ExactMethod(g_ui.guiApi, methodName, 1, true, "System.String");
        if (!method) continue;
        void* args[] = {&name};
        if (InvokeObjectArgs(method, nullptr, args, ui, detail, cap) && ui) return true;
    }
    SetText(detail, cap, L"Không tìm thấy UI theo tên"); return false;
}

bool TrySemanticCallUi(const char* uiName, wchar_t* detail, std::size_t cap) {
    if (!EnsureUiLua(true, detail, cap)) return false;
    Il2CppString* name = g_api.string_new(uiName);
    Il2CppObject* emptyArgs = g_api.array_new(g_ui.systemObject, 0);
    if (!name || !emptyArgs) return false;
    for (const char* methodName : {"MainCallUI", "CallUI"}) {
        for (int argc = 1; argc <= 3; ++argc) {
            const MethodInfo* method = FindMethod(g_ui.guiApi, methodName, argc);
            if (!method || !StaticMethod(method) || !ParamType(method, 0, "System.String")) continue;
            Il2CppObject* null1 = nullptr; Il2CppObject* null2 = nullptr;
            void* args[3] = {&name, nullptr, nullptr}; bool compatible = true;
            if (argc >= 2) {
                if (ParamType(method, 1, "System.Object[]")) args[1] = &emptyArgs;
                else if (GuiCallReferenceParam(method, 1)) args[1] = &null1;
                else compatible = false;
            }
            if (argc >= 3) {
                if (ParamType(method, 2, "System.Object[]")) args[2] = &emptyArgs;
                else if (GuiCallReferenceParam(method, 2)) args[2] = &null2;
                else compatible = false;
            }
            if (!compatible) continue;
            void* exc = nullptr;
            (void)g_api.runtime_invoke(method, nullptr, args, &exc);
            if (!exc) return true;
        }
    }
    SetText(detail, cap, L"Không có MainCallUI/CallUI overload tương thích"); return false;
}

bool SemanticOpenBag(bool verifyOnly, ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    Il2CppObject* bag = nullptr; wchar_t ignored[192]{};
    if (FindUiByName("RoleInfo_BagTab", bag, ignored, _countof(ignored)) && bag) {
        response.resultCode = static_cast<std::int32_t>(ResultCode::SemanticVerified);
        SetText(detail, cap, L"SEMANTIC VERIFY PASS • RoleInfo_BagTab tồn tại"); return true;
    }
    if (verifyOnly) { SetText(detail, cap, L"SEMANTIC VERIFY FAIL • RoleInfo_BagTab chưa tồn tại"); return false; }
    wchar_t parentDetail[192]{}, bagDetail[192]{};
    const bool parent = TrySemanticCallUi("RoleInfo", parentDetail, _countof(parentDetail));
    const bool child = TrySemanticCallUi("RoleInfo_BagTab", bagDetail, _countof(bagDetail));
    response.value0 = parent ? 1 : 0; response.value1 = child ? 1 : 0;
    if (!child) { SetText(detail, cap, L"SEMANTIC BAG DISPATCH FAIL • "); Append(detail, cap, bagDetail); return false; }
    response.resultCode = static_cast<std::int32_t>(ResultCode::SemanticDispatched);
    SetText(detail, cap, L"SEMANTIC BAG DISPATCH PASS • MainCallUI/CallUI RoleInfo_BagTab"); return true;
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
            case Command::SemanticOpenBag:
                ok = SemanticOpenBag(g_shared->request.arg0 != 0, response, detail, _countof(detail)); break;
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
