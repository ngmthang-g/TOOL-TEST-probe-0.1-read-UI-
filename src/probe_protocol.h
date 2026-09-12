#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace tlprobe {

constexpr std::uint32_t kMagic = 0x42525054u; // TPRB
constexpr std::uint32_t kProtocolVersion = 0x00010300u;
constexpr UINT kWakeMessage = WM_APP + 0x641;
constexpr wchar_t kMappingPrefix[] = L"Local\\ThanLongUiProbe_";
constexpr int kCoordinateScale = 100000;
constexpr std::size_t kMaxUiRows = 160;

enum class Command : std::uint32_t {
    None = 0,
    ScanUi = 1,
    PickAtPoint = 2,
    DirectInvokeAtPoint = 3,
    InputSyncClickAtPoint = 4,
};

enum class UiKind : std::int32_t {
    Unknown = 0,
    Button = 1,
    Toggle = 2,
    Rect = 3,
    Other = 4,
};

enum class ResultCode : std::int32_t {
    None = 0,
    SnapshotReady = 1,
    Picked = 2,
    Ambiguous = 3,
    DirectDispatched = 4,
    InputSyncDispatched = 5,
};

struct UiRow {
    std::uint64_t identity = 0;
    std::int32_t kind = 0;
    std::int32_t depth = 0;
    float area = 0.0f;
    float rectX = 0.0f;
    float rectY = 0.0f;
    float rectWidth = 0.0f;
    float rectHeight = 0.0f;
    std::int32_t hasGeometry = 0;
    std::int32_t directCallable = 0;
    std::int32_t pointerCallable = 0;
    wchar_t className[64]{};
    wchar_t name[96]{};
    wchar_t text[128]{};
    wchar_t tag[96]{};
    wchar_t handler[128]{};
    wchar_t ancestors[192]{};
    wchar_t descendants[192]{};
};

struct UiSnapshot {
    std::int32_t totalCount = 0;
    std::int32_t rowCount = 0;
    std::int32_t truncated = 0;
    UiRow rows[kMaxUiRows]{};
};

struct ProbeRequest {
    std::uint32_t command = 0;
    std::int32_t arg0 = 0;
    std::int32_t arg1 = 0;
    std::int32_t arg2 = 0;
};

struct ProbeResponse {
    std::int32_t ok = 0;
    std::int32_t resultCode = 0;
    std::int32_t pickedIndex = -1;
    std::int32_t value0 = 0;
    std::int32_t value1 = 0;
    UiRow picked{};
    UiSnapshot snapshot{};
    wchar_t detail[768]{};
};

struct SharedBlock {
    std::uint32_t magic = kMagic;
    std::uint32_t protocolVersion = kProtocolVersion;
    std::uint32_t targetPid = 0;
    std::uint32_t targetWindowThreadId = 0;
    volatile LONG requestSeq = 0;
    volatile LONG completedSeq = 0;
    volatile LONG bridgeLoaded = 0;
    volatile LONG bridgeBusy = 0;
    ProbeRequest request{};
    ProbeResponse response{};
};

inline void MappingName(DWORD pid, wchar_t* output, std::size_t count) {
    if (!output || count == 0) return;
    wsprintfW(output, L"%s%lu", kMappingPrefix, static_cast<unsigned long>(pid));
}

} // namespace tlprobe
