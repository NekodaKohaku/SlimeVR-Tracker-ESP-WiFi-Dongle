# SlimeVR Tracker ESP WiFi Dongle Receiver Firmware

## 日本語

本プロジェクトは ESP32-S3 Dongle 向けに開発された firmware です。ESP32-S3 Dongle を SlimeVR ESP tracker 専用の WiFi receiver として動作させます。Dongle は独立した SoftAP hotspot を作成し、tracker から UDP data を受信して、USB HID 経由で PC 側の SlimeVR Server へ直接転送します。

この方式により、家庭用 WiFi による高 latency や切断問題を軽減し、より安定した motion capture 体験を提供します。

## ⚠️ ハードウェア互換性

現在、この firmware は ESP32-S3 Dongle でのみ完全にテストされています。

ESP32-S2 でも理論上は動作する可能性がありますが、最大接続数と安定性は未確認です。

## ⚙️ デフォルト WiFi 設定

|設定項目|デフォルト値|備考|
|---|---|---|
|SSID|`SlimeDongle`|tracker が接続する Wi-Fi 名|
|Password|`slimedongle12345`|接続 password|
|UDP Port|`6969`|SlimeVR default port|
|最大接続数|`10`|ESP32 SoftAP の性能制限による|

## 🛠️ 高度な設定

WiFi 名、password、または performance tuning を変更する場合は、以下の file を編集してください。

```text
src/WifiDongleConfig.h
```

調整可能な主な項目:

- SoftAP SSID / password と WiFi channel
- DTIM Period（省電力と latency に影響）
- Heartbeat Interval
- Tracker Timeout
- 最大接続数

## 🚀 クイックスタート

1. **Firmware を flash**：この firmware を ESP32-S3 Dongle に flash します。
2. **PC に接続**：Dongle を PC の USB port に接続します。
3. **Server を起動**：PC 上で SlimeVR Server を起動します。
4. **Tracker を設定**：SlimeVR tracker の WiFi 接続情報を以下に変更します。

```text
SSID: SlimeDongle
Password: slimedongle12345
```

5. **装着して使用開始**：tracker の接続が成功すると、SlimeVR Server の画面に直接表示されます。追加の pairing 手順は不要です。

---

## English

This project is firmware developed for ESP32-S3 Dongle. It turns the ESP32-S3 Dongle into a dedicated WiFi receiver for SlimeVR ESP trackers. The dongle creates an independent SoftAP hotspot, receives UDP data from trackers, and forwards it directly to the PC-side SlimeVR Server through USB HID.

This approach can reduce high latency and disconnection issues caused by home WiFi, providing a more stable motion capture experience.

## ⚠️ Hardware Compatibility

This firmware has currently been fully tested only on ESP32-S3 Dongle.

ESP32-S2 may theoretically work, but the maximum connection count and stability are not yet confirmed.

## ⚙️ Default WiFi Settings

|Setting|Default Value|Note|
|---|---|---|
|SSID|`SlimeDongle`|Wi-Fi name trackers should connect to|
|Password|`slimedongle12345`|Connection password|
|UDP Port|`6969`|SlimeVR default communication port|
|Maximum Connections|`10`|Limited by ESP32 SoftAP hardware performance|

## 🛠️ Advanced Customization

To change the WiFi name, password, or performance tuning options, edit:

```text
src/WifiDongleConfig.h
```

Core adjustable parameters:

- SoftAP SSID / password and WiFi channel
- DTIM Period, which affects power saving and latency
- Heartbeat Interval
- Tracker Timeout
- Maximum connection limit

## 🚀 Quick Start

1. **Flash firmware**: Flash this firmware to your ESP32-S3 Dongle.
2. **Connect to PC**: Plug the dongle into a USB port on your PC.
3. **Start service**: Open SlimeVR Server on your PC.
4. **Configure trackers**: Change your SlimeVR tracker WiFi settings to:

```text
SSID: SlimeDongle
Password: slimedongle12345
```

5. **Wear and start**: Once connected, the trackers will appear directly in SlimeVR Server. No additional pairing procedure is required.

---

## 中文

本專案專為 ESP32-S3 Dongle 開發，將其轉化為 SlimeVR ESP 追蹤器的專用 WiFi 接收器。Dongle 會自行建立獨立的 SoftAP 熱點接收追蹤器的 UDP 數據，並透過 USB HID 直接轉送至 PC 端的 SlimeVR Server。

此方案可有效降低家用 WiFi 的高延遲與斷線問題，提供更穩定的動態捕捉體驗。

## ⚠️ 硬體相容性備註

目前此韌體僅在 ESP32-S3 Dongle 上進行過完整測試。

ESP32-S2 理論上可行，但最大可連線數量與穩定性尚未明確。

## ⚙️ 預設 WiFi 設定

|設定項目|預設值|備註|
|---|---|---|
|SSID|`SlimeDongle`|追蹤器要連線的 Wi-Fi 名稱|
|Password|`slimedongle12345`|連線密碼|
|UDP Port|`6969`|SlimeVR 預設通訊埠|
|最大連線數|`10`|受限於 ESP32 SoftAP 的硬體效能限制|

## 🛠️ 進階自訂配置

若需修改 WiFi 名稱、密碼或進行效能調優，請編輯以下檔案：

```text
src/WifiDongleConfig.h
```

可調整的核心參數：

- SoftAP SSID / 密碼與 WiFi 頻道（Channel）
- DTIM Period（影響省電與延遲）
- 心跳包間隔（Heartbeat Interval）
- 追蹤器逾時斷線判定（Tracker Timeout）
- 最大連線數量上限

## 🚀 快速上手指南

1. **燒錄韌體**：將本韌體燒錄至您的 ESP32-S3 Dongle 中。
2. **連接電腦**：將 Dongle 插上 PC 的 USB 埠。
3. **啟動服務**：開啟電腦上的 SlimeVR Server。
4. **設定追蹤器**：將您的 SlimeVR 追蹤器 WiFi 連線資訊修改為：

```text
SSID: SlimeDongle
Password: slimedongle12345
```

5. **完成配戴**：追蹤器連接成功後，會直接顯示在 SlimeVR Server 介面中（不需任何額外的配對手續），即可開始使用。
