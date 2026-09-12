# ThanLong UI Internal Probe v0.1.4

Mini probe tách từ nền 9.9, chỉ phục vụ đọc UI runtime và thử gọi callback trực tiếp của UI trong Thần Long.

## Trạng thái

- Version: `0.1.4-probe`.
- Build candidate: Windows/MSVC x64; trạng thái chính thức nằm trong `VERSION.txt` sau CI.
- Runtime nền đã biết: **SCAN PASS** (`SCAN ACTIVE UI`) trên client thật; F8 vẫn là selector không click.
- Ba target mới `MỞ TAY NẢI`, `CHUYỂN → SKILL`, `CHUYỂN → TAY NẢI`: **RUNTIME UNTESTED** cho tới khi có log/live evidence từ client thật.

## Ba target v0.1.4

DATA 222 đã được decrypt/extract để lấy identity thật từ `Interface.unity3d`, không dùng ảnh và không dùng tọa độ cứng:

- `MỞ TAY NẢI`: `ButBag` + `ButBagClick`.
- Nút đổi giao diện: `ButtonOriginalSwitchSite` + `ButtonOriginalSwitchSiteClicked`.
- Hai hình kiếm/ô vuông là hai trạng thái của cùng một nút switch, không phải hai button riêng.
- Hướng switch được guard bằng `ToggleFirstTab` / `ToggleSecondTab` và `UIToggle.get_Selected`.

Mỗi target có hai thao tác:

- `NHẬN DIỆN`: read-only, re-enumerate live `UIObject.instances`, trả target/state nhưng không dispatch.
- `TEST DIRECT TARGET`: re-resolve live object + state rồi mới gọi callback. Nếu đã ở đúng giao diện thì trả `TARGET ALREADY IN STATE` và **không click**, tránh toggle ngược.

Nếu exact identity không tồn tại hoặc xuất hiện nhiều target ngang nhau, probe fail-closed (`TARGET NOT FOUND` / `TARGET AMBIGUOUS`) và không mutate.

## F8 generic vẫn giữ nguyên

`F8 -> EventSystem.current.RaycastAll(live PointerEventData) -> GameObject hit -> Transform parent -> UIObject.instances -> TEST DIRECT`

F8 chỉ chọn/đọc. `TEST DIRECT` luôn raycast và re-resolve target hiện tại trước khi gọi callback; không giữ `UIButton*` cũ qua UI transition.

Callback direct hỗ trợ:

- `UIButton.HandleClickEvent()`
- `UIToggle.set_Selected(true)` / `HandleSelectEvent(true)`
- `UIRectTransform.PointerClickHandler` qua `MonoBehaviourExecutor.ExecuteScriptFunction`

## Những đường đã loại khỏi probe UI

- Không có `TEST BAG SEMANTIC`.
- Không có nút `TEST INPUTSYNC`; InputSync donor chỉ còn source reference nội bộ.
- Không có auto train, sell, trade, route, Telegram hay automation legacy.

## Cách test

1. Để `ProbeController.exe` và `ProbeBridge.dll` cùng thư mục.
2. Chọn client -> **Attach** -> **SCAN ACTIVE UI**.
3. Với ba target mới, bấm `NHẬN DIỆN` trước để xem exact Name/Handler/current state.
4. Bấm `TEST DIRECT TARGET` tương ứng.
5. Với UI khác, rê chuột lên control -> F8 -> xem detail -> **TEST DIRECT**.

Build/CI PASS không đồng nghĩa runtime PASS. Chỉ nâng trạng thái runtime sau khi client thật xác nhận đúng UI transition.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

GitHub Actions tạo artifact `ThanLong-UI-Internal-Probe-v0.1.4-win-x64`, đồng thời publish vào `dist/`:

- `ProbeController.exe`
- `ProbeBridge.dll`
- `ThanLong-UI-Internal-Probe-v0.1.4-win-x64.zip`
- `ThanLong-UI-Internal-Probe-v0.1.4-source.zip`
