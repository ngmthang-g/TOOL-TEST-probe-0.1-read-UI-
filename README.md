# ThanLong UI Internal Probe v0.1.3

Mini probe tách từ nền 9.9, chỉ phục vụ đọc UI runtime và thử **gọi callback trực tiếp** của UI trong Thần Long.

## Trạng thái

- Version: `0.1.3-probe`
- Build: **BUILD PASS** trên Windows/MSVC x64 cho v0.1.3.
- Windows build run: `34621857069` — Configure x64, Build Release, native CTest, artifact upload và publish `dist/` đều PASS.
- Runtime đã biết: **SCAN PASS** (`SCAN ACTIVE UI`) trên client thật; InputSync của donor 9.9 là baseline đã biết hoạt động và không phải mục tiêu cần chứng minh lại.
- Runtime cần retest: F8 EventSystem target mapping + `TEST DIRECT`.

## Mục tiêu v0.1.3

v0.1.1 đã xác nhận F8 lấy đúng tọa độ nhưng `RectangleContainsScreenPoint(..., camera=null)` trả `hits=0`, nên Direct chưa bao giờ tới bước callback. v0.1.3 bỏ resolver geometry đó khỏi đường F8/Direct.

Luồng mới:

`F8 -> EventSystem.current.RaycastAll(PointerEventData) -> GameObject hit -> walk Transform parent -> map UIObject.instances -> callable UIObject -> TEST DIRECT`

`TEST DIRECT` luôn **raycast và re-resolve lại target hiện tại** trước khi gọi, không giữ `UIButton*` cũ.

Callback direct hỗ trợ:

- `UIButton.HandleClickEvent()`
- `UIToggle.set_Selected(true)` / `HandleSelectEvent(true)`
- `UIRectTransform.PointerClickHandler` qua `MonoBehaviourExecutor.ExecuteScriptFunction`

## Những thứ đã bỏ khỏi UI probe

- `TEST BAG SEMANTIC` đã bị loại bỏ vì live test cho thấy đường semantic generic có thể làm game timeout/diss.
- Không còn nút `TEST INPUTSYNC`; InputSync chỉ được giữ nội bộ làm donor/reference, không phải mục tiêu probe.
- Không có auto train, sell, trade, route, Telegram hay automation legacy.

## Cách test v0.1.3

1. Để `ProbeController.exe` và `ProbeBridge.dll` cùng thư mục.
2. Chọn client -> **Attach** -> **SCAN ACTIVE UI**.
3. Rê chuột lên dấu X/icon/nút cần tìm -> nhấn **F8**. F8 chỉ raycast/chọn, không click.
4. Log tốt sẽ có `F8 PICK PASS • EventSystem target ...` cùng `raycastHits=`, `mapped=`, `callableMapped=`.
5. Bấm **TEST DIRECT**. Tool re-raycast tại điểm F8 và gọi callback direct của target hiện hành.

Nếu F8 có `raycastHits>0` nhưng `mapped=0`, gửi log đó để sửa lớp map `GameObject -> UIObject`; không quay lại test InputSync.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

GitHub Actions Windows x64 tạo artifact `ThanLong-UI-Internal-Probe-v0.1.3-win-x64` và publish EXE/DLL/ZIP vào `dist/`.

Build pass không đồng nghĩa Direct runtime pass; trạng thái runtime chỉ được nâng sau live evidence trên client thật.


## v0.1.3 live-pointer resolver

v0.1.2 live test trả `Action ném managed exception` trước khi map target. v0.1.3 không tự tạo `PointerEventData`/`List<RaycastResult>` nữa; nó tái sử dụng `EventSystem.current.currentInputModule`, `PointerInputModule.GetLastPointerEventData(-1)` và `m_RaycastResultCache`, tạm set position F8 rồi restore. Log lỗi giờ chỉ rõ stage nếu managed call còn lỗi.
