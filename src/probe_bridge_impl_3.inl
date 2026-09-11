        !g_ui.rectTransformUtility || !g_ui.unityScreen) {
        SetText(detail, cap, L"Unity UI geometry classes chưa resolve"); return false;
    }
    g_ui.geometryReady = true;
    return true;
}

bool AssignableObject(Il2CppClass* base, Il2CppObject* object) {
    if (!base || !object) return false;
    Il2CppClass* actual = g_api.object_get_class(object);
    return actual && g_api.class_is_assignable_from(base, actual);
}

bool NormalizeRectTransformObject(Il2CppObject* candidate, Il2CppObject*& rectTransform) {
    rectTransform = nullptr;
    if (!candidate) return false;
    if (AssignableObject(g_ui.unityRectTransform, candidate)) { rectTransform = candidate; return true; }
    if (!AssignableObject(g_ui.unityGameObject, candidate)) return false;
    Il2CppClass* actual = g_api.object_get_class(candidate);
    Il2CppObject* transform = nullptr;
    wchar_t ignored[128]{};
    const MethodInfo* getter = actual ? FindMethod(actual, "get_transform", 0) : nullptr;
    if (!getter || !InvokeObject(getter, candidate, transform, ignored, _countof(ignored)) ||
        !AssignableObject(g_ui.unityRectTransform, transform)) return false;
    rectTransform = transform;
    return true;
}

bool GetterMayExposeRectTransform(const MethodInfo* method) {
    if (!method || StaticMethod(method) || g_api.method_get_param_count(method) != 0) return false;
    const Il2CppType* type = g_api.method_get_return_type(method);
    Il2CppClass* klass = type ? g_api.class_from_type(type) : nullptr;
    return klass && (g_api.class_is_assignable_from(g_ui.unityRectTransform, klass) ||
        g_api.class_is_assignable_from(g_ui.unityTransform, klass) ||
        g_api.class_is_assignable_from(g_ui.unityGameObject, klass));
}

bool ResolveRectTransform(Il2CppObject* object, Il2CppClass* klass, Il2CppObject*& rectTransform) {
    rectTransform = nullptr;
    if (!object || !klass) return false;
    if (NormalizeRectTransformObject(object, rectTransform)) return true;
    if (g_api.class_get_methods && g_api.method_get_name) {
        for (Il2CppClass* current = klass; current; current = g_api.class_get_parent(current)) {
            void* iterator = nullptr; int inspected = 0;
            while (const MethodInfo* method = g_api.class_get_methods(current, &iterator)) {
                if (++inspected > 512) break;
                const char* name = g_api.method_get_name(method);
                if (!name || name[0] != 'g' || name[1] != 'e' || name[2] != 't' || name[3] != '_' ||
                    !GetterMayExposeRectTransform(method)) continue;
                Il2CppObject* candidate = nullptr; wchar_t ignored[128]{};
                if (InvokeObject(method, object, candidate, ignored, _countof(ignored)) &&
                    NormalizeRectTransformObject(candidate, rectTransform)) return true;
            }
        }
    }
    for (const char* name : {"get_RectTransform", "get_CoreRectTransform", "get_Transform", "get_CoreTransform", "get_GameObject", "get_CoreGameObject"}) {
        const MethodInfo* method = FindMethod(klass, name, 0);
        if (!GetterMayExposeRectTransform(method)) continue;
        Il2CppObject* candidate = nullptr; wchar_t ignored[128]{};
        if (InvokeObject(method, object, candidate, ignored, _countof(ignored)) &&
            NormalizeRectTransformObject(candidate, rectTransform)) return true;
    }
    return false;
}

bool ReadRectMetrics(Il2CppObject* rectTransform, UiControl& out) {
    Il2CppClass* klass = rectTransform ? g_api.object_get_class(rectTransform) : nullptr;
    const MethodInfo* getter = klass ? FindMethod(klass, "get_rect", 0) : nullptr;
    if (!getter) return false;
    Il2CppObject* boxed = nullptr; wchar_t ignored[128]{};
    if (!InvokeObject(getter, rectTransform, boxed, ignored, _countof(ignored)) || !boxed) return false;
    void* raw = g_api.object_unbox(boxed);
    if (!raw) return false;
    const auto v = *reinterpret_cast<const UnityRectValue*>(raw);
    out.rectX = v.x; out.rectY = v.y; out.rectW = v.width; out.rectH = v.height;
    out.area = std::fabs(v.width * v.height);
    out.hasGeometry = std::isfinite(out.area) && out.area > 0.0f;
    return out.hasGeometry;
}

