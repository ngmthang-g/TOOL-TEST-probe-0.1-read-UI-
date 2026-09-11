#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "probe_logic.h"
#include "probe_protocol.h"

using namespace tlprobe;

namespace {

using Il2CppDomain = void;
using Il2CppAssembly = void;
using Il2CppImage = void;
using Il2CppClass = void;
using MethodInfo = void;
using FieldInfo = void;
using Il2CppType = void;
using Il2CppObject = void;
using Il2CppString = void;

HANDLE g_mapping = nullptr;
SharedBlock* g_shared = nullptr;

template <typename T>
bool Resolve(HMODULE module, const char* name, T& out) {
    out = nullptr;
    FARPROC p = GetProcAddress(module, name);
    if (!p) return false;
    static_assert(sizeof(p) == sizeof(out), "pointer-size mismatch");
    const unsigned char* src = reinterpret_cast<const unsigned char*>(&p);
    unsigned char* dst = reinterpret_cast<unsigned char*>(&out);
    for (std::size_t i = 0; i < sizeof(out); ++i) dst[i] = src[i];
    return out != nullptr;
}

bool Eq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) if (*a++ != *b++) return false;
    return *a == *b;
}

void SetText(wchar_t* out, std::size_t cap, const wchar_t* value) {
    if (!out || cap == 0) return;
    std::size_t i = 0;
    if (value) while (i + 1 < cap && value[i]) { out[i] = value[i]; ++i; }
    out[i] = 0;
}

void Append(wchar_t* out, std::size_t cap, const wchar_t* value) {
    if (!out || !value || cap == 0) return;
    std::size_t n = 0;
    while (n + 1 < cap && out[n]) ++n;
    std::size_t i = 0;
    while (n + 1 < cap && value[i]) out[n++] = value[i++];
    out[n] = 0;
}

void AppendInt(wchar_t* out, std::size_t cap, int value) {
    wchar_t tmp[32]{};
    wsprintfW(tmp, L"%d", value);
    Append(out, cap, tmp);
}

void CopyWide(wchar_t* out, std::size_t cap, const std::wstring& value) {
    SetText(out, cap, value.c_str());
}

std::wstring WideFromUtf8(const char* value) {
    if (!value || !*value) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring out(static_cast<std::size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value, -1, out.data(), n);
    return out;
}

std::string Utf8FromWide(const std::wstring& value) {
    if (value.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), n, nullptr, nullptr);
    return out;
}

