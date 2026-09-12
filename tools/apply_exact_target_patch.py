from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

bridge_path = ROOT / "src/probe_bridge_impl_4.inl"
bridge = bridge_path.read_text(encoding="utf-8")
start = bridge.index("std::wstring LowerAscii")
end = bridge.index("bool EnsureInputSync", start)

block = r'''enum class SkillBarSwitchState {
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

'''

bridge_path.write_text(bridge[:start] + block + bridge[end:], encoding="utf-8")

controller_path = ROOT / "src/probe_controller_impl_2.inl"
controller = controller_path.read_text(encoding="utf-8")
old = '''    g_app.lastPicked = response.picked;\n    const std::wstring action = std::wstring(L"TEST DIRECT TARGET • ") + label;\n    ShowRowDetail(response.picked, action.c_str());\n    Log(std::wstring(response.detail) + L" • waiting for fresh state proof (timer is observation delay, not success proof)");\n    ScheduleEvidence(action, response.picked.identity, before);'''
new = '''    g_app.lastPicked = response.picked;\n    const std::wstring action = std::wstring(L"TEST DIRECT TARGET • ") + label;\n    ShowRowDetail(response.picked, action.c_str());\n    if (response.resultCode == static_cast<std::int32_t>(ResultCode::TargetAlreadyInState)) {\n        Log(std::wstring(response.detail) + L" • NO-OP an toàn, không dispatch và không chờ evidence giả");\n        return;\n    }\n    Log(std::wstring(response.detail) + L" • waiting for fresh state proof (timer is observation delay, not success proof)");\n    ScheduleEvidence(action, response.picked.identity, before);'''
if old not in controller:
    raise SystemExit("controller action block not found")
controller_path.write_text(controller.replace(old, new, 1), encoding="utf-8")

print("exact target + directional state guard patch applied")