bool BuildUnityScreenPoint(int normalizedX, int normalizedY, UnityVector2& point,
                           const MethodInfo*& contains, wchar_t* detail, std::size_t cap) {
    contains = nullptr;
    if (!EnsureUiGeometry(detail, cap)) return false;
    if (normalizedX < 0 || normalizedX >= kCoordinateScale || normalizedY < 0 || normalizedY >= kCoordinateScale) {
        SetText(detail, cap, L"Tọa độ chuẩn hóa nằm ngoài client"); return false;
    }
    std::int32_t width = 0, height = 0;
    if (!StaticScalar(g_ui.unityScreen, "get_width", width, detail, cap) || width <= 0 ||
        !StaticScalar(g_ui.unityScreen, "get_height", height, detail, cap) || height <= 0) {
        SetText(detail, cap, L"Không đọc được Unity Screen.width/height"); return false;
    }
    contains = ExactMethod(g_ui.rectTransformUtility, "RectangleContainsScreenPoint", 3, true,
        "UnityEngine.RectTransform", "UnityEngine.Vector2", "UnityEngine.Camera");
    if (!contains) { SetText(detail, cap, L"Không resolve RectangleContainsScreenPoint"); return false; }
    point.x = static_cast<float>(static_cast<double>(normalizedX) * width / kCoordinateScale);
    const double topY = static_cast<double>(normalizedY) * height / kCoordinateScale;
    point.y = static_cast<float>(height - 1.0 - topY);
    return true;
}

bool RectContainsScreenPoint(Il2CppObject* rectTransform, const UnityVector2& point, const MethodInfo* contains) {
    UnityVector2 p = point; Il2CppObject* camera = nullptr;
    void* args[] = {&rectTransform, &p, &camera};
    std::int64_t result = 0; wchar_t ignored[128]{};
    return InvokeScalarArgs(contains, nullptr, args, result, ignored, _countof(ignored)) && result != 0;
}

bool ClassifyControl(Il2CppClass* klass, LocalKind& kind) {
    for (const auto& pair : g_ui.kindCache) if (pair.first == klass) { kind = pair.second; return true; }
    if (g_ui.button && g_api.class_is_assignable_from(g_ui.button, klass)) kind = LocalKind::Button;
    else if (g_ui.toggle && g_api.class_is_assignable_from(g_ui.toggle, klass)) kind = LocalKind::Toggle;
    else if (g_ui.rect && g_api.class_is_assignable_from(g_ui.rect, klass)) kind = LocalKind::Rect;
    else return false;
    g_ui.kindCache.push_back({klass, kind});
    return true;
}

void AppendLabel(std::wstring& target, const std::wstring& value) {
    if (value.empty() || target.find(value) != std::wstring::npos) return;
    if (!target.empty()) target += L"/";
    target += value;
}

int UiDepth(Il2CppObject* object) {
    int depth = 0; std::vector<Il2CppObject*> seen;
    while (object && depth < 64) {
        if (std::find(seen.begin(), seen.end(), object) != seen.end()) break;
        seen.push_back(object);
        Il2CppClass* klass = g_api.object_get_class(object);
        Il2CppObject* parent = nullptr;
        if (!klass || !ObjectGetter(object, klass, "get_Parent", parent) || !parent) break;
        object = parent; ++depth;
    }
    return depth;
}

void ReadAncestors(UiControl& control) {
    control.labels.ancestors.clear();
    Il2CppObject* parentArray = nullptr;
    if (ObjectGetter(control.object, control.klass, "get_CoreParents", parentArray) && parentArray) {
        std::vector<Il2CppObject*> parents;
        if (ReadManagedPointerArray(parentArray, parents, 64)) {
            for (Il2CppObject* parent : parents) {
                Il2CppClass* klass = parent ? g_api.object_get_class(parent) : nullptr;
                std::wstring name;
                if (klass && ReadUiString(parent, klass, "Name", name)) AppendLabel(control.labels.ancestors, name);
            }
            if (!control.labels.ancestors.empty()) return;
        }
    }
    Il2CppObject* current = control.object; std::vector<Il2CppObject*> seen;
    for (int depth = 0; depth < 12 && current; ++depth) {
        if (std::find(seen.begin(), seen.end(), current) != seen.end()) break;
        seen.push_back(current);
        Il2CppClass* klass = g_api.object_get_class(current);
        Il2CppObject* parent = nullptr;
        if (!klass || !ObjectGetter(current, klass, "get_Parent", parent) || !parent) break;
        Il2CppClass* parentClass = g_api.object_get_class(parent);
        std::wstring name;
        if (parentClass && ReadUiString(parent, parentClass, "Name", name)) AppendLabel(control.labels.ancestors, name);
        current = parent;
    }
}

