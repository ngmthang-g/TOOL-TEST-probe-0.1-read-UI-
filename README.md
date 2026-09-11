# ThanLong UI Internal Probe v0.1

Mini probe tách từ nền UI/bridge của tool 9.9 để kiểm tra **UI runtime không cần tên hiển thị** trong Thần Long. Bản này cố ý bỏ toàn bộ automation cũ; chỉ giữ chức năng tìm UI và thử ba đường tác động nội bộ.

## Trạng thái

- Version: `0.1.0-probe`
- Build target: Windows x64
- Runtime: **RUNTIME UNTESTED** cho tới khi test trên client game thật.
- Không cache `UIButton*` giữa các lần UI thay đổi; mỗi action re-scan/re-resolve object hiện tại.

## Cách dùng

1. Đặt `ProbeController.exe` và `ProbeBridge.dll` cùng thư mục.
2. Mở game Thần Long, sau đó chạy `ProbeController.exe` cùng mức quyền với game.
3. Chọn client và bấm **Attach**.
4. Bấm **SCAN ACTIVE UI** để đọc các UIObject đang active: class, Name, Text, Tag, handler, parent chain, descendant labels, depth và local Rect.
5. Rê chuột lên dấu X, icon túi hoặc control cần thử rồi nhấn **F8**. F8 **chỉ chọn UI, không click**.
6. Sau khi kiểm tra đúng object ở khung Selected UI, dùng một trong ba phép thử có chủ đích:
   - **TEST DIRECT**: re-resolve object tại điểm rồi ưu tiên `UIButton.HandleClickEvent`, `UIToggle` hoặc Lua `PointerClickHandler`.
   - **TEST INPUTSYNC**: re-resolve điểm rồi đi `InputSyncManager.TryClickUI -> EndUIDrag`, có guard/cancel drag.
   - **TEST BAG SEMANTIC**: thử semantic `MainCallUI/CallUI` cho `RoleInfo_BagTab`, không phụ thuộc tọa độ icon túi.
7. Probe tự fresh-scan sau action và log số UI identity added/removed/unchanged. Với TEST BAG SEMANTIC còn gọi verify `RoleInfo_BagTab` riêng.

> Delay của timer chỉ để lấy snapshot sau; **không được coi thời gian trôi qua là bằng chứng thành công**. Kết luận runtime phải dựa trên thay đổi UI/semantic state và quan sát game thật.

## Fail-closed

- Nếu hai UI hit có diện tích và depth ngang nhau, probe trả `AMBIGUOUS` thay vì đoán.
- Nếu điểm F8 không nằm trong client area game, không gửi action.
- Nếu bridge còn request cũ sau timeout, controller không gửi chồng request.
- InputSync từ chối chạy nếu `_uiDragging` đã active và gọi `CancelUIDragState` khi release lỗi.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Output: `build/bin/ProbeController.exe` và `build/bin/ProbeBridge.dll`.

GitHub Actions artifact: **ThanLong-UI-Internal-Probe-v0.1-win-x64**. Workflow cũng cố publish bản build vào `dist/` trên nhánh chính để tải EXE/DLL/ZIP trực tiếp.
