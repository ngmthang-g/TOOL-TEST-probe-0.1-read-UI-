#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "probe_logic.h"
#include "probe_protocol.h"

using namespace tlprobe;

namespace {

constexpr wchar_t kWindowClass[] = L"ThanLongUiInternalProbeV012";
constexpr wchar_t kWindowTitle[] = L"ThanLong UI Internal Probe v0.1.2 — DIRECT RETEST";
constexpr wchar_t kGameModule[] = L"GameAssembly.dll";
constexpr int kHotkeyPick = 1;
constexpr UINT_PTR kEvidenceTimer = 0x501;
constexpr DWORD kBridgeNudgeMs = 750;

enum ControlId : int {
    IDC_CLIENTS = 1001,
    IDC_REFRESH,
    IDC_ATTACH,
    IDC_SCAN,
    IDC_LIST,
    IDC_DETAIL,
    IDC_TEST_DIRECT,
    IDC_LOG,
    IDC_STATUS,
};

struct GameClient {
    DWORD pid = 0;
    DWORD threadId = 0;
    HWND window = nullptr;
    std::wstring title;
};

std::wstring ExeDir() {
    wchar_t path[MAX_PATH * 4]{};
    const DWORD n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(_countof(path)));
    if (!n || n >= _countof(path)) return L".";
    std::wstring out(path, path + n);
    const auto pos = out.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : out.substr(0, pos);
}

template <typename T>
bool ResolveProc(HMODULE module, const char* name, T& out) {
    out = nullptr;
    FARPROC p = module ? GetProcAddress(module, name) : nullptr;
    if (!p) return false;
    static_assert(sizeof(p) == sizeof(out), "pointer-size mismatch");
    std::memcpy(&out, &p, sizeof(out));
    return out != nullptr;
}

bool HasModule(DWORD pid, const wchar_t* name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, name) == 0) { found = true; break; }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

BOOL CALLBACK EnumGameWindows(HWND hwnd, LPARAM param) {
    if (!IsWindowVisible(hwnd) || GetWindowTextLengthW(hwnd) <= 0) return TRUE;
    DWORD pid = 0;
    const DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || !tid || !HasModule(pid, kGameModule)) return TRUE;
    auto* out = reinterpret_cast<std::vector<GameClient>*>(param);
    for (const auto& existing : *out) if (existing.pid == pid) return TRUE;
    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, _countof(title));
    out->push_back({pid, tid, hwnd, title});
    return TRUE;
}

std::vector<GameClient> FindClients() {
    std::vector<GameClient> out;
    EnumWindows(EnumGameWindows, reinterpret_cast<LPARAM>(&out));
    std::sort(out.begin(), out.end(), [](const GameClient& a, const GameClient& b) { return a.pid < b.pid; });
    return out;
}

class BridgeClient {
public:
    ~BridgeClient() { Close(); }
    BridgeClient() = default;
    BridgeClient(const BridgeClient&) = delete;
    BridgeClient& operator=(const BridgeClient&) = delete;