void CollectDescendantLabels(UiControl& control) {
    control.labels.descendants.clear();
    std::vector<Il2CppObject*> pending{control.object};
    std::vector<Il2CppObject*> visited;
    while (!pending.empty() && visited.size() < 96) {
        Il2CppObject* current = pending.back(); pending.pop_back();
        if (!current || std::find(visited.begin(), visited.end(), current) != visited.end()) continue;
        visited.push_back(current);
        Il2CppClass* klass = g_api.object_get_class(current);
        if (!klass) continue;
        if (current != control.object) {
            std::wstring name, text;
            if (ReadUiString(current, klass, "Name", name)) AppendLabel(control.labels.descendants, name);
            if (ReadUiString(current, klass, "Text", text)) AppendLabel(control.labels.descendants, text);
        }
        Il2CppObject* children = nullptr;
        if (!ObjectGetter(current, klass, "get_CoreChildren", children) || !children)
            (void)ObjectGetter(current, klass, "get_Children", children);
        if (!children) continue;
        std::vector<Il2CppObject*> values;
        if (!ReadManagedPointerArray(children, values, 128)) continue;
        for (Il2CppObject* child : values) pending.push_back(child);
    }
}

std::uint64_t BuildIdentity(const UiControl& control) {
    std::wstring key = control.className + L"|" + control.labels.name + L"|" + control.labels.text + L"|" +
        control.labels.tag + L"|" + control.labels.handler + L"|" + control.labels.ancestors + L"|" +
        control.labels.descendants + L"|" + std::to_wstring(control.depth);
    const std::uint64_t pointerSeed = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(control.object)) ^ 14695981039346656037ull;
    return probe_logic::Fnv1a64(key, pointerSeed);
}

bool ReadOneControl(Il2CppObject* object, UiControl& row, bool withGeometry) {
    if (!object) return false;
    Il2CppClass* klass = g_api.object_get_class(object);
    if (!klass) return false;
    const MethodInfo* activeGetter = FindMethod(klass, "get_ActiveInHierarchy", 0);
    if (activeGetter) {
        std::int32_t active = 1; wchar_t ignored[128]{};
        if (ScalarGetter(klass, "get_ActiveInHierarchy", object, active, ignored, _countof(ignored)) && !active) return false;
    }
    row = {};
    row.object = object; row.klass = klass;
    row.className = WideFromUtf8(g_api.class_get_name ? g_api.class_get_name(klass) : "");
    (void)ClassifyControl(klass, row.kind);
    (void)ReadUiString(object, klass, "Name", row.labels.name);
    (void)ReadUiString(object, klass, "Text", row.labels.text);
    (void)ReadUiString(object, klass, "Tag", row.labels.tag);
    (void)ReadUiString(object, klass, "PointerClickHandler", row.labels.handler);
    if (row.labels.handler.empty()) (void)ReadUiString(object, klass, "ClickHandler", row.labels.handler);
    ReadAncestors(row);
    CollectDescendantLabels(row);
    row.depth = UiDepth(object);

    bool interactable = true;
    if (row.kind == LocalKind::Button || row.kind == LocalKind::Toggle) {
        std::int32_t value = 1; wchar_t ignored[96]{};
        if (FindMethod(klass, "get_Interactable", 0) && ScalarGetter(klass, "get_Interactable", object, value, ignored, _countof(ignored))) interactable = value != 0;
    }
    if (row.kind == LocalKind::Button) row.directCallable = interactable && ExactMethod(klass, "HandleClickEvent", 0, false);
    else if (row.kind == LocalKind::Toggle) row.directCallable = interactable &&
        (ExactMethod(klass, "set_Selected", 1, false, "System.Boolean") || ExactMethod(klass, "HandleSelectEvent", 1, false, "System.Boolean"));
    else if (row.kind == LocalKind::Rect) row.directCallable = !row.labels.handler.empty();
    row.pointerCallable = !row.labels.handler.empty();

    if (withGeometry) {
        Il2CppObject* rectTransform = nullptr;
        if (ResolveRectTransform(object, klass, rectTransform)) (void)ReadRectMetrics(rectTransform, row);
    }
    row.identity = BuildIdentity(row);
    return true;
}

bool EnumerateActiveUiObjects(std::vector<UiControl>& objects, wchar_t* detail, std::size_t cap) {
    objects.clear();
    if (!EnsureUiDiscovery(detail, cap)) return false;
    bool geometryAvailable = false;
    wchar_t geometryDetail[128]{};
    geometryAvailable = EnsureUiGeometry(geometryDetail, _countof(geometryDetail));

    Il2CppObject* dictionary = nullptr;
    g_api.field_static_get_value(g_ui.instances, &dictionary);
    Il2CppObject* entries = nullptr;
    std::int32_t count = 0; std::uintptr_t capacity = 0;
    if (!dictionary || !ReadLocal(dictionary, 0x18, entries) || !entries ||
        !ReadLocal(dictionary, 0x20, count) || count < 0 || count > 32768 ||
        !ReadLocal(entries, 0x18, capacity) || capacity > 32768) {
        SetText(detail, cap, L"UIObject.instances dictionary không hợp lệ"); return false;
    }
    for (std::uintptr_t i = 0; i < capacity; ++i) {
        Il2CppObject* object = nullptr;
        const std::size_t entry = 0x20 + static_cast<std::size_t>(i) * 0x18;
        if (!ReadLocal(entries, entry + 0x10, object) || !object) continue;
        UiControl row{};
        if (ReadOneControl(object, row, geometryAvailable)) objects.push_back(std::move(row));
    }
    return true;
}