struct Api {
    HMODULE module = nullptr;
    Il2CppDomain* (__cdecl* domain_get)() = nullptr;
    const Il2CppAssembly* (__cdecl* domain_assembly_open)(Il2CppDomain*, const char*) = nullptr;
    const Il2CppImage* (__cdecl* assembly_get_image)(const Il2CppAssembly*) = nullptr;
    Il2CppClass* (__cdecl* class_from_name)(const Il2CppImage*, const char*, const char*) = nullptr;
    const MethodInfo* (__cdecl* class_get_method_from_name)(Il2CppClass*, const char*, int) = nullptr;
    Il2CppClass* (__cdecl* class_get_parent)(Il2CppClass*) = nullptr;
    std::uint32_t (__cdecl* method_get_flags)(const MethodInfo*, std::uint32_t*) = nullptr;
    std::uint32_t (__cdecl* method_get_param_count)(const MethodInfo*) = nullptr;
    const Il2CppType* (__cdecl* method_get_param)(const MethodInfo*, std::uint32_t) = nullptr;
    const Il2CppType* (__cdecl* method_get_return_type)(const MethodInfo*) = nullptr;
    char* (__cdecl* type_get_name)(const Il2CppType*) = nullptr;
    void (__cdecl* free_fn)(void*) = nullptr;
    Il2CppObject* (__cdecl* runtime_invoke)(const MethodInfo*, void*, void**, void**) = nullptr;
    void* (__cdecl* object_unbox)(Il2CppObject*) = nullptr;
    Il2CppClass* (__cdecl* object_get_class)(Il2CppObject*) = nullptr;
    FieldInfo* (__cdecl* class_get_field_from_name)(Il2CppClass*, const char*) = nullptr;
    const Il2CppType* (__cdecl* field_get_type)(FieldInfo*) = nullptr;
    void (__cdecl* field_get_value)(Il2CppObject*, FieldInfo*, void*) = nullptr;
    Il2CppClass* (__cdecl* class_from_type)(const Il2CppType*) = nullptr;
    bool (__cdecl* class_is_valuetype)(const Il2CppClass*) = nullptr;
    std::int32_t (__cdecl* string_length)(Il2CppString*) = nullptr;
    const wchar_t* (__cdecl* string_chars)(Il2CppString*) = nullptr;
    bool (__cdecl* class_is_assignable_from)(Il2CppClass*, Il2CppClass*) = nullptr;
    void (__cdecl* field_static_get_value)(FieldInfo*, void*) = nullptr;
    const Il2CppImage* (__cdecl* get_corlib)() = nullptr;
    Il2CppObject* (__cdecl* array_new)(Il2CppClass*, std::uintptr_t) = nullptr;
    Il2CppString* (__cdecl* string_new)(const char*) = nullptr;
    std::size_t (__cdecl* image_get_class_count)(const Il2CppImage*) = nullptr;
    Il2CppClass* (__cdecl* image_get_class)(const Il2CppImage*, std::size_t) = nullptr;
    const char* (__cdecl* class_get_name)(Il2CppClass*) = nullptr;
    const MethodInfo* (__cdecl* class_get_methods)(Il2CppClass*, void**) = nullptr;
    const char* (__cdecl* method_get_name)(const MethodInfo*) = nullptr;
    bool discoveryExportsLoaded = false;
    bool luaExportsLoaded = false;

    bool Load(wchar_t* detail, std::size_t cap) {
        if (module) return true;
        module = GetModuleHandleW(L"GameAssembly.dll");
        if (!module) { SetText(detail, cap, L"GameAssembly.dll chưa sẵn sàng"); return false; }
#define NEED(symbol) do { if (!Resolve(module, "il2cpp_" #symbol, symbol)) { SetText(detail, cap, L"Thiếu IL2CPP export bắt buộc"); return false; } } while (0)
        NEED(domain_get); NEED(domain_assembly_open); NEED(assembly_get_image); NEED(class_from_name);
        NEED(class_get_method_from_name); NEED(class_get_parent); NEED(method_get_flags);
        NEED(method_get_param_count); NEED(method_get_param); NEED(method_get_return_type);
        NEED(type_get_name); NEED(runtime_invoke); NEED(object_unbox); NEED(object_get_class);
        NEED(class_get_field_from_name); NEED(field_get_type); NEED(field_get_value);
        NEED(class_from_type); NEED(class_is_valuetype); NEED(string_length); NEED(string_chars);
#undef NEED
        if (!Resolve(module, "il2cpp_free", free_fn)) { SetText(detail, cap, L"Thiếu il2cpp_free"); return false; }
        return true;
    }

    bool LoadDiscovery(wchar_t* detail, std::size_t cap) {
        if (!Load(detail, cap)) return false;
        if (discoveryExportsLoaded) return true;
        if (!Resolve(module, "il2cpp_class_is_assignable_from", class_is_assignable_from) ||
            !Resolve(module, "il2cpp_field_static_get_value", field_static_get_value) ||
            !Resolve(module, "il2cpp_class_get_name", class_get_name)) {
            SetText(detail, cap, L"UI discovery thiếu IL2CPP export bắt buộc");
            return false;
        }
        (void)Resolve(module, "il2cpp_image_get_class_count", image_get_class_count);
        (void)Resolve(module, "il2cpp_image_get_class", image_get_class);
        (void)Resolve(module, "il2cpp_class_get_methods", class_get_methods);
        (void)Resolve(module, "il2cpp_method_get_name", method_get_name);
        discoveryExportsLoaded = true;
        return true;
    }

