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

### Flash 容量とビルド環境

ESP32-S3 は Flash / PSRAM の構成が異なります。実機の容量に合う環境を選んでください。

| PlatformIO 環境 | Flash | PSRAM |
| :--- | :--- | :--- |
| `slime_dongle_s3` | 16MB | OPI PSRAM |
| `slime_dongle_s3_4mb` | 4MB | 不要・初期化しません |
| `slime_dongle_xiao_s3` | 8MB | OPI PSRAM |

4MB ボードのビルド: `pio run -e slime_dongle_s3_4mb`。書き込みは同じコマンドに `-t upload` を追加します。4MB 環境は `default.csv` を使用し、アプリ領域は 1,310,720 bytes です。容量の異なる環境のバイナリを使用しないでください。新しい 4MB 環境は実機での起動確認が必要です。LED は GPIO17、BOOT ボタンは GPIO0 を前提とします。市販ボードで異なる場合は `variants/slime_dongle_s3/pins_arduino.h` を調整してください。

### ⚙️ デフォルト WiFi 設定

| 設定項目 | デフォルト値 | 備考 |
| :--- | :--- | :--- |
| **SSID** | `SlimeDongle-XXXXXX` | MAC 後半 6 桁を含む装置固有名 |
| **パスワード** | 初回起動時に生成 | 装置ごとに保存される 12 文字のランダム値 |
| **UDP ポート** | `6969` | SlimeVR のデフォルトポート |
| **最大接続数** | `10` | ESP32 SoftAP の最大制限 |

### 初期パスワードの確認

PC に USB 接続し、上記の **Open SlimeVR WiFi Dongle Manager** をデスクトップ版 Chrome / Edge で開き、Dongle に接続して WiFi 設定欄を確認してください。パスワードの「表示」を押すと確認できます。シリアルターミナル（115200 baud）で `wifi show` を実行しても確認できます。その SSID とパスワードを Tracker に設定してください。

初回起動時に生成した 12 文字のパスワードは装置に保存され、再起動や `wifi reset` では変わりません。保存済みのカスタム WiFi 設定はアップデート後も優先されます。以前の固定デフォルトを使い、カスタム設定を保存していない装置では、アップデート後に SSID / パスワードが変わるため、Tracker 側の再設定が必要です。NVS を全消去すると新しいパスワードが生成されるため、再確認・再設定が必要です。NVS への保存に失敗した場合は `src/WifiDongleConfig.h` の `apPassword` が使われるため、必ず `wifi show` で実際の値を確認してください。

`appendMacSuffixToDefaultSsid = true` で装置固有 SSID、`generateRandomDefaultPassword = true` でランダム初期パスワードを有効にしています。固定値にしたい場合は対応する設定を `false` にし、`apSsid` / `apPassword` を変更して再ビルドしてください。保存済み設定がある場合は `wifi reset` 後に再起動するとデフォルトが適用されます。

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

