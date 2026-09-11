    const wchar_t* chars = g_api.string_chars(value);
    if (len < 0 || len > 8192 || !chars) return false;
    std::size_t n = static_cast<std::size_t>(len);
    if (n + 1 > cap) n = cap - 1;
    for (std::size_t i = 0; i < n; ++i) out[i] = chars[i];
    out[n] = 0;
    return true;
}

template <typename T>
bool ReadLocal(const void* base, std::size_t offset, T& value) {
    if (!base) return false;
    SIZE_T done = 0;
    const auto* address = reinterpret_cast<const unsigned char*>(base) + offset;
    return ReadProcessMemory(GetCurrentProcess(), address, &value, sizeof(value), &done) != FALSE && done == sizeof(value);
}

template <typename T>
bool WriteLocal(void* base, std::size_t offset, const T& value) {
    if (!base) return false;
    SIZE_T done = 0;
    auto* address = reinterpret_cast<unsigned char*>(base) + offset;
    return WriteProcessMemory(GetCurrentProcess(), address, &value, sizeof(value), &done) != FALSE && done == sizeof(value);
}

FieldInfo* FindField(Il2CppClass* klass, const char* name) {
    for (Il2CppClass* current = klass; current; current = g_api.class_get_parent(current)) {
        if (FieldInfo* field = g_api.class_get_field_from_name(current, name)) return field;
    }
    return nullptr;
}

enum class LocalKind { Other, Button, Toggle, Rect };

struct Labels {
    std::wstring name;
    std::wstring text;
    std::wstring tag;
    std::wstring handler;
    std::wstring ancestors;
    std::wstring descendants;
};

struct UnityVector2 { float x = 0.0f; float y = 0.0f; };
struct UnityRectValue { float x = 0.0f; float y = 0.0f; float width = 0.0f; float height = 0.0f; };

struct UiControl {
    Il2CppObject* object = nullptr;
    Il2CppClass* klass = nullptr;
    LocalKind kind = LocalKind::Other;
    Labels labels{};
    std::wstring className;
    int depth = 0;
    float rectX = 0.0f;
    float rectY = 0.0f;
    float rectW = 0.0f;
    float rectH = 0.0f;
    float area = 0.0f;
    bool hasGeometry = false;
    bool directCallable = false;
    bool pointerCallable = false;
    std::uint64_t identity = 0;
};

struct UiRuntime {
    bool discoveryReady = false;
    bool luaReady = false;
    bool geometryReady = false;
    bool inputReady = false;
    const Il2CppImage* image = nullptr;
    Il2CppClass* uiObject = nullptr;
    Il2CppClass* button = nullptr;
    Il2CppClass* toggle = nullptr;
    Il2CppClass* rect = nullptr;
    Il2CppClass* executor = nullptr;
    Il2CppClass* guiApi = nullptr;
    Il2CppClass* systemObject = nullptr;
    FieldInfo* instances = nullptr;
    std::vector<std::pair<Il2CppClass*, LocalKind>> kindCache{};

    const Il2CppImage* coreImage = nullptr;
    const Il2CppImage* uiModuleImage = nullptr;
    const Il2CppImage* legacyUnityImage = nullptr;
    const Il2CppImage* eventSystemsImage = nullptr;
    Il2CppClass* unityRectTransform = nullptr;
    Il2CppClass* unityTransform = nullptr;
    Il2CppClass* unityGameObject = nullptr;
    Il2CppClass* rectTransformUtility = nullptr;
    Il2CppClass* unityScreen = nullptr;

    bool eventSystemReady = false;
    Il2CppClass* eventSystem = nullptr;
    Il2CppClass* pointerEventData = nullptr;
    Il2CppClass* raycastListClass = nullptr;
    Il2CppClass* raycastResultClass = nullptr;
    const MethodInfo* eventGetCurrent = nullptr;
    const MethodInfo* eventRaycastAll = nullptr;
    const MethodInfo* pointerCtor = nullptr;
    const MethodInfo* pointerSetPosition = nullptr;
    const MethodInfo* raycastListCtor = nullptr;
    const MethodInfo* raycastListCount = nullptr;
    const MethodInfo* raycastListGetItem = nullptr;
    FieldInfo* raycastResultGameObject = nullptr;
    const MethodInfo* raycastResultGetGameObject = nullptr;

    Il2CppClass* inputSyncManager = nullptr;
    const MethodInfo* inputGetInstance = nullptr;
    const MethodInfo* inputPress = nullptr;
    const MethodInfo* inputRelease = nullptr;
    const MethodInfo* inputCancel = nullptr;
    FieldInfo* inputDragging = nullptr;
};

UiRuntime g_ui;