    bool LoadLua(wchar_t* detail, std::size_t cap) {
        if (!LoadDiscovery(detail, cap)) return false;
        if (luaExportsLoaded) return true;
        if (!Resolve(module, "il2cpp_get_corlib", get_corlib) ||
            !Resolve(module, "il2cpp_array_new", array_new) ||
            !Resolve(module, "il2cpp_string_new", string_new)) {
            SetText(detail, cap, L"Lua callback thiếu IL2CPP export bắt buộc");
            return false;
        }
        luaExportsLoaded = true;
        return true;
    }
};

Api g_api;

const Il2CppImage* ImageForAssembly(const char* name, const char* dllName) {
    Il2CppDomain* domain = g_api.domain_get ? g_api.domain_get() : nullptr;
    if (!domain) return nullptr;
    const Il2CppAssembly* assembly = g_api.domain_assembly_open(domain, name);
    if (!assembly && dllName) assembly = g_api.domain_assembly_open(domain, dllName);
    return assembly ? g_api.assembly_get_image(assembly) : nullptr;
}

const Il2CppImage* MainImage() {
    return ImageForAssembly("Assembly-CSharp", "Assembly-CSharp.dll");
}

bool StaticMethod(const MethodInfo* method) {
    if (!method) return false;
    constexpr std::uint32_t kStatic = 0x0010;
    std::uint32_t iflags = 0;
    return (g_api.method_get_flags(method, &iflags) & kStatic) != 0;
}

const MethodInfo* FindMethod(Il2CppClass* klass, const char* name, int argc) {
    for (Il2CppClass* current = klass; current; current = g_api.class_get_parent(current)) {
        if (const MethodInfo* method = g_api.class_get_method_from_name(current, name, argc)) return method;
    }
    return nullptr;
}

bool ParamType(const MethodInfo* method, std::uint32_t index, const char* expected) {
    if (!method || index >= g_api.method_get_param_count(method)) return false;
    const Il2CppType* type = g_api.method_get_param(method, index);
    char* name = type ? g_api.type_get_name(type) : nullptr;
    if (!name) return false;
    const bool ok = Eq(name, expected);
    g_api.free_fn(name);
    return ok;
}

bool ReturnType(const MethodInfo* method, const char* expected) {
    if (!method) return false;
    const Il2CppType* type = g_api.method_get_return_type(method);
    char* name = type ? g_api.type_get_name(type) : nullptr;
    if (!name) return false;
    const bool ok = Eq(name, expected);
    g_api.free_fn(name);
    return ok;
}

bool FieldType(FieldInfo* field, const char* expected) {
    if (!field) return false;
    const Il2CppType* type = g_api.field_get_type(field);
    char* name = type ? g_api.type_get_name(type) : nullptr;
    if (!name) return false;
    const bool ok = Eq(name, expected);
    g_api.free_fn(name);
    return ok;
}

const MethodInfo* ExactMethod(Il2CppClass* klass, const char* name, int argc, bool isStatic,
                              const char* p0 = nullptr, const char* p1 = nullptr,
                              const char* p2 = nullptr) {
    const MethodInfo* method = FindMethod(klass, name, argc);
    if (!method || StaticMethod(method) != isStatic) return nullptr;
    if (argc > 0 && p0 && !ParamType(method, 0, p0)) return nullptr;
    if (argc > 1 && p1 && !ParamType(method, 1, p1)) return nullptr;
    if (argc > 2 && p2 && !ParamType(method, 2, p2)) return nullptr;
    return method;
}

bool InvokeObjectArgs(const MethodInfo* method, void* instance, void** args,
                      Il2CppObject*& out, wchar_t* detail, std::size_t cap) {
    out = nullptr;
    if (!method) { SetText(detail, cap, L"Object method chưa resolve"); return false; }
    void* exc = nullptr;
    out = g_api.runtime_invoke(method, instance, args, &exc);
    if (exc) { SetText(detail, cap, L"Managed exception ở object method"); return false; }
    return true;
}

