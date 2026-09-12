from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    p = ROOT / path
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}: {old[:80]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# Controller metadata + IDs.
replace_once(
    "src/probe_controller_impl_1.inl",
    'constexpr wchar_t kWindowClass[] = L"ThanLongUiInternalProbeV013";\nconstexpr wchar_t kWindowTitle[] = L"ThanLong UI Internal Probe v0.1.3 — DIRECT RETEST";',
    'constexpr wchar_t kWindowClass[] = L"ThanLongUiInternalProbeV014";\nconstexpr wchar_t kWindowTitle[] = L"ThanLong UI Internal Probe v0.1.4 — NAMED UI TARGETS";'
)
replace_once(
    "src/probe_controller_impl_1.inl",
    '    IDC_TEST_DIRECT,\n    IDC_LOG,',
    '    IDC_TEST_DIRECT,\n    IDC_RECOGNIZE_BAG,\n    IDC_DIRECT_BAG,\n    IDC_RECOGNIZE_SKILLS,\n    IDC_DIRECT_SKILLS,\n    IDC_RECOGNIZE_BAGUI,\n    IDC_DIRECT_BAGUI,\n    IDC_LOG,'
)

# Named-target matching + dispatch in bridge.
matcher = r'''
std::wstring LowerAscii(std::wstring value) {
    for (wchar_t& ch : value) {
        if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
    }
    return value;
}

bool ContainsAny(const std::wstring& value, std::initializer_list<const wchar_t*> terms) {
    for (const wchar_t* term : terms) {
        if (term && *term && value.find(term) != std::wstring::npos) return true;
    }
    return false;
}

std::wstring TargetFingerprint(const UiControl& control) {
    return LowerAscii(control.className + L"|" + control.labels.name + L"|" + control.labels.text + L"|" +
                      control.labels.tag + L"|" + control.labels.handler + L"|" + control.labels.ancestors + L"|" +
                      control.labels.descendants);
}

const wchar_t* TargetLabel(UiTarget target) {
    switch (target) {
        case UiTarget::OpenBag: return L"MỞ TAY NẢI";
        case UiTarget::SwitchToSkills: return L"CHUYỂN → SKILL";
        case UiTarget::SwitchToBagUi: return L"CHUYỂN → TAY NẢI";
        default: return L"UNKNOWN TARGET";
    }
}

int TargetThreshold(UiTarget target) {
    switch (target) {
        case UiTarget::OpenBag: return 70;
        case UiTarget::SwitchToSkills: return 80;
        case UiTarget::SwitchToBagUi: return 80;
        default: return 100000;
    }
}

int ScoreNamedTarget(UiTarget target, const UiControl& control) {
    if (!control.directCallable) return -100000;
    const std::wstring all = TargetFingerprint(control);
    const std::wstring name = LowerAscii(control.labels.name);
    const std::wstring text = LowerAscii(control.labels.text);
    const std::wstring handler = LowerAscii(control.labels.handler);
    const std::wstring ancestors = LowerAscii(control.labels.ancestors);
    const std::wstring descendants = LowerAscii(control.labels.descendants);
    const bool hudContext = ContainsAny(ancestors, {L"skillbar", L"topicon", L"mainui", L"maininterface", L"hud", L"operation", L"action"});
    const bool switchVerb = ContainsAny(all, {L"switch", L"change", L"toggle", L"expand", L"collapse", L"more", L"show", L"convert"});
    const bool bagCaption = control.labels.text.find(L"Túi đồ") != std::wstring::npos ||
                            control.labels.descendants.find(L"Túi đồ") != std::wstring::npos ||
                            text.find(L"túi đồ") != std::wstring::npos || descendants.find(L"túi đồ") != std::wstring::npos;

    int score = 0;
    switch (target) {
        case UiTarget::OpenBag:
            if (bagCaption) score += 140;
            if (ContainsAny(name, {L"openbag", L"buttonbag", L"bagbutton", L"btnbag"})) score += 85;
            if (ContainsAny(handler, {L"openbag", L"bagclick", L"bagclicked"})) score += 85;
            if (all.find(L"inventory") != std::wstring::npos) score += 50;
            if (all.find(L"bag") != std::wstring::npos) score += 45;
            if (hudContext) score += 18;
            if (ContainsAny(all, {L"sortbag", L"mergeitems", L"quickitem", L"gem bag", L"fashionbag", L"soulstone"})) score -= 90;
            break;

        case UiTarget::SwitchToSkills:
            if (ContainsAny(name, {L"switchtoskill", L"showskill", L"skillsswitch", L"skillbarswitch"})) score += 90;
            if (ContainsAny(handler, {L"switchtoskill", L"showskill", L"skillbar", L"skillmode"})) score += 75;
            if (all.find(L"skill") != std::wstring::npos) score += 45;
            if (ContainsAny(descendants, {L"skill", L"fight", L"sword", L"combat"})) score += 28;
            if (switchVerb) score += 35;
            if (hudContext) score += 25;
            if (bagCaption) score -= 140;
            if (ContainsAny(handler, {L"buttonskillhovered", L"useskill"})) score -= 80;
            break;

        case UiTarget::SwitchToBagUi:
            if (ContainsAny(name, {L"switchtobag", L"showbag", L"bagswitch", L"menuswitch", L"functionswitch"})) score += 90;
            if (ContainsAny(handler, {L"switchtobag", L"showbag", L"menu", L"function", L"shortcut"})) score += 70;
            if (all.find(L"bag") != std::wstring::npos) score += 42;
            if (ContainsAny(all, {L"menu", L"function", L"shortcut", L"grid", L"item"})) score += 30;
            if (switchVerb) score += 35;
            if (hudContext) score += 25;
            if (bagCaption) score -= 160; // Do not confuse the actual Túi đồ button with the mode switch.
            if (ContainsAny(handler, {L"sortbag", L"mergeitems", L"openbag"})) score -= 80;
            break;

        default:
            return -100000;
    }
    return score;
}

bool BetterNamedCandidate(const UiControl& candidate, const UiControl& current) {
    if (candidate.hasGeometry != current.hasGeometry) return candidate.hasGeometry;
    if (candidate.hasGeometry && current.hasGeometry) {
        if (std::fabs(candidate.area - current.area) > 0.5f) return candidate.area < current.area;
    }
    return candidate.depth > current.depth;
}

bool NamedCandidateTie(const UiControl& a, const UiControl& b) {
    if (a.hasGeometry != b.hasGeometry) return false;
    if (a.hasGeometry && b.hasGeometry && std::fabs(a.area - b.area) > 0.5f) return false;
    return a.depth == b.depth && a.identity != b.identity;
}

bool FindNamedTarget(UiTarget target, UiControl& selected, bool& ambiguous,
                     int& selectedScore, wchar_t* detail, std::size_t cap) {
    selected = {};
    ambiguous = false;
    selectedScore = -100000;
    if (target == UiTarget::None) {
        SetText(detail, cap, L"TARGET NOT FOUND • target id không hợp lệ");
        return false;
    }

    std::vector<UiControl> controls;
    if (!EnumerateActiveUiObjects(controls, detail, cap)) return false;
    const int threshold = TargetThreshold(target);
    bool found = false;
    for (UiControl& control : controls) {
        const int score = ScoreNamedTarget(target, control);
        if (score < threshold) continue;
        if (!found || score > selectedScore || (score == selectedScore && BetterNamedCandidate(control, selected))) {
            selected = control;
            selectedScore = score;
            found = true;
            ambiguous = false;
        } else if (score == selectedScore && NamedCandidateTie(control, selected)) {
            ambiguous = true;
        }
    }

    if (!found) {
        SetText(detail, cap, L"TARGET NOT FOUND • ");
        Append(detail, cap, TargetLabel(target));
        Append(detail, cap, L" • không có fingerprint đủ điểm; không dispatch");
        return false;
    }
    if (ambiguous) {
        SetText(detail, cap, L"TARGET AMBIGUOUS • ");
        Append(detail, cap, TargetLabel(target));
        Append(detail, cap, L" • nhiều live control ngang hạng; không dispatch");
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
    response.resultCode = static_cast<std::int32_t>(ResultCode::TargetRecognized);
    SetText(detail, cap, L"TARGET RECOGNIZED • ");
    Append(detail, cap, TargetLabel(target));
    Append(detail, cap, L" • score="); AppendInt(detail, cap, score);
    Append(detail, cap, L" • Name="); Append(detail, cap, response.picked.name);
    Append(detail, cap, L" • Handler="); Append(detail, cap, response.picked.handler);
    return true;
}

bool DirectInvokeTarget(UiTarget target, ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    // Re-enumerate and re-resolve on every request. Never reuse a pointer from RecognizeTarget.
    UiControl selected{};
    bool ambiguous = false;
    int score = 0;
    if (!FindNamedTarget(target, selected, ambiguous, score, detail, cap)) {
        if (ambiguous) response.resultCode = static_cast<std::int32_t>(ResultCode::Ambiguous);
        return false;
    }
    FillRow(selected, response.picked);
    response.value0 = score;
    if (!InvokeControl(selected, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ResultCode::TargetDispatched);
    SetText(detail, cap, L"TEST DIRECT TARGET PASS • fresh live resolve -> callback • ");
    Append(detail, cap, TargetLabel(target));
    Append(detail, cap, L" • score="); AppendInt(detail, cap, score);
    return true;
}

'''
replace_once("src/probe_bridge_impl_4.inl", "bool EnsureInputSync(wchar_t* detail, std::size_t cap) {", matcher + "bool EnsureInputSync(wchar_t* detail, std::size_t cap) {")
replace_once(
    "src/probe_bridge_impl_4.inl",
    '            case Command::InputSyncClickAtPoint:\n                ok = InputSyncClickAtPoint(g_shared->request.arg0, g_shared->request.arg1, response, detail, _countof(detail)); break;\n            default:',
    '            case Command::InputSyncClickAtPoint:\n                ok = InputSyncClickAtPoint(g_shared->request.arg0, g_shared->request.arg1, response, detail, _countof(detail)); break;\n            case Command::RecognizeTarget:\n                ok = RecognizeTarget(static_cast<UiTarget>(g_shared->request.arg0), response, detail, _countof(detail)); break;\n            case Command::DirectInvokeTarget:\n                ok = DirectInvokeTarget(static_cast<UiTarget>(g_shared->request.arg0), response, detail, _countof(detail)); break;\n            default:'
)

# Controller named-target actions.
controller_actions = r'''
void RecognizeNamedTarget(UiTarget target, const wchar_t* label) {
    ProbeResponse response{};
    std::wstring error;
    if (!g_app.bridge.Call(Command::RecognizeTarget, static_cast<int>(target), 0, 0, response, error)) {
        Log(std::wstring(L"NHẬN DIỆN ") + label + L" FAIL • " + error);
        return;
    }
    g_app.lastPicked = response.picked;
    ShowRowDetail(response.picked, (std::wstring(L"NHẬN DIỆN • ") + label).c_str());
    Log(std::wstring(response.detail) + L" • read-only, chưa dispatch action");
}

void RunNamedTargetAction(UiTarget target, const wchar_t* label) {
    UiSnapshot before{};
    std::wstring error;
    if (!ScanSnapshot(before, false, error)) {
        Log(std::wstring(L"TEST DIRECT TARGET ") + label + L" PRE-SCAN FAIL • " + error);
        return;
    }
    ProbeResponse response{};
    if (!g_app.bridge.Call(Command::DirectInvokeTarget, static_cast<int>(target), 0, 0, response, error)) {
        Log(std::wstring(L"TEST DIRECT TARGET ") + label + L" FAIL • " + error);
        return;
    }
    g_app.lastPicked = response.picked;
    const std::wstring action = std::wstring(L"TEST DIRECT TARGET • ") + label;
    ShowRowDetail(response.picked, action.c_str());
    Log(std::wstring(response.detail) + L" • waiting for fresh state proof (timer is observation delay, not success proof)");
    ScheduleEvidence(action, response.picked.identity, before);
}

'''
replace_once("src/probe_controller_impl_2.inl", "void CompleteEvidence() {", controller_actions + "void CompleteEvidence() {")

old_ui = '''    MakeControl(L"STATIC", L"Selected UI / evidence:", 0, 12, 480, 180, 20, 0);\n    g_app.detail = MakeControl(L"EDIT", L"Nhấn F8 khi con trỏ nằm trên UI cần probe. F8 KHÔNG CLICK.",\n                               ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_BORDER,\n                               12, 502, 835, 142, IDC_DETAIL);\n    MakeControl(L"BUTTON", L"TEST DIRECT", BS_PUSHBUTTON, 860, 506, 220, 36, IDC_TEST_DIRECT);\n\n    MakeControl(L"STATIC", L"Log:", 0, 860, 555, 50, 20, 0);\n    g_app.log = MakeControl(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_BORDER,\n                            860, 578, 557, 170, IDC_LOG);\n    MakeControl(L"STATIC",\n                L"Probe v0.1.3: F8 dùng EventSystem.RaycastAll -> map UIObject -> TEST DIRECT. Không gọi Bag Semantic/InputSync test.",\n                SS_LEFT, 12, 656, 820, 44, 0);'''
new_ui = '''    MakeControl(L"STATIC", L"Selected UI / evidence:", 0, 12, 462, 180, 20, 0);\n    g_app.detail = MakeControl(L"EDIT", L"Nhấn F8 khi con trỏ nằm trên UI cần probe. F8 KHÔNG CLICK.",\n                               ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_BORDER,\n                               12, 482, 835, 176, IDC_DETAIL);\n    MakeControl(L"BUTTON", L"TEST DIRECT", BS_PUSHBUTTON, 860, 482, 220, 36, IDC_TEST_DIRECT);\n    MakeControl(L"STATIC", L"F8 generic", SS_LEFT, 1092, 490, 115, 22, 0);\n\n    MakeControl(L"STATIC", L"MỞ TAY NẢI", SS_LEFT, 860, 535, 150, 22, 0);\n    MakeControl(L"BUTTON", L"NHẬN DIỆN", BS_PUSHBUTTON, 1015, 527, 125, 32, IDC_RECOGNIZE_BAG);\n    MakeControl(L"BUTTON", L"TEST DIRECT TARGET", BS_PUSHBUTTON, 1148, 527, 269, 32, IDC_DIRECT_BAG);\n\n    MakeControl(L"STATIC", L"CHUYỂN → SKILL", SS_LEFT, 860, 575, 150, 22, 0);\n    MakeControl(L"BUTTON", L"NHẬN DIỆN", BS_PUSHBUTTON, 1015, 567, 125, 32, IDC_RECOGNIZE_SKILLS);\n    MakeControl(L"BUTTON", L"TEST DIRECT TARGET", BS_PUSHBUTTON, 1148, 567, 269, 32, IDC_DIRECT_SKILLS);\n\n    MakeControl(L"STATIC", L"CHUYỂN → TAY NẢI", SS_LEFT, 860, 615, 150, 22, 0);\n    MakeControl(L"BUTTON", L"NHẬN DIỆN", BS_PUSHBUTTON, 1015, 607, 125, 32, IDC_RECOGNIZE_BAGUI);\n    MakeControl(L"BUTTON", L"TEST DIRECT TARGET", BS_PUSHBUTTON, 1148, 607, 269, 32, IDC_DIRECT_BAGUI);\n\n    MakeControl(L"STATIC", L"Log:", 0, 860, 653, 50, 20, 0);\n    g_app.log = MakeControl(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_BORDER,\n                            860, 675, 557, 185, IDC_LOG);\n    MakeControl(L"STATIC",\n                L"Probe v0.1.4: F8 = EventSystem selector. Ba target mới dùng live fingerprint Name/Text/Handler/Ancestors/Descendants; nhận diện fail-closed, direct luôn re-resolve. Không Bag Semantic.",\n                SS_LEFT, 12, 675, 820, 70, 0);'''
replace_once("src/probe_controller_impl_2.inl", old_ui, new_ui)

replace_once(
    "src/probe_controller_impl_2.inl",
    '                case IDC_TEST_DIRECT: RunPointAction(Command::DirectInvokeAtPoint, L"TEST DIRECT"); break;\n                default: break;',
    '                case IDC_TEST_DIRECT: RunPointAction(Command::DirectInvokeAtPoint, L"TEST DIRECT"); break;\n                case IDC_RECOGNIZE_BAG: RecognizeNamedTarget(UiTarget::OpenBag, L"MỞ TAY NẢI"); break;\n                case IDC_DIRECT_BAG: RunNamedTargetAction(UiTarget::OpenBag, L"MỞ TAY NẢI"); break;\n                case IDC_RECOGNIZE_SKILLS: RecognizeNamedTarget(UiTarget::SwitchToSkills, L"CHUYỂN → SKILL"); break;\n                case IDC_DIRECT_SKILLS: RunNamedTargetAction(UiTarget::SwitchToSkills, L"CHUYỂN → SKILL"); break;\n                case IDC_RECOGNIZE_BAGUI: RecognizeNamedTarget(UiTarget::SwitchToBagUi, L"CHUYỂN → TAY NẢI"); break;\n                case IDC_DIRECT_BAGUI: RunNamedTargetAction(UiTarget::SwitchToBagUi, L"CHUYỂN → TAY NẢI"); break;\n                default: break;'
)

replace_once(
    "src/probe_controller_impl_3.inl",
    '                                CW_USEDEFAULT, CW_USEDEFAULT, 1460, 810,',
    '                                CW_USEDEFAULT, CW_USEDEFAULT, 1460, 930,'
)

print("v0.1.4 source patch applied")
