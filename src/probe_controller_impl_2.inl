}

void ShowRowDetail(const UiRow& row, const wchar_t* prefix = L"") {
    std::wstringstream ss;
    if (prefix && *prefix) ss << prefix << L"\r\n";
    ss << L"Identity: 0x" << HexId(row.identity) << L"\r\n"
       << L"Kind/Class: " << KindName(row.kind) << L" / " << row.className << L"\r\n"
       << L"Name: " << row.name << L"\r\n"
       << L"Text: " << row.text << L"\r\n"
       << L"Tag: " << row.tag << L"\r\n"
       << L"Handler: " << row.handler << L"\r\n"
       << L"Ancestors: " << row.ancestors << L"\r\n"
       << L"Descendants: " << row.descendants << L"\r\n"
       << L"Depth: " << row.depth << L"\r\n"
       << L"Local rect: " << RectText(row) << L"\r\n"
       << L"Area: " << row.area << L"\r\n"
       << L"Direct callable: " << (row.directCallable ? L"YES" : L"NO") << L"\r\n"
       << L"Pointer callable: " << (row.pointerCallable ? L"YES" : L"NO");
    SetWindowTextW(g_app.detail, ss.str().c_str());
}

void AddListColumn(int index, int width, const wchar_t* title) {
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.iSubItem = index;
    column.cx = width;
    column.pszText = const_cast<wchar_t*>(title);
    ListView_InsertColumn(g_app.list, index, &column);
}