bool IsUiObjectClass(Il2CppClass* klass) { return klass && FindField(klass, "instances"); }
bool IsButtonClass(Il2CppClass* klass) { return klass && ExactMethod(klass, "HandleClickEvent", 0, false); }
bool IsToggleClass(Il2CppClass* klass) {
    return klass && (ExactMethod(klass, "set_Selected", 1, false, "System.Boolean") ||
                     ExactMethod(klass, "HandleSelectEvent", 1, false, "System.Boolean"));
}
bool IsRectClass(Il2CppClass* klass) { return klass && ExactMethod(klass, "get_PointerClickHandler", 0, false); }
bool IsExecutorClass(Il2CppClass* klass) {
    return klass && ExactMethod(klass, "get_Instance", 0, true) && FindMethod(klass, "ExecuteScriptFunction", 3);
}

void FindUiClassesByMetadata(const Il2CppImage* image) {
    if (!image || !g_api.image_get_class_count || !g_api.image_get_class || !g_api.class_get_name) return;
    const std::size_t count = g_api.image_get_class_count(image);
    if (count == 0 || count > 65536) return;
    for (std::size_t i = 0; i < count; ++i) {
        Il2CppClass* klass = g_api.image_get_class(image, i);
        const char* name = klass ? g_api.class_get_name(klass) : nullptr;
        if (!name) continue;
        if (!g_ui.uiObject && Eq(name, "UIObject") && IsUiObjectClass(klass)) g_ui.uiObject = klass;
        else if (!g_ui.button && Eq(name, "UIButton") && IsButtonClass(klass)) g_ui.button = klass;
        else if (!g_ui.toggle && Eq(name, "UIToggle") && IsToggleClass(klass)) g_ui.toggle = klass;
        else if (!g_ui.rect && Eq(name, "UIRectTransform") && IsRectClass(klass)) g_ui.rect = klass;
        else if (!g_ui.executor && Eq(name, "MonoBehaviourExecutor") && IsExecutorClass(klass)) g_ui.executor = klass;
    }
}

Il2CppClass* FindExecutorClass(const Il2CppImage* image) {
    if (!image) return nullptr;
    for (const char* ns : {"FGStudio.LuaSystem", "FGStudio.LuaSystem.Base", "FGStudio.LuaSystem.GUI", "FGStudio.Engine.Utilities", ""}) {
        Il2CppClass* klass = g_api.class_from_name(image, ns, "MonoBehaviourExecutor");
        if (IsExecutorClass(klass)) return klass;
    }
    FindUiClassesByMetadata(image);
    return g_ui.executor;
}

bool EnsureUiDiscovery(wchar_t* detail, std::size_t cap) {
    if (g_ui.discoveryReady) return true;
    if (!g_api.LoadDiscovery(detail, cap)) return false;
    g_ui.image = MainImage();
    if (!g_ui.image) { SetText(detail, cap, L"Không mở được Assembly-CSharp"); return false; }
    g_ui.uiObject = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.Base", "UIObject");
    g_ui.button = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.GUI", "UIButton");
    g_ui.toggle = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.GUI", "UIToggle");
    g_ui.rect = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.GUI", "UIRectTransform");
    if (g_ui.uiObject && !IsUiObjectClass(g_ui.uiObject)) g_ui.uiObject = nullptr;
    if (g_ui.button && !IsButtonClass(g_ui.button)) g_ui.button = nullptr;
    if (g_ui.toggle && !IsToggleClass(g_ui.toggle)) g_ui.toggle = nullptr;
    if (g_ui.rect && !IsRectClass(g_ui.rect)) g_ui.rect = nullptr;
    FindUiClassesByMetadata(g_ui.image);
    g_ui.instances = g_ui.uiObject ? FindField(g_ui.uiObject, "instances") : nullptr;
    if (!g_ui.uiObject || !g_ui.instances || (!g_ui.button && !g_ui.toggle && !g_ui.rect)) {
        SetText(detail, cap, L"UIObject.instances hoặc control classes chưa resolve");
        return false;
    }
    g_ui.discoveryReady = true;
    return true;
}

bool EnsureUiLua(bool requireGui, wchar_t* detail, std::size_t cap) {
    if (g_ui.luaReady && (!requireGui || g_ui.guiApi)) return true;
    if (!EnsureUiDiscovery(detail, cap) || !g_api.LoadLua(detail, cap)) return false;
    if (!g_ui.executor) g_ui.executor = FindExecutorClass(g_ui.image);
    if (!g_ui.guiApi) g_ui.guiApi = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.API", "LuaSystemAPI_GUI");
    if (!g_ui.systemObject) {
        const Il2CppImage* corlib = g_api.get_corlib();
        g_ui.systemObject = corlib ? g_api.class_from_name(corlib, "System", "Object") : nullptr;
    }
    if (!g_ui.executor || !g_ui.systemObject || (requireGui && !g_ui.guiApi)) {
        SetText(detail, cap, L"Lua UI executor/API chưa resolve");
        return false;
    }
    g_ui.luaReady = true;
    return true;
}