詳細な更新内容は [CHANGELOG](CHANGELOG.md#日本語) を参照してください。

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

### Flash Capacity and Build Environment

ESP32-S3 boards have different Flash / PSRAM configurations. Select the environment matching your hardware.

| PlatformIO environment | Flash | PSRAM |
| :--- | :--- | :--- |
| `slime_dongle_s3` | 16MB | OPI PSRAM |
| `slime_dongle_s3_4mb` | 4MB | Not required or initialized |
| `slime_dongle_xiao_s3` | 8MB | OPI PSRAM |

Build for 4MB: `pio run -e slime_dongle_s3_4mb`; add `-t upload` to flash it. This environment uses `default.csv` with a 1,310,720-byte application slot. Do not use a binary built for a different flash capacity. The new 4MB environment still needs hardware boot verification. LED GPIO17 and BOOT GPIO0 are assumed; adjust `variants/slime_dongle_s3/pins_arduino.h` if your board differs.

### ⚙️ Default WiFi Settings

| Setting | Default Value | Note |
| :--- | :--- | :--- |
| **SSID** | `SlimeDongle-XXXXXX` | Device-specific name using the last six MAC digits |
| **Password** | Generated on first boot | Device-specific 12-character random value |
| **UDP Port** | `6969` | SlimeVR default port |
| **Maximum Connections** | `10` | Maximum limit of ESP32 SoftAP |

### Finding the Initial Password

Connect the Dongle to your PC over USB, open **Open SlimeVR WiFi Dongle Manager** above in desktop Chrome / Edge, connect to the Dongle, and check the WiFi settings. Click **Show** to reveal the password. Alternatively, open a serial terminal at 115200 baud and run `wifi show`. Configure your Trackers with the displayed SSID and password.

The 12-character password generated on first boot is saved on the device. Rebooting or running `wifi reset` does not regenerate it. Existing saved custom WiFi settings continue to take priority after an update. Devices using the previous fixed defaults without saved custom settings will change SSID / password after updating; reconfigure your Trackers with the new values. Erasing all NVS generates a new password, so read it again and update your Trackers. If saving the generated password to NVS fails, `apPassword` in `src/WifiDongleConfig.h` is used as a fallback; always check the actual value with `wifi show`.

`appendMacSuffixToDefaultSsid = true` enables the device-specific SSID; `generateRandomDefaultPassword = true` enables the random initial password. To use fixed defaults, set the corresponding options to `false`, edit `apSsid` / `apPassword`, and rebuild. If custom settings are already saved, run `wifi reset` and reboot to apply defaults.

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

See [CHANGELOG](CHANGELOG.md#english) for detailed updates.

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

### Flash 容量與建置環境

ESP32-S3 有不同 Flash／PSRAM 配置，請依實際硬體選擇環境。

| PlatformIO 環境 | Flash | PSRAM |
| :--- | :--- | :--- |
| `slime_dongle_s3` | 16MB | OPI PSRAM |
| `slime_dongle_s3_4mb` | 4MB | 不需要，也不會初始化 |
| `slime_dongle_xiao_s3` | 8MB | OPI PSRAM |

4MB 建置指令：`pio run -e slime_dongle_s3_4mb`；加上 `-t upload` 即可燒錄。此環境使用 `default.csv`，單一應用程式分割區為 1,310,720 bytes。請勿使用其他容量環境產生的韌體。新增的 4MB 環境仍需實機開機驗證。LED 預設 GPIO17、BOOT 按鈕 GPIO0；若板子不同，請調整 `variants/slime_dongle_s3/pins_arduino.h`。

### ⚙️ 預設 WiFi 設定

| 設定項目 | 預設值 | 備註 |
| :--- | :--- | :--- |
| **SSID** | `SlimeDongle-XXXXXX` | 使用 MAC 後六碼的裝置專屬名稱 |
| **密碼** | 第一次開機時產生 | 每台裝置各自保存的 12 字元隨機值 |
| **UDP 埠** | `6969` | SlimeVR 預設埠 |
| **最大連線數** | `10` | ESP32 SoftAP 的最大限制 |

### 如何找到初始密碼

用 USB 將 Dongle 接到 PC，透過桌面版 Chrome／Edge 開啟上方 **Open SlimeVR WiFi Dongle Manager**，連接 Dongle 後查看 WiFi 設定欄，按「顯示」即可查看密碼。也可以開啟序列終端（115200 baud），輸入 `wifi show`。將顯示的 SSID 與密碼設定到 Tracker 即可。

首次開機產生的 12 字元密碼會保存在裝置中，重開機或執行 `wifi reset` 都不會重新產生。升級後仍優先使用既有已儲存的自訂 WiFi 設定。若之前使用固定預設值、未儲存自訂設定，升級後 SSID／密碼會改變，需以新值重新設定 Tracker。若全數清除 NVS，會產生新密碼，需重新查詢並更新 Tracker 設定。若隨機密碼無法存入 NVS，會使用 `src/WifiDongleConfig.h` 的 `apPassword` 備用值，請以 `wifi show` 顯示的實際值為準。

目前 `appendMacSuffixToDefaultSsid = true` 啟用裝置專屬 SSID，`generateRandomDefaultPassword = true` 啟用隨機初始密碼。若要改用固定預設值，可將對應選項改為 `false`，修改 `apSsid`／`apPassword` 後重新建置。若已有自訂設定，需執行 `wifi reset` 並重開機，才會套用預設值。

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

詳細更新內容請見 [CHANGELOG](CHANGELOG.md#中文)。

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