void FillRow(const UiControl& control, UiRow& row) {
    row = {};
    row.identity = control.identity;
    switch (control.kind) {
        case LocalKind::Button: row.kind = static_cast<int>(UiKind::Button); break;
        case LocalKind::Toggle: row.kind = static_cast<int>(UiKind::Toggle); break;
        case LocalKind::Rect: row.kind = static_cast<int>(UiKind::Rect); break;
        default: row.kind = static_cast<int>(UiKind::Other); break;
    }
    row.depth = control.depth;
    row.area = control.area;
    row.rectX = control.rectX;
    row.rectY = control.rectY;
    row.rectWidth = control.rectW;
    row.rectHeight = control.rectH;
    row.hasGeometry = control.hasGeometry ? 1 : 0;
    row.directCallable = control.directCallable ? 1 : 0;
    row.pointerCallable = control.pointerCallable ? 1 : 0;
    CopyWide(row.className, _countof(row.className), control.className);
    CopyWide(row.name, _countof(row.name), control.labels.name);
    CopyWide(row.text, _countof(row.text), control.labels.text);
    CopyWide(row.tag, _countof(row.tag), control.labels.tag);
    CopyWide(row.handler, _countof(row.handler), control.labels.handler);
    CopyWide(row.ancestors, _countof(row.ancestors), control.labels.ancestors);
    CopyWide(row.descendants, _countof(row.descendants), control.labels.descendants);
}

bool ScanUi(ProbeResponse& response, wchar_t* detail, std::size_t cap) {
    std::vector<UiControl> objects;
    if (!EnumerateActiveUiObjects(objects, detail, cap)) return false;
    response.snapshot.totalCount = static_cast<std::int32_t>(objects.size());
    response.snapshot.rowCount = static_cast<std::int32_t>(std::min<std::size_t>(objects.size(), kMaxUiRows));
    response.snapshot.truncated = objects.size() > kMaxUiRows ? 1 : 0;
    for (int i = 0; i < response.snapshot.rowCount; ++i) FillRow(objects[static_cast<std::size_t>(i)], response.snapshot.rows[i]);
    response.resultCode = static_cast<std::int32_t>(ResultCode::SnapshotReady);
    SetText(detail, cap, L"SCAN ACTIVE UI • total="); AppendInt(detail, cap, response.snapshot.totalCount);
    Append(detail, cap, L" rows="); AppendInt(detail, cap, response.snapshot.rowCount);
    if (response.snapshot.truncated) Append(detail, cap, L" • TRUNCATED");
    return true;
}

bool FindControlAtPoint(int normalizedX, int normalizedY, UiControl& selected,
                        bool& ambiguous, wchar_t* detail, std::size_t cap) {
    ambiguous = false;
    UnityVector2 point{}; const MethodInfo* contains = nullptr;
    if (!BuildUnityScreenPoint(normalizedX, normalizedY, point, contains, detail, cap)) return false;
    std::vector<UiControl> objects;
    if (!EnumerateActiveUiObjects(objects, detail, cap)) return false;
    std::vector<probe_logic::HitRank> hits;
    for (std::size_t i = 0; i < objects.size(); ++i) {
        UiControl& control = objects[i];
        if (!control.directCallable || !control.hasGeometry) continue;
        Il2CppObject* rectTransform = nullptr;
        if (!ResolveRectTransform(control.object, control.klass, rectTransform)) continue;
        if (!RectContainsScreenPoint(rectTransform, point, contains)) continue;
        hits.push_back({static_cast<int>(i), control.area, control.depth, control.identity});
    }
    const auto pick = probe_logic::ChooseHit(hits);
    if (pick.status == probe_logic::PickStatus::Ambiguous) {
        ambiguous = true; SetText(detail, cap, L"AMBIGUOUS • hai control nội bộ đồng hạng; fail-closed"); return false;
    }
    if (pick.status != probe_logic::PickStatus::Selected || pick.index < 0 || static_cast<std::size_t>(pick.index) >= objects.size()) {
        SetText(detail, cap, L"Không có control callable tại điểm F8"); return false;
    }
    selected = std::move(objects[static_cast<std::size_t>(pick.index)]);