bool ReadManagedPointerArray(Il2CppObject* array, std::vector<Il2CppObject*>& values, std::size_t hardLimit) {
    values.clear();
    std::uintptr_t length = 0;
    if (!array || !ReadLocal(array, 0x18, length) || length > hardLimit) return false;
    values.reserve(static_cast<std::size_t>(length));
    for (std::uintptr_t i = 0; i < length; ++i) {
        Il2CppObject* value = nullptr;
        if (!ReadLocal(array, 0x20 + static_cast<std::size_t>(i) * sizeof(void*), value)) return false;
        if (value) values.push_back(value);
    }
    return true;
}

bool EnsureManagedStringFactory() {
    return g_api.string_new || (g_api.module && Resolve(g_api.module, "il2cpp_string_new", g_api.string_new));
}

bool IsPrimitiveField(FieldInfo* field) {
    return field && (FieldType(field, "System.Boolean") || FieldType(field, "System.Byte") ||
        FieldType(field, "System.SByte") || FieldType(field, "System.Int16") ||
        FieldType(field, "System.UInt16") || FieldType(field, "System.Int32") ||
        FieldType(field, "System.UInt32") || FieldType(field, "System.Int64") ||
        FieldType(field, "System.UInt64") || FieldType(field, "System.Single") || FieldType(field, "System.Double"));
}

bool ReadUiMemberObject(Il2CppObject* object, Il2CppClass* klass, const char* member, Il2CppObject*& output) {
    output = nullptr;
    if (!object || !klass || !member) return false;
    const std::string getterName = std::string("get_") + member;
    if (const MethodInfo* getter = FindMethod(klass, getterName.c_str(), 0)) {
        wchar_t ignored[128]{};
        Il2CppObject* value = nullptr;
        if (InvokeObject(getter, ManagedThis(object), value, ignored, _countof(ignored)) && value) {
            output = value; return true;
        }
    }
    if (!g_api.class_is_valuetype(klass)) {
        if (FieldInfo* field = FindField(klass, member); field && !IsPrimitiveField(field)) {
            Il2CppObject* value = nullptr;
            g_api.field_get_value(object, field, &value);
            if (value) { output = value; return true; }
        }
    }
    if (!EnsureManagedStringFactory()) return false;
    Il2CppString* key = g_api.string_new(member);
    if (!key) return false;
    for (const char* methodName : {"get_Item", "GetValue", "Get", "RawGet"}) {
        if (const MethodInfo* method = FindMethod(klass, methodName, 1)) {
            void* args[] = {&key};
            wchar_t ignored[128]{};
            Il2CppObject* value = nullptr;
            if (InvokeObjectArgs(method, ManagedThis(object), args, value, ignored, _countof(ignored)) && value) {
                output = value; return true;
            }
        }
    }
    return false;
}

bool ManagedObjectText(Il2CppObject* object, std::wstring& output) {
    output.clear();
    if (!object) return false;
    Il2CppClass* klass = g_api.object_get_class(object);
    const char* className = klass && g_api.class_get_name ? g_api.class_get_name(klass) : nullptr;
    if (className && (Eq(className, "String") || Eq(className, "System.String"))) {
        wchar_t tmp[1024]{};
        if (!CopyString(reinterpret_cast<Il2CppString*>(object), tmp, _countof(tmp))) return false;
        output = tmp; return !output.empty();
    }
    void* raw = g_api.object_unbox(object);
    if (!className || !raw) return false;
    if (Eq(className, "Boolean") || Eq(className, "System.Boolean")) output = *reinterpret_cast<const std::uint8_t*>(raw) ? L"1" : L"0";
    else if (Eq(className, "Int32") || Eq(className, "System.Int32")) output = std::to_wstring(*reinterpret_cast<const std::int32_t*>(raw));
    else if (Eq(className, "UInt32") || Eq(className, "System.UInt32")) output = std::to_wstring(*reinterpret_cast<const std::uint32_t*>(raw));
    else if (Eq(className, "Int64") || Eq(className, "System.Int64")) output = std::to_wstring(*reinterpret_cast<const std::int64_t*>(raw));
    else if (Eq(className, "UInt64") || Eq(className, "System.UInt64")) output = std::to_wstring(*reinterpret_cast<const std::uint64_t*>(raw));
    else return false;
    return true;
}

