# SlimeVR ESP Tracker WiFi Dongle

[![Open SlimeVR WiFi Dongle Manager](https://img.shields.io/badge/Open-SlimeVR%20WiFi%20Dongle%20Manager-blue?style=for-the-badge)](https://nekodakohaku.github.io/SlimeVR-Tracker-ESP-WiFi-Dongle/)

[日本語](#日本語) | [English](#english) | [中文](#中文)

---

## 日本語

本プロジェクトは、ESP32-S3 を SlimeVR ESP トラッカー専用の WiFi レシーバーにするファームウェアです。

ESP32-S3 Dongle を PC に接続すると、Dongle が独立した SoftAP ホットスポットを作成し、トラッカーから受信した UDP データを USB HID 経由で PC 側の SlimeVR Server へ転送します。

これにより、家庭用 WiFi との距離や混雑、2.4GHz 帯の干渉などによる遅延や切断の問題を軽減できます。

また、一般的なルーターでは変更できない特殊な設定を使用しているため、通常のルーターと比べてトラッカーの消費電力を約 10% 削減できます。

🖼️ 実測データ（投稿）: <https://x.com/NekodaKohaku/status/2067842378573254682>

### ⚠️ ハードウェア互換性

現在、このファームウェアは以下のデバイスで動作確認済みです：

- ✅ ESP32-S3-WROOM-1

ESP32-S2 でも理論上は動作する可能性がありますが、最大接続数と安定性は未確認です。

### ⚙️ デフォルト WiFi 設定

| 設定項目 | デフォルト値 | 備考 |
| :--- | :--- | :--- |
| **SSID** | `SlimeDongle-XXXXXX` | MAC 後半 6 桁を含む装置固有名 |
| **パスワード** | 初回起動時に生成 | 装置ごとに保存される 12 文字のランダム値 |
| **UDP ポート** | `6969` | SlimeVR のデフォルトポート |
| **最大接続数** | `10` | ESP32 SoftAP の最大制限 |

### 🛠️ 設定変更

WiFi 名、パスワード、チャンネルなどを変更する場合は、以下のファイルを編集してください。

```text
src/WifiDongleConfig.h
```

主な設定項目:

- SoftAP の SSID / パスワード
- デフォルト SSID の MAC サフィックス / ランダムパスワード
- WiFi チャンネル
- トラッカーのタイムアウト
- ハートビート間隔
- 最大接続数
- 複数 Dongle 用の一意な USB シリアル

### 💻 USB コマンドコンソール

シリアルポートを開いた後に `help` を入力すると、すべてのコマンドを表示できます。

```text
help                         Display this help text
info                         Get device information
status                       Show dongle status and internal chip temperature
uptime                       Show device uptime
reboot                       Restart the dongle
bootloader                   Enter ESP32 ROM download mode
meow                         Meow!
trackers list                List stored trackers
trackers clear               Clear tracker mappings and restart
wifi show                    Show current WiFi settings, including password
wifi set ssid <name>         Save a new SoftAP name
wifi set password <password> Save a new SoftAP password
wifi set channel <auto|1-13> Save a new WiFi channel
wifi reset                   Restore this device's default WiFi settings
```

### 🌐 Web Serial Control

Dongle の設定確認・変更には、[Web Serial Control](https://nekodakohaku.github.io/SlimeVR-Tracker-ESP-WiFi-Dongle/) を使用してください。デスクトップ版 Chrome または Edge が必要です。

### 🚀 クイックスタート

1. ファームウェアを ESP32-S3 Dongle に書き込みます。
2. Dongle を PC の USB ポートに接続します。
3. Web Serial Control で Dongle に接続するか、シリアルターミナルから `wifi show` を実行して、装置固有の SSID とパスワードを確認します。
4. 表示された SSID とパスワードを SlimeVR トラッカーへ設定し、Web Serial Control またはシリアルターミナルを切断します。
5. PC 上で SlimeVR Server を起動します。
6. トラッカーが接続されると、SlimeVR Server に表示されます。

> 💡 **備考:**
> USB 延長ケーブルを使用し、Dongle を高く遮蔽物のない場所に設置することをお勧めします。パソコン背面の USB ポートは、遮蔽物によって信号が弱くなる場合があります。

### 🍟 ぽてとら向けカスタムファームウェア

ぽてとらシリーズを使用している場合は、本ファームウェアから派生した、ぽてとら向けカスタムファームウェアを利用できます。

<https://github.com/MintoCandy/SlimeVR-Tracker-ESP-WiFi-Dongle-Potetora-Custom>

### 📚 参考

- <https://github.com/SlimeVR/SlimeVR-Tracker-ESP-Receiver>
- <https://github.com/SlimeVR/SlimeVR-Tracker-nRF-Receiver>
- <https://github.com/Kirisame-Nanoha/mocopi-slimevr-hid-receiver/tree/main>

---

## English

This project turns an ESP32-S3 into a dedicated WiFi receiver for SlimeVR ESP Trackers.

By plugging the ESP32-S3 Dongle into your PC, the dongle creates its own independent SoftAP hotspot, receives UDP data from Trackers, and forwards the data to the PC-side SlimeVR Server through USB HID.

This can help reduce latency and disconnection issues caused by home WiFi distance, congestion, or 2.4GHz interference.

Because it uses special settings that ordinary routers cannot adjust, it can reduce the tracker's power consumption by about 10%.

🖼️ Measurement (post): <https://x.com/NekodaKohaku/status/2067842378573254682>

### ⚠️ Hardware Compatibility

This firmware has currently been tested on:

- ✅ ESP32-S3-WROOM-1

ESP32-S2 may theoretically work, but the maximum connection count and stability are not yet confirmed.

### ⚙️ Default WiFi Settings

| Setting | Default Value | Note |
| :--- | :--- | :--- |
| **SSID** | `SlimeDongle-XXXXXX` | Device-specific name using the last six MAC digits |
| **Password** | Generated on first boot | Device-specific 12-character random value |
| **UDP Port** | `6969` | SlimeVR default port |
| **Maximum Connections** | `10` | Maximum limit of ESP32 SoftAP |

### 🛠️ Configuration

To change the WiFi name, password, channel, or other options, edit:

```text
src/WifiDongleConfig.h
```

Main options:

- SoftAP SSID / password
- Default SSID MAC suffix / random password
- WiFi channel
- Tracker timeout
- Heartbeat interval
- Maximum connection count
- Unique USB serial for multiple dongles

### 💻 USB Command Console

Enter `help` after opening the serial port to display the full command list.

```text
help                         Display this help text
info                         Get device information
status                       Show dongle status and internal chip temperature
uptime                       Show device uptime
reboot                       Restart the dongle
bootloader                   Enter ESP32 ROM download mode
meow                         Meow!
trackers list                List stored trackers
trackers clear               Clear tracker mappings and restart
wifi show                    Show current WiFi settings, including password
wifi set ssid <name>         Save a new SoftAP name
wifi set password <password> Save a new SoftAP password
wifi set channel <auto|1-13> Save a new WiFi channel
wifi reset                   Restore this device's default WiFi settings
```

### 🌐 Web Serial Control

To view or change the Dongle settings, use the [Web Serial Control](https://nekodakohaku.github.io/SlimeVR-Tracker-ESP-WiFi-Dongle/). Desktop Chrome or Edge is required.

### 🚀 Quick Start

1. Flash this firmware to your ESP32-S3 Dongle.
2. Plug the dongle into a USB port on your PC.
3. Connect with Web Serial Control, or run `wifi show` in a serial terminal, to read this dongle's SSID and password.
4. Enter the displayed SSID and password on your SlimeVR Trackers, then disconnect Web Serial Control or the serial terminal.
5. Start SlimeVR Server on your PC.
6. Once connected, the Trackers should appear in SlimeVR Server.

> 💡 **Note:**
> Use a USB extension cable to position the dongle higher and away from obstructions. A USB port on the back of the PC may weaken the signal if the dongle is obstructed.

### 🍟 Custom Firmware for Potetora

If you use a Potetora-series device, you can use the Potetora custom firmware derived from this firmware.

<https://github.com/MintoCandy/SlimeVR-Tracker-ESP-WiFi-Dongle-Potetora-Custom>

### 📚 References

- <https://github.com/SlimeVR/SlimeVR-Tracker-ESP-Receiver>
- <https://github.com/SlimeVR/SlimeVR-Tracker-nRF-Receiver>
- <https://github.com/Kirisame-Nanoha/mocopi-slimevr-hid-receiver/tree/main>

---

## 中文

本專案是將 ESP32-S3 變成 SlimeVR ESP 追蹤器專用的 WiFi 接收器的韌體。

將 ESP32-S3 Dongle 插在電腦上後，Dongle 會建立獨立的 SoftAP 熱點，接收追蹤器傳來的 UDP 資料，並透過 USB HID 將資料轉送到 PC 端的 SlimeVR Server。

這樣可以減少因家用 WiFi 距離、壅塞或 2.4GHz 干擾造成的延遲與斷線問題。

由於使用了一般路由器無法調整的特殊設定，相比之下可為追蹤器節省約 10% 的電力。

🖼️ 實測數據（貼文）: <https://x.com/NekodaKohaku/status/2067842378573254682>

### ⚠️ 硬體相容性

目前此韌體已完整測試於：

- ✅ ESP32-S3-WROOM-1

ESP32-S2 理論上可能可行，但最大連線數與穩定性尚未確認。

### ⚙️ 預設 WiFi 設定

| 設定項目 | 預設值 | 備註 |
| :--- | :--- | :--- |
| **SSID** | `SlimeDongle-XXXXXX` | 使用 MAC 後六碼的裝置專屬名稱 |
| **密碼** | 第一次開機時產生 | 每台裝置各自保存的 12 字元隨機值 |
| **UDP 埠** | `6969` | SlimeVR 預設埠 |
| **最大連線數** | `10` | ESP32 SoftAP 的最大限制 |

### 🛠️ 設定修改

若要修改 WiFi 名稱、密碼、頻道或其他設定，請編輯：

```text
src/WifiDongleConfig.h
```

主要可調整項目：

- SoftAP 的 SSID / 密碼
- 預設 SSID 的 MAC 後綴／隨機密碼
- WiFi 頻道
- 追蹤器逾時
- 心跳間隔
- 最大連線數
- 多台 Dongle 用的唯一 USB 序號

### 💻 USB 指令控制台

開啟序列埠後輸入 `help`，即可顯示完整指令表。

```text
help                         Display this help text
info                         Get device information
status                       Show dongle status and internal chip temperature
uptime                       Show device uptime
reboot                       Restart the dongle
bootloader                   Enter ESP32 ROM download mode
meow                         Meow!
trackers list                List stored trackers
trackers clear               Clear tracker mappings and restart
wifi show                    Show current WiFi settings, including password
wifi set ssid <name>         Save a new SoftAP name
wifi set password <password> Save a new SoftAP password
wifi set channel <auto|1-13> Save a new WiFi channel
wifi reset                   Restore this device's default WiFi settings
```

### 🌐 Web Serial 控制台

若要查看或更改 Dongle 設定，請使用 [Web Serial 控制台](https://nekodakohaku.github.io/SlimeVR-Tracker-ESP-WiFi-Dongle/)。需要使用桌面版 Chrome 或 Edge。

### 🚀 快速上手

1. 將韌體燒錄到 ESP32-S3 Dongle。
2. 將 Dongle 插到 PC 的 USB 埠。
3. 使用 Web Serial 控制台連接 Dongle，或在序列終端執行 `wifi show`，讀取這台裝置專屬的 SSID 與密碼。
4. 將顯示的 SSID 與密碼設定到 SlimeVR 追蹤器，然後中斷 Web Serial 控制台或序列終端的連線。
5. 啟動 PC 上的 SlimeVR Server。
6. 追蹤器連線成功後，會顯示在 SlimeVR Server 中。

> 💡 **備註：**
> 建議使用 USB 延長線將 Dongle 放到較高或無遮擋的位置。使用電腦後面的 USB 插孔可能會因遮擋而使訊號減弱。

### 🍟 ぽてとら 系列客製韌體

如果使用的是 ぽてとら 系列裝置，可以使用由本韌體分支而來的 ぽてとら 專用客製韌體。

<https://github.com/MintoCandy/SlimeVR-Tracker-ESP-WiFi-Dongle-Potetora-Custom>

### 📚 參考

- <https://github.com/SlimeVR/SlimeVR-Tracker-ESP-Receiver>
- <https://github.com/SlimeVR/SlimeVR-Tracker-nRF-Receiver>
- <https://github.com/Kirisame-Nanoha/mocopi-slimevr-hid-receiver/tree/main>