void InitListColumns() {
    ListView_SetExtendedListViewStyle(g_app.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    AddListColumn(0, 42, L"#");
    AddListColumn(1, 130, L"Kind/Class");
    AddListColumn(2, 160, L"Name");
    AddListColumn(3, 170, L"Text");
    AddListColumn(4, 120, L"Tag");
    AddListColumn(5, 190, L"Handler");
    AddListColumn(6, 220, L"Ancestors");
    AddListColumn(7, 220, L"Descendants");
    AddListColumn(8, 55, L"Depth");
    AddListColumn(9, 150, L"Local rect");
    AddListColumn(10, 70, L"Direct");
}

void SetListSubItem(int row, int col, const std::wstring& value) {
    ListView_SetItemText(g_app.list, row, col, const_cast<wchar_t*>(value.c_str()));
}

void PopulateSnapshot(const UiSnapshot& snapshot) {
    g_app.lastSnapshot = snapshot;
    ListView_DeleteAllItems(g_app.list);
    const int count = std::max(0, std::min<int>(snapshot.rowCount, static_cast<int>(kMaxUiRows)));
    for (int i = 0; i < count; ++i) {
        const UiRow& row = snapshot.rows[i];
        const std::wstring indexText = std::to_wstring(i);
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = const_cast<wchar_t*>(indexText.c_str());
        item.lParam = i;
        ListView_InsertItem(g_app.list, &item);
        SetListSubItem(i, 1, KindName(row.kind) + L"/" + row.className);
        SetListSubItem(i, 2, row.name);
        SetListSubItem(i, 3, row.text);
        SetListSubItem(i, 4, row.tag);
        SetListSubItem(i, 5, row.handler);
        SetListSubItem(i, 6, row.ancestors);
        SetListSubItem(i, 7, row.descendants);
        SetListSubItem(i, 8, std::to_wstring(row.depth));
        SetListSubItem(i, 9, RectText(row));
        SetListSubItem(i, 10, row.directCallable ? L"YES" : L"NO");
    }
}

bool ScanSnapshot(UiSnapshot& snapshot, bool showInList, std::wstring& error) {
    ProbeResponse response{};
    if (!g_app.bridge.Call(Command::ScanUi, 0, 0, 0, response, error)) return false;
    snapshot = response.snapshot;
    if (showInList) PopulateSnapshot(snapshot);
    Log(response.detail);
    return true;
}

void RefreshClients() {
    g_app.bridge.Close();
    g_app.pointValid = false;
    g_app.games = FindClients();
    SendMessageW(g_app.clients, CB_RESETCONTENT, 0, 0);
    for (const auto& game : g_app.games) {
        std::wstring label = L"PID " + std::to_wstring(game.pid) + L" • " + game.title;
        SendMessageW(g_app.clients, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    if (!g_app.games.empty()) SendMessageW(g_app.clients, CB_SETCURSEL, 0, 0);
    SetStatus(L"RUNTIME PARTIAL • " + std::to_wstring(g_app.games.size()) + L" client(s) tìm thấy");
    Log(L"REFRESH • tìm thấy " + std::to_wstring(g_app.games.size()) + L" client GameAssembly.dll");
}

void AttachSelected() {
    const LRESULT selection = SendMessageW(g_app.clients, CB_GETCURSEL, 0, 0);
    if (selection == CB_ERR || selection < 0 || static_cast<std::size_t>(selection) >= g_app.games.size()) {
        Log(L"ATTACH FAIL • chưa chọn client");
        return;
    }
    std::wstring error;
    if (!g_app.bridge.Attach(g_app.games[static_cast<std::size_t>(selection)], error)) {
        SetStatus(L"ATTACH FAIL");
        Log(L"ATTACH FAIL • " + error);
        return;
    }
    const auto& game = g_app.bridge.Game();
    SetStatus(L"ATTACHED PID " + std::to_wstring(game.pid) + L" • F8 chỉ chọn UI, không click");
    Log(L"ATTACH PASS • PID " + std::to_wstring(game.pid) + L" • WH_GETMESSAGE game thread");
}

std::wstring PointDiagnostic(const POINT& screenPoint, const POINT& clientPoint,
                           int width, int height, int normalizedX = -1, int normalizedY = -1) {
    std::wstringstream ss;
    ss << L"screen=" << screenPoint.x << L"," << screenPoint.y
       << L" client=" << clientPoint.x << L"," << clientPoint.y
       << L" size=" << width << L"x" << height;
    if (normalizedX >= 0 && normalizedY >= 0)
        ss << L" normalized=" << normalizedX << L"," << normalizedY;
    return ss.str();
}

bool CurrentNormalizedPoint(int& x, int& y, std::wstring& diagnostic, std::wstring& error) {
    if (!g_app.bridge.Attached()) { error = L"Chưa attach client"; return false; }
    POINT screenPoint{};
    if (!GetCursorPos(&screenPoint)) { error = L"GetCursorPos thất bại"; return false; }
    POINT clientPoint = screenPoint;
    const GameClient& game = g_app.bridge.Game();
    if (!ScreenToClient(game.window, &clientPoint)) { error = L"ScreenToClient thất bại"; return false; }
    RECT client{};
    if (!GetClientRect(game.window, &client)) { error = L"GetClientRect thất bại"; return false; }
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    diagnostic = PointDiagnostic(screenPoint, clientPoint, width, height);
    if (width <= 0 || height <= 0 || clientPoint.x < 0 || clientPoint.y < 0 ||
        clientPoint.x >= width || clientPoint.y >= height) {
        error = L"Con trỏ không nằm trong client area game • " + diagnostic;
        return false;
    }
    x = static_cast<int>((static_cast<long long>(clientPoint.x) * kCoordinateScale) / width);
    y = static_cast<int>((static_cast<long long>(clientPoint.y) * kCoordinateScale) / height);
    diagnostic = PointDiagnostic(screenPoint, clientPoint, width, height, x, y);
    return true;
}

void PickAtCursor(Command command) {
    int x = 0, y = 0;
    std::wstring diagnostic, error;
    if (!CurrentNormalizedPoint(x, y, diagnostic, error)) { Log(L"F8 PICK FAIL • " + error); return; }

    // F8 is selection-only. The bridge now uses Unity EventSystem.RaycastAll to identify the live target.
    g_app.pointValid = true;
    g_app.pointX = x;
    g_app.pointY = y;

    ProbeResponse response{};
    if (!g_app.bridge.Call(command, x, y, 0, response, error)) {
        const std::wstring captured = L"F8 POINT CAPTURED — chưa dispatch action\r\n" + diagnostic +
                                      L"\r\nEVENTSYSTEM PICK: FAIL • " + error;
        SetWindowTextW(g_app.detail, captured.c_str());
        Log(L"F8 POINT CAPTURED • " + diagnostic + L" • UI PICK FAIL: " + error);
        return;
    }
    g_app.lastPicked = response.picked;
    ShowRowDetail(response.picked, L"F8 PICK — chỉ chọn, chưa dispatch action");
    Log(std::wstring(response.detail) + L" • " + diagnostic);
}

void ManualScan() {
    UiSnapshot snapshot{};
    std::wstring error;
    if (!ScanSnapshot(snapshot, true, error)) { Log(L"SCAN FAIL • " + error); return; }
    SetStatus(L"SCAN PASS • total=" + std::to_wstring(snapshot.totalCount) +
              L" rows=" + std::to_wstring(snapshot.rowCount) +
              (snapshot.truncated ? L" • TRUNCATED" : L""));
}

void ScheduleEvidence(const std::wstring& action, std::uint64_t targetId,
                      const UiSnapshot& before) {
    g_app.evidence.active = true;
    g_app.evidence.action = action;
    g_app.evidence.targetId = targetId;
    g_app.evidence.beforeIds = Identities(before);
    SetTimer(g_app.window, kEvidenceTimer, 250, nullptr);
}

void RunPointAction(Command command, const wchar_t* label) {
    if (!g_app.pointValid) { Log(std::wstring(label) + L" FAIL • nhấn F8 chọn UI trước"); return; }
    UiSnapshot before{};
    std::wstring error;
    if (!ScanSnapshot(before, false, error)) { Log(std::wstring(label) + L" PRE-SCAN FAIL • " + error); return; }
    ProbeResponse response{};
    if (!g_app.bridge.Call(command, g_app.pointX, g_app.pointY, 0, response, error)) {
        Log(std::wstring(label) + L" FAIL • " + error);
        return;
    }
    g_app.lastPicked = response.picked;
    ShowRowDetail(response.picked, label);
    Log(std::wstring(response.detail) + L" • waiting for fresh state proof (timer is observation delay, not success proof)");
    ScheduleEvidence(label, response.picked.identity, before);
}

void CompleteEvidence() {
    KillTimer(g_app.window, kEvidenceTimer);
    if (!g_app.evidence.active) return;
    PendingEvidence evidence = g_app.evidence;
    g_app.evidence = {};

    UiSnapshot after{};
    std::wstring error;
    if (!ScanSnapshot(after, true, error)) {
        Log(evidence.action + L" EVIDENCE FAIL • post-scan: " + error);
        return;
    }
    const auto diff = probe_logic::DiffIdentities(evidence.beforeIds, Identities(after));
    bool targetStillPresent = false;
    if (evidence.targetId) {
        const auto afterIds = Identities(after);
        targetStillPresent = std::find(afterIds.begin(), afterIds.end(), evidence.targetId) != afterIds.end();
    }
    std::wstring summary = evidence.action + L" EVIDENCE • added=" + std::to_wstring(diff.added) +
                           L" removed=" + std::to_wstring(diff.removed) +
                           L" unchanged=" + std::to_wstring(diff.unchanged);
    if (evidence.targetId) summary += L" • picked=" + HexId(evidence.targetId) +
                                     (targetStillPresent ? L" STILL-PRESENT" : L" NOT-PRESENT");
    Log(summary);

}

void OnListSelection() {
    const int index = ListView_GetNextItem(g_app.list, -1, LVNI_SELECTED);
    if (index < 0 || index >= g_app.lastSnapshot.rowCount || index >= static_cast<int>(kMaxUiRows)) return;
    ShowRowDetail(g_app.lastSnapshot.rows[index], L"SCAN ROW");
}

HWND MakeControl(const wchar_t* klass, const wchar_t* text, DWORD style,
                 int x, int y, int w, int h, int id) {
    return CreateWindowExW(0, klass, text, WS_CHILD | WS_VISIBLE | style,
                           x, y, w, h, g_app.window,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

void CreateUi(HWND hwnd) {
    g_app.window = hwnd;
    MakeControl(L"STATIC", L"Client:", 0, 12, 13, 46, 22, 0);
    g_app.clients = MakeControl(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL, 60, 8, 540, 320, IDC_CLIENTS);
    MakeControl(L"BUTTON", L"Refresh", BS_PUSHBUTTON, 610, 7, 84, 28, IDC_REFRESH);
    MakeControl(L"BUTTON", L"Attach", BS_PUSHBUTTON, 702, 7, 84, 28, IDC_ATTACH);
    MakeControl(L"BUTTON", L"SCAN ACTIVE UI", BS_PUSHBUTTON, 798, 7, 136, 28, IDC_SCAN);
    g_app.status = MakeControl(L"STATIC", L"RUNTIME PARTIAL", SS_LEFT, 948, 12, 470, 22, IDC_STATUS);

    g_app.list = MakeControl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | WS_BORDER | WS_TABSTOP,
                             12, 45, 1405, 425, IDC_LIST);
    InitListColumns();

    MakeControl(L"STATIC", L"Selected UI / evidence:", 0, 12, 480, 180, 20, 0);
    g_app.detail = MakeControl(L"EDIT", L"Nhấn F8 khi con trỏ nằm trên UI cần probe. F8 KHÔNG CLICK.",
                               ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_BORDER,
                               12, 502, 835, 142, IDC_DETAIL);
    MakeControl(L"BUTTON", L"TEST DIRECT", BS_PUSHBUTTON, 860, 506, 220, 36, IDC_TEST_DIRECT);

    MakeControl(L"STATIC", L"Log:", 0, 860, 555, 50, 20, 0);
    g_app.log = MakeControl(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_BORDER,
                            860, 578, 557, 170, IDC_LOG);
    MakeControl(L"STATIC",
                L"Probe v0.1.2: F8 dùng EventSystem.RaycastAll -> map UIObject -> TEST DIRECT. Không gọi Bag Semantic/InputSync test.",
                SS_LEFT, 12, 656, 820, 44, 0);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            CreateUi(hwnd);
            if (!RegisterHotKey(hwnd, kHotkeyPick, MOD_NOREPEAT, VK_F8))
                Log(L"WARNING • RegisterHotKey F8 thất bại");
            RefreshClients();
            return 0;

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_REFRESH: RefreshClients(); break;
                case IDC_ATTACH: AttachSelected(); break;
                case IDC_SCAN: ManualScan(); break;
                case IDC_TEST_DIRECT: RunPointAction(Command::DirectInvokeAtPoint, L"TEST DIRECT"); break;
                default: break;
            }
            return 0;
        }

        case WM_NOTIFY: {
            const auto* hdr = reinterpret_cast<const NMHDR*>(lParam);
            if (hdr && hdr->idFrom == IDC_LIST && hdr->code == LVN_ITEMCHANGED) OnListSelection();
            return 0;
        }

        case WM_TIMER:
            if (wParam == kEvidenceTimer) CompleteEvidence();
            return 0;

        case WM_HOTKEY:
            if (wParam == kHotkeyPick) PickAtCursor(Command::PickAtPoint);
            return 0;

        case WM_DESTROY:
            UnregisterHotKey(hwnd, kHotkeyPick);
            KillTimer(hwnd, kEvidenceTimer);
            g_app.bridge.Close();
            PostQuitMessage(0);