bool InvokeObject(const MethodInfo* method, void* instance, Il2CppObject*& out,
                  wchar_t* detail, std::size_t cap) {
    return InvokeObjectArgs(method, instance, nullptr, out, detail, cap);
}

void* ManagedThis(Il2CppObject* object) {
    if (!object) return nullptr;
    Il2CppClass* klass = g_api.object_get_class(object);
    if (klass && g_api.class_is_valuetype(klass)) {
        if (void* raw = g_api.object_unbox(object)) return raw;
    }
    return object;
}

bool InvokeScalarArgs(const MethodInfo* method, void* instance, void** args,
                      std::int64_t& out, wchar_t* detail, std::size_t cap) {
    out = 0;
    if (!method) { SetText(detail, cap, L"Scalar method chưa resolve"); return false; }
    const Il2CppType* returnType = g_api.method_get_return_type(method);
    char* typeName = returnType ? g_api.type_get_name(returnType) : nullptr;
    if (!typeName) { SetText(detail, cap, L"Không đọc được scalar return type"); return false; }
    void* exc = nullptr;
    Il2CppObject* boxed = g_api.runtime_invoke(method, instance, args, &exc);
    if (exc || !boxed) { g_api.free_fn(typeName); SetText(detail, cap, L"Scalar invoke lỗi/null"); return false; }
    void* raw = g_api.object_unbox(boxed);
    if (!raw) { g_api.free_fn(typeName); SetText(detail, cap, L"Không unbox scalar"); return false; }
    bool ok = true;
    if (Eq(typeName, "System.Boolean")) out = *reinterpret_cast<const std::uint8_t*>(raw) ? 1 : 0;
    else if (Eq(typeName, "System.Int32")) out = *reinterpret_cast<const std::int32_t*>(raw);
    else if (Eq(typeName, "System.UInt32")) out = *reinterpret_cast<const std::uint32_t*>(raw);
    else if (Eq(typeName, "System.Int64")) out = *reinterpret_cast<const std::int64_t*>(raw);
    else if (Eq(typeName, "System.UInt64")) {
        const auto v = *reinterpret_cast<const std::uint64_t*>(raw);
        if (v > static_cast<std::uint64_t>(INT64_MAX)) ok = false;
        else out = static_cast<std::int64_t>(v);
    } else ok = false;
    g_api.free_fn(typeName);
    if (!ok) SetText(detail, cap, L"Scalar return type chưa hỗ trợ");
    return ok;
}

bool InvokeScalar(const MethodInfo* method, void* instance, std::int64_t& out,
                  wchar_t* detail, std::size_t cap) {
    return InvokeScalarArgs(method, instance, nullptr, out, detail, cap);
}

bool ScalarGetter(Il2CppClass* klass, const char* name, void* instance, std::int32_t& out,
                  wchar_t* detail, std::size_t cap) {
    std::int64_t v = 0;
    if (!InvokeScalar(FindMethod(klass, name, 0), instance, v, detail, cap)) return false;
    if (v < INT32_MIN || v > INT32_MAX) return false;
    out = static_cast<std::int32_t>(v);
    return true;
}

bool StaticScalar(Il2CppClass* klass, const char* name, std::int32_t& out,
                  wchar_t* detail, std::size_t cap) {
    const MethodInfo* method = FindMethod(klass, name, 0);
    if (!method || !StaticMethod(method)) return false;
    return ScalarGetter(klass, name, nullptr, out, detail, cap);
}

bool InvokeVoid(const MethodInfo* method, void* instance, void** args,
                wchar_t* detail, std::size_t cap) {
    if (!method) { SetText(detail, cap, L"Action method chưa resolve"); return false; }
    void* exc = nullptr;
    (void)g_api.runtime_invoke(method, instance, args, &exc);
    if (exc) { SetText(detail, cap, L"Action ném managed exception"); return false; }
    return true;
}

bool CopyString(Il2CppString* value, wchar_t* out, std::size_t cap) {
    if (!value || !out || cap == 0) return false;
    const int len = g_api.string_length(value);