bool ReadUiMemberText(Il2CppObject* object, Il2CppClass* klass, const char* member, std::wstring& output) {
    output.clear();
    Il2CppObject* value = nullptr;
    if (ReadUiMemberObject(object, klass, member, value) && ManagedObjectText(value, output)) return true;
    FieldInfo* field = FindField(klass, member);
    if (!field) return false;
    if (FieldType(field, "System.String")) {
        Il2CppString* str = nullptr; g_api.field_get_value(object, field, &str);
        wchar_t tmp[1024]{};
        if (!str || !CopyString(str, tmp, _countof(tmp))) return false;
        output = tmp; return true;
    }
    if (FieldType(field, "System.Boolean")) { std::uint8_t v{}; g_api.field_get_value(object, field, &v); output = v ? L"1" : L"0"; return true; }
    if (FieldType(field, "System.Int32")) { std::int32_t v{}; g_api.field_get_value(object, field, &v); output = std::to_wstring(v); return true; }
    if (FieldType(field, "System.UInt32")) { std::uint32_t v{}; g_api.field_get_value(object, field, &v); output = std::to_wstring(v); return true; }
    if (FieldType(field, "System.Int64")) { std::int64_t v{}; g_api.field_get_value(object, field, &v); output = std::to_wstring(v); return true; }
    if (FieldType(field, "System.UInt64")) { std::uint64_t v{}; g_api.field_get_value(object, field, &v); output = std::to_wstring(v); return true; }
    return false;
}

bool ReadUiString(Il2CppObject* object, Il2CppClass* klass, const char* property, std::wstring& output) {
    return ReadUiMemberText(object, klass, property, output);
}

bool ObjectGetter(Il2CppObject* object, Il2CppClass* klass, const char* name, Il2CppObject*& output) {
    const char* member = name;
    if (name && name[0]=='g' && name[1]=='e' && name[2]=='t' && name[3]=='_') member = name + 4;
    return ReadUiMemberObject(object, klass, member, output);
}

enum class ImageSlot { Core, Ui, Legacy };
enum class GeometryClass { RectTransform, Transform, GameObject, RectTransformUtility, Screen };

const Il2CppImage* GeometryImage(ImageSlot slot) {
    switch (slot) {
        case ImageSlot::Core: return g_ui.coreImage;
        case ImageSlot::Ui: return g_ui.uiModuleImage;
        case ImageSlot::Legacy: return g_ui.legacyUnityImage;
    }
    return nullptr;
}

void OpenUnityImages() {
    if (!g_ui.coreImage) g_ui.coreImage = ImageForAssembly("UnityEngine.CoreModule", "UnityEngine.CoreModule.dll");
    if (!g_ui.uiModuleImage) g_ui.uiModuleImage = ImageForAssembly("UnityEngine.UIModule", "UnityEngine.UIModule.dll");
    if (!g_ui.legacyUnityImage) g_ui.legacyUnityImage = ImageForAssembly("UnityEngine", "UnityEngine.dll");
    if (!g_ui.eventSystemsImage) g_ui.eventSystemsImage = ImageForAssembly("UnityEngine.UI", "UnityEngine.UI.dll");
}

Il2CppClass* ResolveGeometryClass(GeometryClass role) {
    const char* name = "";
    ImageSlot order[3] = {ImageSlot::Core, ImageSlot::Legacy, ImageSlot::Ui};
    if (role == GeometryClass::RectTransform) name = "RectTransform";
    else if (role == GeometryClass::Transform) name = "Transform";
    else if (role == GeometryClass::GameObject) name = "GameObject";
    else if (role == GeometryClass::Screen) name = "Screen";
    else { name = "RectTransformUtility"; order[0] = ImageSlot::Ui; order[1] = ImageSlot::Core; order[2] = ImageSlot::Legacy; }
    for (ImageSlot slot : order) {
        if (const Il2CppImage* image = GeometryImage(slot)) {
            if (Il2CppClass* klass = g_api.class_from_name(image, "UnityEngine", name)) return klass;
        }
    }
    return nullptr;
}

bool EnsureUiGeometry(wchar_t* detail, std::size_t cap) {
    if (g_ui.geometryReady) return true;
    if (!EnsureUiDiscovery(detail, cap)) return false;
    OpenUnityImages();
    g_ui.unityRectTransform = ResolveGeometryClass(GeometryClass::RectTransform);
    g_ui.unityTransform = ResolveGeometryClass(GeometryClass::Transform);
    g_ui.unityGameObject = ResolveGeometryClass(GeometryClass::GameObject);
    g_ui.rectTransformUtility = ResolveGeometryClass(GeometryClass::RectTransformUtility);
    g_ui.unityScreen = ResolveGeometryClass(GeometryClass::Screen);
    if (!g_ui.unityRectTransform || !g_ui.unityTransform || !g_ui.unityGameObject ||
