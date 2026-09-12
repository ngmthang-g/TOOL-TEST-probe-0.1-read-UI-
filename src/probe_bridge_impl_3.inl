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
                           wchar_t* detail, std::size_t cap) {
    if (!EnsureUiGeometry(detail, cap)) return false;
    if (normalizedX < 0 || normalizedX >= kCoordinateScale || normalizedY < 0 || normalizedY >= kCoordinateScale) {
        SetText(detail, cap, L"Tọa độ chuẩn hóa nằm ngoài client"); return false;
    }
    std::int32_t width = 0, height = 0;
    if (!StaticScalar(g_ui.unityScreen, "get_width", width, detail, cap) || width <= 0 ||
        !StaticScalar(g_ui.unityScreen, "get_height", height, detail, cap) || height <= 0) {
        SetText(detail, cap, L"Không đọc được Unity Screen.width/height"); return false;
    }
    point.x = static_cast<float>(static_cast<double>(normalizedX) * width / kCoordinateScale);
    const double topY = static_cast<double>(normalizedY) * height / kCoordinateScale;
    point.y = static_cast<float>(height - 1.0 - topY);
    return true;
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

struct EventRaycastStats {
    int raycastHits = 0;
    int totalObjects = 0;
    int boundObjects = 0;
    int mappedObjects = 0;
    int callableMapped = 0;
    UnityVector2 unityPoint{};
};

struct MappedRaycast {
    int objectIndex = -1;
    int raycastOrder = -1;
    int ancestorDistance = -1;
};

Il2CppClass* ResolveEventClass(const char* name) {
    if (!name) return nullptr;
    OpenUnityImages();
    for (const Il2CppImage* image : {g_ui.eventSystemsImage, g_ui.uiModuleImage, g_ui.legacyUnityImage}) {
        if (!image) continue;
        if (Il2CppClass* klass = g_api.class_from_name(image, "UnityEngine.EventSystems", name)) return klass;
    }
    return nullptr;
}

bool EnsureEventSystemRaycast(wchar_t* detail, std::size_t cap) {
    if (g_ui.eventSystemReady) return true;
    if (!EnsureUiGeometry(detail, cap)) return false;
    g_ui.eventSystem = ResolveEventClass("EventSystem");
    g_ui.pointerEventData = ResolveEventClass("PointerEventData");
    if (!g_ui.eventSystem || !g_ui.pointerEventData) {
        SetText(detail, cap, L"EventSystem/PointerEventData chưa resolve"); return false;
    }
    g_ui.eventGetCurrent = ExactMethod(g_ui.eventSystem, "get_current", 0, true);
    g_ui.eventRaycastAll = ExactMethod(g_ui.eventSystem, "RaycastAll", 2, false,
                                       "UnityEngine.EventSystems.PointerEventData");
    g_ui.pointerCtor = ExactMethod(g_ui.pointerEventData, ".ctor", 1, false,
                                   "UnityEngine.EventSystems.EventSystem");
    g_ui.pointerSetPosition = ExactMethod(g_ui.pointerEventData, "set_position", 1, false,
                                          "UnityEngine.Vector2");
    if (!g_ui.eventGetCurrent || !g_ui.eventRaycastAll || !g_ui.pointerCtor || !g_ui.pointerSetPosition) {
        SetText(detail, cap, L"EventSystem RaycastAll/PointerEventData signature chưa resolve"); return false;
    }

    const Il2CppType* listType = g_api.method_get_param(g_ui.eventRaycastAll, 1);
    g_ui.raycastListClass = listType ? g_api.class_from_type(listType) : nullptr;
    if (!g_ui.raycastListClass) {
        SetText(detail, cap, L"Không resolve được List<RaycastResult>"); return false;
    }
    g_ui.raycastListCtor = ExactMethod(g_ui.raycastListClass, ".ctor", 0, false);
    g_ui.raycastListCount = ExactMethod(g_ui.raycastListClass, "get_Count", 0, false);
    g_ui.raycastListGetItem = ExactMethod(g_ui.raycastListClass, "get_Item", 1, false, "System.Int32");
    const Il2CppType* itemType = g_ui.raycastListGetItem ? g_api.method_get_return_type(g_ui.raycastListGetItem) : nullptr;
    g_ui.raycastResultClass = itemType ? g_api.class_from_type(itemType) : nullptr;
    g_ui.raycastResultGameObject = g_ui.raycastResultClass ? FindField(g_ui.raycastResultClass, "gameObject") : nullptr;
    g_ui.raycastResultGetGameObject = g_ui.raycastResultClass ? FindMethod(g_ui.raycastResultClass, "get_gameObject", 0) : nullptr;
    if (!g_ui.raycastListCtor || !g_ui.raycastListCount || !g_ui.raycastListGetItem ||
        !g_ui.raycastResultClass || (!g_ui.raycastResultGameObject && !g_ui.raycastResultGetGameObject)) {
        SetText(detail, cap, L"List<RaycastResult> hoặc RaycastResult.gameObject chưa resolve"); return false;
    }
    g_ui.eventSystemReady = true;
    return true;
}

bool ExtractRaycastGameObject(Il2CppObject* boxedResult, Il2CppObject*& gameObject) {
    gameObject = nullptr;
    if (!boxedResult) return false;
    wchar_t ignored[128]{};
    if (g_ui.raycastResultGetGameObject &&
        InvokeObject(g_ui.raycastResultGetGameObject, ManagedThis(boxedResult), gameObject, ignored, _countof(ignored)) &&
        gameObject && AssignableObject(g_ui.unityGameObject, gameObject)) return true;
    gameObject = nullptr;
    if (g_ui.raycastResultGameObject) {
        g_api.field_get_value(boxedResult, g_ui.raycastResultGameObject, &gameObject);
        if (gameObject && AssignableObject(g_ui.unityGameObject, gameObject)) return true;
    }
    gameObject = nullptr;
    return false;
}

struct LivePointerRaycastRuntime {
    bool ready = false;
    Il2CppClass* eventSystem = nullptr;
    Il2CppClass* pointerEventData = nullptr;
    const MethodInfo* eventGetCurrent = nullptr;
    const MethodInfo* eventGetCurrentInputModule = nullptr;
    const MethodInfo* eventRaycastAll = nullptr;
    const MethodInfo* pointerGetPosition = nullptr;
    const MethodInfo* pointerSetPosition = nullptr;
};

LivePointerRaycastRuntime g_liveRaycast;

bool InvokeStageVoid(const MethodInfo* method, void* instance, void** args,
                     const wchar_t* stage, wchar_t* detail, std::size_t cap) {
    wchar_t inner[256]{};
    if (InvokeVoid(method, instance, args, inner, _countof(inner))) return true;
    SetText(detail, cap, stage);
    Append(detail, cap, L" • ");
    Append(detail, cap, inner);
    return false;
}

bool EnsureLivePointerRaycast(wchar_t* detail, std::size_t cap) {
    if (g_liveRaycast.ready) return true;
    if (!EnsureUiGeometry(detail, cap)) return false;
    g_liveRaycast.eventSystem = ResolveEventClass("EventSystem");
    g_liveRaycast.pointerEventData = ResolveEventClass("PointerEventData");
    if (!g_liveRaycast.eventSystem || !g_liveRaycast.pointerEventData) {
        SetText(detail, cap, L"EventSystem/PointerEventData chưa resolve");
        return false;
    }
    g_liveRaycast.eventGetCurrent = ExactMethod(g_liveRaycast.eventSystem, "get_current", 0, true);
    g_liveRaycast.eventGetCurrentInputModule = ExactMethod(g_liveRaycast.eventSystem, "get_currentInputModule", 0, false);
    g_liveRaycast.eventRaycastAll = ExactMethod(g_liveRaycast.eventSystem, "RaycastAll", 2, false,
                                                 "UnityEngine.EventSystems.PointerEventData");
    g_liveRaycast.pointerGetPosition = ExactMethod(g_liveRaycast.pointerEventData, "get_position", 0, false);
    g_liveRaycast.pointerSetPosition = ExactMethod(g_liveRaycast.pointerEventData, "set_position", 1, false,
                                                    "UnityEngine.Vector2");
    if (!g_liveRaycast.eventGetCurrent || !g_liveRaycast.eventGetCurrentInputModule ||
        !g_liveRaycast.eventRaycastAll || !g_liveRaycast.pointerGetPosition || !g_liveRaycast.pointerSetPosition) {
        SetText(detail, cap, L"Live EventSystem/InputModule signature chưa resolve");
        return false;
    }
    g_liveRaycast.ready = true;
    return true;
}

bool GetLivePointerEventData(Il2CppObject* inputModule, Il2CppObject*& pointerData,
                             wchar_t* detail, std::size_t cap) {
    pointerData = nullptr;
    if (!inputModule) return false;
    Il2CppClass* moduleClass = g_api.object_get_class(inputModule);
    const MethodInfo* getLast = moduleClass ? FindMethod(moduleClass, "GetLastPointerEventData", 1) : nullptr;
    if (!getLast || StaticMethod(getLast) || !ParamType(getLast, 0, "System.Int32")) {
        SetText(detail, cap, L"PointerInputModule.GetLastPointerEventData(int) chưa resolve");
        return false;
    }
    for (std::int32_t id : {-1, 0}) {
        void* args[] = {&id};
        Il2CppObject* candidate = nullptr;
        wchar_t inner[192]{};
        if (InvokeObjectArgs(getLast, inputModule, args, candidate, inner, _countof(inner)) && candidate &&
            AssignableObject(g_liveRaycast.pointerEventData, candidate)) {
            pointerData = candidate;
            return true;
        }
    }
    SetText(detail, cap, L"PointerInputModule chưa có live PointerEventData; rê chuột trong game rồi F8");
    return false;
}

bool GetLiveRaycastCache(Il2CppObject* inputModule, Il2CppObject*& cache,
                         wchar_t* detail, std::size_t cap) {
    cache = nullptr;
    Il2CppClass* moduleClass = inputModule ? g_api.object_get_class(inputModule) : nullptr;
    FieldInfo* cacheField = moduleClass ? FindField(moduleClass, "m_RaycastResultCache") : nullptr;
    if (!cacheField) {
        SetText(detail, cap, L"BaseInputModule.m_RaycastResultCache chưa resolve");
        return false;
    }
    g_api.field_get_value(inputModule, cacheField, &cache);
    if (!cache) {
        SetText(detail, cap, L"m_RaycastResultCache hiện null");
        return false;
    }
    return true;
}

bool ExtractLiveRaycastGameObject(Il2CppObject* boxedResult, Il2CppClass* resultClass,
                                  FieldInfo* gameField, const MethodInfo* getGameObject,
                                  Il2CppObject*& gameObject) {
    gameObject = nullptr;
    if (!boxedResult || !resultClass) return false;
    wchar_t ignored[128]{};
    if (getGameObject &&
        InvokeObject(getGameObject, ManagedThis(boxedResult), gameObject, ignored, _countof(ignored)) &&
        gameObject && AssignableObject(g_ui.unityGameObject, gameObject)) return true;
    gameObject = nullptr;
    if (gameField) {
        g_api.field_get_value(boxedResult, gameField, &gameObject);
        if (gameObject && AssignableObject(g_ui.unityGameObject, gameObject)) return true;
    }
    gameObject = nullptr;
    return false;
}

bool RaycastRawGameObjects(int normalizedX, int normalizedY,
                           std::vector<Il2CppObject*>& hits,
                           EventRaycastStats& stats,
                           wchar_t* detail, std::size_t cap) {
    hits.clear();
    stats = {};
    if (!EnsureLivePointerRaycast(detail, cap)) return false;
    if (!BuildUnityScreenPoint(normalizedX, normalizedY, stats.unityPoint, detail, cap)) return false;

    Il2CppObject* current = nullptr;
    wchar_t inner[256]{};
    if (!InvokeObject(g_liveRaycast.eventGetCurrent, nullptr, current, inner, _countof(inner)) || !current) {
        SetText(detail, cap, L"EventSystem.current FAIL • "); Append(detail, cap, inner); return false;
    }

    Il2CppObject* inputModule = nullptr;
    inner[0] = 0;
    if (!InvokeObject(g_liveRaycast.eventGetCurrentInputModule, current, inputModule, inner, _countof(inner)) || !inputModule) {
        SetText(detail, cap, L"EventSystem.currentInputModule FAIL • "); Append(detail, cap, inner); return false;
    }

    Il2CppObject* pointerData = nullptr;
    if (!GetLivePointerEventData(inputModule, pointerData, detail, cap)) return false;

    Il2CppObject* cache = nullptr;
    if (!GetLiveRaycastCache(inputModule, cache, detail, cap)) return false;
    Il2CppClass* listClass = g_api.object_get_class(cache);
    const MethodInfo* clear = listClass ? FindMethod(listClass, "Clear", 0) : nullptr;
    const MethodInfo* getCount = listClass ? FindMethod(listClass, "get_Count", 0) : nullptr;
    const MethodInfo* getItem = listClass ? FindMethod(listClass, "get_Item", 1) : nullptr;
    if (!clear || !getCount || !getItem || !ParamType(getItem, 0, "System.Int32")) {
        SetText(detail, cap, L"Live RaycastResult cache methods chưa resolve");
        return false;
    }

    const Il2CppType* itemType = g_api.method_get_return_type(getItem);
    Il2CppClass* resultClass = itemType ? g_api.class_from_type(itemType) : nullptr;
    FieldInfo* gameField = resultClass ? FindField(resultClass, "gameObject") : nullptr;
    const MethodInfo* getGameObject = resultClass ? FindMethod(resultClass, "get_gameObject", 0) : nullptr;
    if (!resultClass || (!gameField && !getGameObject)) {
        SetText(detail, cap, L"RaycastResult.gameObject chưa resolve");
        return false;
    }

    Il2CppObject* oldPositionBox = nullptr;
    inner[0] = 0;
    if (!InvokeObject(g_liveRaycast.pointerGetPosition, pointerData, oldPositionBox, inner, _countof(inner)) || !oldPositionBox) {
        SetText(detail, cap, L"PointerEventData.get_position FAIL • "); Append(detail, cap, inner); return false;
    }
    void* oldRaw = g_api.object_unbox(oldPositionBox);
    if (!oldRaw) { SetText(detail, cap, L"PointerEventData.position unbox FAIL"); return false; }
    UnityVector2 oldPosition = *reinterpret_cast<const UnityVector2*>(oldRaw);

    if (!InvokeStageVoid(clear, cache, nullptr, L"RaycastCache.Clear(before) FAIL", detail, cap)) return false;

    UnityVector2 point = stats.unityPoint;
    void* setArgs[] = {&point};
    if (!InvokeStageVoid(g_liveRaycast.pointerSetPosition, pointerData, setArgs,
                         L"PointerEventData.set_position FAIL", detail, cap)) return false;

    void* rayArgs[] = {&pointerData, &cache};
    bool rayOk = InvokeStageVoid(g_liveRaycast.eventRaycastAll, current, rayArgs,
                                 L"EventSystem.RaycastAll FAIL", detail, cap);

    void* restoreArgs[] = {&oldPosition};
    wchar_t restoreDetail[256]{};
    bool restoreOk = InvokeVoid(g_liveRaycast.pointerSetPosition, pointerData, restoreArgs,
                                restoreDetail, _countof(restoreDetail));
    if (!rayOk) {
        wchar_t ignored[128]{}; (void)InvokeVoid(clear, cache, nullptr, ignored, _countof(ignored));
        return false;
    }
    if (!restoreOk) {
        wchar_t ignored[128]{}; (void)InvokeVoid(clear, cache, nullptr, ignored, _countof(ignored));
        SetText(detail, cap, L"PointerEventData.restore_position FAIL • "); Append(detail, cap, restoreDetail);
        return false;
    }

    std::int64_t count64 = 0;
    inner[0] = 0;
    if (!InvokeScalar(getCount, cache, count64, inner, _countof(inner)) || count64 < 0 || count64 > 4096) {
        wchar_t ignored[128]{}; (void)InvokeVoid(clear, cache, nullptr, ignored, _countof(ignored));
        SetText(detail, cap, L"Raycast cache Count FAIL • "); Append(detail, cap, inner); return false;
    }
    const int count = static_cast<int>(count64);
    stats.raycastHits = count;
    const int limit = std::min(count, 64);
    hits.reserve(static_cast<std::size_t>(limit));
    for (int i = 0; i < limit; ++i) {
        std::int32_t index = i;
        void* itemArgs[] = {&index};
        Il2CppObject* boxed = nullptr;
        wchar_t itemDetail[128]{};
        if (!InvokeObjectArgs(getItem, cache, itemArgs, boxed, itemDetail, _countof(itemDetail)) || !boxed) continue;
        Il2CppObject* gameObject = nullptr;
        if (!ExtractLiveRaycastGameObject(boxed, resultClass, gameField, getGameObject, gameObject)) continue;
        hits.push_back(gameObject);
    }

    if (!InvokeStageVoid(clear, cache, nullptr, L"RaycastCache.Clear(after) FAIL", detail, cap)) return false;
    if (hits.empty()) {
        SetText(detail, cap, L"EventSystem live raycast không trả GameObject UI tại điểm F8");
        Append(detail, cap, L" • rawCount="); AppendInt(detail, cap, count);
        return false;
    }
    return true;
}

bool GameObjectForControl(const UiControl& control, Il2CppObject*& gameObject) {
    gameObject = nullptr;
    Il2CppObject* rectTransform = nullptr;
    if (!ResolveRectTransform(control.object, control.klass, rectTransform) || !rectTransform) return false;
    Il2CppClass* klass = g_api.object_get_class(rectTransform);
    const MethodInfo* getter = klass ? FindMethod(klass, "get_gameObject", 0) : nullptr;
    wchar_t ignored[128]{};
    return getter && InvokeObject(getter, rectTransform, gameObject, ignored, _countof(ignored)) &&
           gameObject && AssignableObject(g_ui.unityGameObject, gameObject);
}

bool ParentGameObject(Il2CppObject* gameObject, Il2CppObject*& parentGameObject) {
    parentGameObject = nullptr;
    if (!gameObject) return false;
    Il2CppClass* gameClass = g_api.object_get_class(gameObject);
    const MethodInfo* getTransform = gameClass ? FindMethod(gameClass, "get_transform", 0) : nullptr;
    Il2CppObject* transform = nullptr; wchar_t ignored[128]{};
    if (!getTransform || !InvokeObject(getTransform, gameObject, transform, ignored, _countof(ignored)) || !transform) return false;
    Il2CppClass* transformClass = g_api.object_get_class(transform);
    const MethodInfo* getParent = transformClass ? FindMethod(transformClass, "get_parent", 0) : nullptr;
    Il2CppObject* parentTransform = nullptr;
    if (!getParent || !InvokeObject(getParent, transform, parentTransform, ignored, _countof(ignored)) || !parentTransform) return false;
    Il2CppClass* parentClass = g_api.object_get_class(parentTransform);
    const MethodInfo* getGameObject = parentClass ? FindMethod(parentClass, "get_gameObject", 0) : nullptr;
    return getGameObject && InvokeObject(getGameObject, parentTransform, parentGameObject, ignored, _countof(ignored)) &&
           parentGameObject && AssignableObject(g_ui.unityGameObject, parentGameObject);
}

void MapRaycastGameObject(Il2CppObject* hitGameObject, int raycastOrder,
                          const std::vector<Il2CppObject*>& controlGameObjects,
                          std::vector<MappedRaycast>& mapped) {
    Il2CppObject* current = hitGameObject;
    std::vector<Il2CppObject*> seen;
    for (int distance = 0; current && distance < 24; ++distance) {
        if (std::find(seen.begin(), seen.end(), current) != seen.end()) break;
        seen.push_back(current);
        for (std::size_t i = 0; i < controlGameObjects.size(); ++i) {
            if (controlGameObjects[i] != current) continue;
            const bool already = std::any_of(mapped.begin(), mapped.end(), [i](const MappedRaycast& m) {
                return m.objectIndex == static_cast<int>(i);
            });
            if (!already) mapped.push_back({static_cast<int>(i), raycastOrder, distance});
        }
        Il2CppObject* parent = nullptr;
        if (!ParentGameObject(current, parent)) break;
        current = parent;
    }
}

bool RaycastUiObjectsAtPoint(int normalizedX, int normalizedY,
                             std::vector<UiControl>& objects,
                             std::vector<MappedRaycast>& mapped,
                             EventRaycastStats& stats,
                             wchar_t* detail, std::size_t cap) {
    mapped.clear();
    std::vector<Il2CppObject*> raycastGameObjects;
    if (!RaycastRawGameObjects(normalizedX, normalizedY, raycastGameObjects, stats, detail, cap)) return false;
    if (!EnumerateActiveUiObjects(objects, detail, cap)) return false;
    stats.totalObjects = static_cast<int>(objects.size());

    std::vector<Il2CppObject*> controlGameObjects(objects.size(), nullptr);
    for (std::size_t i = 0; i < objects.size(); ++i) {
        Il2CppObject* gameObject = nullptr;
        if (GameObjectForControl(objects[i], gameObject)) {
            controlGameObjects[i] = gameObject;
            ++stats.boundObjects;
        }
    }
    for (std::size_t order = 0; order < raycastGameObjects.size(); ++order)
        MapRaycastGameObject(raycastGameObjects[order], static_cast<int>(order), controlGameObjects, mapped);

    stats.mappedObjects = static_cast<int>(mapped.size());
    for (const auto& m : mapped) {
        if (m.objectIndex >= 0 && static_cast<std::size_t>(m.objectIndex) < objects.size() &&
            objects[static_cast<std::size_t>(m.objectIndex)].directCallable) ++stats.callableMapped;
    }
    return true;
}

void AppendRaycastStats(wchar_t* detail, std::size_t cap, const EventRaycastStats& stats) {
    Append(detail, cap, L" • raycastHits="); AppendInt(detail, cap, stats.raycastHits);
    Append(detail, cap, L" uiObjects="); AppendInt(detail, cap, stats.totalObjects);
    Append(detail, cap, L" bound="); AppendInt(detail, cap, stats.boundObjects);
    Append(detail, cap, L" mapped="); AppendInt(detail, cap, stats.mappedObjects);
    Append(detail, cap, L" callableMapped="); AppendInt(detail, cap, stats.callableMapped);
    Append(detail, cap, L" UnityScreenPoint=");
    wchar_t pointText[96]{};
    swprintf_s(pointText, L"%.1f,%.1f", stats.unityPoint.x, stats.unityPoint.y);
    Append(detail, cap, pointText);
}

bool FindEventSystemControlAtPoint(int normalizedX, int normalizedY, bool requireDirect,
                                   UiControl& selected, bool& ambiguous,
                                   EventRaycastStats& stats,
                                   wchar_t* detail, std::size_t cap) {
    ambiguous = false;
    std::vector<UiControl> objects;
    std::vector<MappedRaycast> mapped;
    if (!RaycastUiObjectsAtPoint(normalizedX, normalizedY, objects, mapped, stats, detail, cap)) return false;

    std::vector<probe_logic::RaycastCandidate> callable;
    callable.reserve(mapped.size());
    for (const auto& m : mapped) {
        if (m.objectIndex < 0 || static_cast<std::size_t>(m.objectIndex) >= objects.size()) continue;
        const UiControl& c = objects[static_cast<std::size_t>(m.objectIndex)];
        callable.push_back({m.objectIndex, m.raycastOrder, m.ancestorDistance, c.directCallable, c.identity});
    }
    const auto pick = probe_logic::ChooseRaycastCandidate(callable);
    if (pick.status == probe_logic::PickStatus::Ambiguous) {
        ambiguous = true;
        SetText(detail, cap, L"AMBIGUOUS • EventSystem map ra nhiều callable UIObject đồng hạng; fail-closed");
        AppendRaycastStats(detail, cap, stats);
        return false;
    }
    if (pick.status == probe_logic::PickStatus::Selected && pick.index >= 0 &&
        static_cast<std::size_t>(pick.index) < objects.size()) {
        selected = std::move(objects[static_cast<std::size_t>(pick.index)]);
        return true;
    }
    if (requireDirect) {
        SetText(detail, cap, L"EventSystem raycast có target nhưng chưa map được callable UIObject");
        AppendRaycastStats(detail, cap, stats);
        return false;
    }

    if (mapped.empty()) {
        SetText(detail, cap, L"EventSystem raycast có GameObject nhưng không map được UIObject");
        AppendRaycastStats(detail, cap, stats);
        return false;
    }
    std::stable_sort(mapped.begin(), mapped.end(), [&objects](const MappedRaycast& a, const MappedRaycast& b) {
        if (a.raycastOrder != b.raycastOrder) return a.raycastOrder < b.raycastOrder;
        if (a.ancestorDistance != b.ancestorDistance) return a.ancestorDistance < b.ancestorDistance;
        const int ad = (a.objectIndex >= 0 && static_cast<std::size_t>(a.objectIndex) < objects.size()) ? objects[static_cast<std::size_t>(a.objectIndex)].depth : -1;
        const int bd = (b.objectIndex >= 0 && static_cast<std::size_t>(b.objectIndex) < objects.size()) ? objects[static_cast<std::size_t>(b.objectIndex)].depth : -1;
        return ad > bd;
    });
    const int index = mapped.front().objectIndex;
    if (index < 0 || static_cast<std::size_t>(index) >= objects.size()) return false;
    selected = std::move(objects[static_cast<std::size_t>(index)]);
    return true;
}