    bool Attach(const GameClient& game, std::wstring& error) {
        Close();
        game_ = game;
        wchar_t mappingName[96]{};
        MappingName(game.pid, mappingName, _countof(mappingName));
        mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                      static_cast<DWORD>(sizeof(SharedBlock)), mappingName);
        if (!mapping_) { error = L"Không tạo được shared memory"; return false; }
        shared_ = reinterpret_cast<SharedBlock*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBlock)));
        if (!shared_) { error = L"Không map được shared memory"; Close(); return false; }
        ZeroMemory(shared_, sizeof(*shared_));
        shared_->magic = kMagic;
        shared_->protocolVersion = kProtocolVersion;
        shared_->targetPid = game.pid;
        shared_->targetWindowThreadId = game.threadId;

        const std::wstring dllPath = ExeDir() + L"\\ProbeBridge.dll";
        if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            error = L"Thiếu ProbeBridge.dll cạnh ProbeController.exe";
            Close();
            return false;
        }
        localDll_ = LoadLibraryW(dllPath.c_str());
        if (!localDll_) {
            error = L"LoadLibrary ProbeBridge.dll thất bại Win32=" + std::to_wstring(GetLastError());
            Close();
            return false;
        }
        HOOKPROC hookProc = nullptr;
        if (!ResolveProc(localDll_, "TlcProbeGetMessageHook", hookProc)) {
            error = L"ProbeBridge.dll thiếu TlcProbeGetMessageHook";
            Close();
            return false;
        }
        hook_ = SetWindowsHookExW(WH_GETMESSAGE, hookProc, localDll_, game.threadId);
        if (!hook_) {
            error = L"SetWindowsHookExW thất bại; tool phải cùng quyền với game";
            Close();
            return false;
        }
        if (!PostThreadMessageW(game.threadId, kWakeMessage, 0, 0)) {
            error = L"Không đánh thức được game message thread";
            Close();
            return false;
        }
        attached_ = true;
        return true;
    }

    void Close() {
        if (hook_) UnhookWindowsHookEx(hook_);
        if (localDll_) FreeLibrary(localDll_);
        if (shared_) UnmapViewOfFile(shared_);
        if (mapping_) CloseHandle(mapping_);
        hook_ = nullptr;
        localDll_ = nullptr;
        shared_ = nullptr;
        mapping_ = nullptr;
        attached_ = false;
        pendingSeq_ = 0;
        pendingWakeTick_ = 0;
        game_ = {};
    }

    bool Attached() const { return attached_; }
    DWORD Pid() const { return game_.pid; }
    const GameClient& Game() const { return game_; }

    bool Call(Command command, int arg0, int arg1, int arg2,
              ProbeResponse& response, std::wstring& error, DWORD timeoutMs = 1800) {
        if (!attached_ || !shared_) { error = L"Bridge chưa attach"; return false; }

        if (pendingSeq_ > 0) {
            if (shared_->completedSeq == pendingSeq_) {
                MemoryBarrier();
                pendingSeq_ = 0;
                pendingWakeTick_ = 0;
            } else {
                const DWORD now = GetTickCount();
                if (shared_->bridgeBusy == 0 &&
                    (pendingWakeTick_ == 0 || now - pendingWakeTick_ >= kBridgeNudgeMs)) {
                    (void)PostThreadMessageW(game_.threadId, kWakeMessage, 0, 0);
                    pendingWakeTick_ = now;
                }
                error = L"Bridge còn request cũ sau timeout; không gửi chồng request";
                return false;
            }
        }
        if (shared_->bridgeBusy != 0) { error = L"Bridge busy; không gửi chồng request"; return false; }

        const LONG seq = shared_->requestSeq + 1;
        shared_->request = {};
        shared_->request.command = static_cast<std::uint32_t>(command);
        shared_->request.arg0 = arg0;
        shared_->request.arg1 = arg1;
        shared_->request.arg2 = arg2;
        MemoryBarrier();
        InterlockedExchange(&shared_->requestSeq, seq);
        if (!PostThreadMessageW(game_.threadId, kWakeMessage, 0, 0)) {
            error = L"Không đánh thức được game thread";
            return false;
        }

        const DWORD begin = GetTickCount();
        while (GetTickCount() - begin < timeoutMs) {
            if (shared_->completedSeq == seq) {
                MemoryBarrier();
                response = shared_->response;
                if (!response.ok) {
                    error = response.detail[0] ? response.detail : L"Bridge trả lỗi";
                    return false;
                }
                return true;
            }
            Sleep(2);
        }
        pendingSeq_ = seq;
        pendingWakeTick_ = GetTickCount();
        error = L"Bridge timeout; fail-closed";
        return false;
    }

private:
    GameClient game_{};
    HANDLE mapping_ = nullptr;
    SharedBlock* shared_ = nullptr;
    HMODULE localDll_ = nullptr;
    HHOOK hook_ = nullptr;
    bool attached_ = false;
    LONG pendingSeq_ = 0;
    DWORD pendingWakeTick_ = 0;
};

std::wstring KindName(std::int32_t kind) {
    switch (static_cast<UiKind>(kind)) {
        case UiKind::Button: return L"Button";
        case UiKind::Toggle: return L"Toggle";
        case UiKind::Rect: return L"Rect";
        case UiKind::Other: return L"Other";
        default: return L"Unknown";
    }
}

std::wstring RectText(const UiRow& row) {
    if (!row.hasGeometry) return L"-";
    wchar_t text[160]{};
    swprintf_s(text, L"%.1f,%.1f %.1fx%.1f", row.rectX, row.rectY, row.rectWidth, row.rectHeight);
    return text;
}

std::wstring HexId(std::uint64_t value) {
    wchar_t text[32]{};
    swprintf_s(text, L"%016llX", static_cast<unsigned long long>(value));
    return text;
}

std::vector<std::uint64_t> Identities(const UiSnapshot& snapshot) {
    std::vector<std::uint64_t> out;
    const int count = std::max(0, std::min<int>(snapshot.rowCount, static_cast<int>(kMaxUiRows)));
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) if (snapshot.rows[i].identity) out.push_back(snapshot.rows[i].identity);
    return out;
}

struct PendingEvidence {
    bool active = false;
    std::wstring action;
    std::uint64_t targetId = 0;
    std::vector<std::uint64_t> beforeIds;
};

struct AppState {
    HWND window = nullptr;
    HWND clients = nullptr;
    HWND status = nullptr;
    HWND list = nullptr;
    HWND detail = nullptr;
    HWND log = nullptr;
    std::vector<GameClient> games;
    BridgeClient bridge;
    bool pointValid = false;
    int pointX = 0;
    int pointY = 0;
    UiRow lastPicked{};
    UiSnapshot lastSnapshot{};
    PendingEvidence evidence{};
};

AppState g_app;

void SetStatus(const std::wstring& text) {
    if (g_app.status) SetWindowTextW(g_app.status, text.c_str());
}

std::wstring TimeStamp() {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t text[32]{};
    swprintf_s(text, L"[%02u:%02u:%02u]", t.wHour, t.wMinute, t.wSecond);
    return text;
}

void Log(const std::wstring& text) {
    if (!g_app.log) return;
    const std::wstring line = TimeStamp() + L" " + text + L"\r\n";
    const int len = GetWindowTextLengthW(g_app.log);
    SendMessageW(g_app.log, EM_SETSEL, len, len);
    SendMessageW(g_app.log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(g_app.log, EM_SCROLLCARET, 0, 0);
