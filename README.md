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

### USB スケジューリングと診断（v1.1.5）

USB は 1 ms に最大 1 バッチを送信し、新センサーは登録後に姿勢を送ります。送信受付失敗時は登録・状態を保持して再試行し、姿勢は最新値に更新します。センサーごとの最新値を保持するため、古い姿勢が蓄積しません。状態キューには上限があり、超過は計数します。

Web Serial の「状態を更新」、または `diagnostics` で Tracker ごとの接続状態、RSSI（dBm、0 は未報告）、UDP PPS、最終有効受信からの時間、センサー ID、受信数、姿勢統合数、USB 受付失敗試行数、UDP 形式拒否、制御キュー破棄数を確認できます。計数は起動後の累積です。PPS は初期観測期間が必要で、受信停止後は 0 になります。姿勢統合は無線損失ではなく、USB 受付も Server の受信を保証しません。形式拒否は対応パケットの不正形式で、RF 損失率は推定しません。Tracker を特定できない拒否は `status` の全体 `Rejected UDP packets` のみに含まれます。


各列の ? はホバー・キーボードフォーカス・タップで説明を表示します。Escape、再タップ、外側クリックで閉じます。RX は累積受信数、USB 失敗は受付失敗試行数で、無線損失数ではありません。

公式 SlimeVR ESP Tracker ファームを対象とします。USB スケジュール・診断・保存済み ID・Dongle WiFi 設定は Tracker 改造不要です。RSSI・電池・温度・拡張センサーは Tracker のビルドとハードが送信する情報に依存します。確認した公式 UDP 実装には PingPong と一部センサー設定フラグがありますが、本 Dongle はまだ操作 UI を提供していません。ESB/ESP-NOW OTA・TDMA・doze/wake・nRF 派生版の校正／電源イベントは直接移植できません。公式固有の OTA は別途設計・検証が必要です。無受信だけでは休止・電源断・無線断を区別できません。

### 個別登録削除・センサー情報・診断出力（v1.1.6）

Tracker 一覧の「登録を削除」で、オフラインの 1 台だけを削除できます。先に対象 Tracker を切り、既定で 6 秒以上待ってください。他の ID と Tracker の WiFi 設定は変わりません。削除された MAC と ID は Dongle 再起動まで再登録／再利用を抑止します。同じ WiFi 設定の Tracker を再び起動すると、Dongle 再起動後は再登録される場合があります。保持した ID と新規接続できる空き枠は再起動後に整理されます。

センサー別情報は Tracker を選択して取得します。各センサーの最終姿勢時刻、IMU、温度、報告状態を独立表示し、未報告は — とします。「直近姿勢」は 1.5 秒以内の受信で、故障や休眠の判定ではありません。電池と RSSI は物理 Tracker で共有します。

「診断をエクスポート」は JSON をローカル保存し、WiFi SSID・パスワード・MAC・USB シリアル・端末ログは含みません。値は順次取得するため、同一時刻のスナップショットではありません。コンソールは `trackers remove <id>` と `sensors <id>` を追加しました。旧ファームで完全なデータが得られない場合、正常なレポートとして保存しません。

### WiFi AP とチャンネル選択の参考（v1.1.7）

Web Serial のスキャンは周辺 2.4GHz WiFi AP の BSSID 数・最強信号・チャンネルを表示し、1/6/11 から参考候補を示します。AP 信号と周波数の重なりによる独自の推定値で、干渉率・実トラフィック・速度の測定ではありません。主に 20MHz 幅を想定し、Bluetooth・無線音声・USB 3.0 雑音などは測定できません。候補が良くても実際の安定性は保証されません。

Tracker をすべて切り、WiFi クライアントを切断してから USB 経由でスキャンしてください。最後の受信から既定で 6 秒以上待ちます。通常の VR 機器を動かし、Dongle は普段の位置に置いてください。接続端末があると開始を拒否し、測定中の接続で中止します。無線チャンネルを一時変更するため、使用中の測定には向きません。

保存は手動で、現在のチャンネルは変わりません。Dongle 再起動後に適用され、Tracker は再接続します。結果は 5 分で失効し、AP がなければ推奨しません。コンソール: `channels start` / `channels status` / `channels cancel`。`channels apply <generation> <channel>` は新しい結果の候補だけ保存します。AP 名や MAC は結果に含めません。

適用後は Tracker を起動して同じ VR 条件で診断を比較してください。USB 延長で主機・USB 3.0 機器から離す比較も有効です。PPS や最終受信時刻は観測値で、無線損失率や干渉源を直接示しません。公式 Tracker ファームの変更は不要です。

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

### USB scheduling and diagnostics (v1.1.5)

USB reports are limited to one batch per millisecond. New sensor registration is sent before its poses. Failed submissions retain registration/status records for retry; retried poses are refreshed to the latest values. Each sensor has a latest-pose slot, so backlog does not grow with its sampling rate. Status queues remain bounded; overflow is counted.

Use **Update status** in Web Serial to refresh the per-tracker diagnostics table, or run `diagnostics` in the console. It shows online state, RSSI (dBm; 0 means not yet reported), received UDP datagrams per second, time since the last accepted datagram, sensor IDs, received datagrams, coalesced poses, failed USB submission attempts, rejected UDP packets, and control-queue drops. Counters are since boot; PPS needs an initial sampling window and becomes zero when reception stops. Coalescing is intentional replacement of unsent poses, not radio loss. USB acceptance does not prove the Server received a report. Rejected packets are malformed supported datagrams; no RF-loss estimate is shown. Packets that cannot be attributed to a known tracker only appear in the global `Rejected UDP packets` status counter.


Diagnostic column headers include ? help buttons, available by hover, keyboard focus or tap. Escape, a second tap or an outside click dismisses the explanation. RX means cumulative received datagrams; USB failures count submission attempts, not lost wireless packets.

This receiver targets the official SlimeVR ESP Tracker firmware. Local USB scheduling, diagnostics, saved mappings and WiFi settings do not require a custom Tracker build. RSSI, battery, temperature and auxiliary-sensor data depend on what that Tracker build and hardware actually send. The checked official UDP implementation supports PingPong and specific sensor config flags, but this Dongle does not yet expose those controls. ESB/ESP-NOW OTA, TDMA, doze/wake and the nRF fork's calibration/power event protocol cannot simply be transplanted. The official Tracker has its own OTA implementation; integrating it needs a separate design and test. Missing packets do not identify whether a Tracker is asleep, powered off or disconnected.

### Individual removal, sensor details and report export (v1.1.6)

Use **Remove binding** beside a stored offline Tracker. Turn it off first and wait at least the default 6-second timeout. Other IDs and the Tracker WiFi settings stay unchanged. Removed MACs cannot re-register and removed IDs/peer slots are retired until Dongle restart. After restart, a Tracker with matching WiFi credentials may join again. Restart also releases retired IDs/slots for new devices.

Select a Tracker to read independent sensor observations: last pose age, IMU type, temperature, reported state and pose count. Missing values display —. Recent pose means a pose received within 1.5 seconds, not a hardware health or sleep verdict. Battery and RSSI remain shared by the physical Tracker.

**Export diagnostics** saves a local JSON report. WiFi SSID/password, MAC addresses, USB serial and terminal logs are excluded. Samples are sequential, not an atomic snapshot. Console commands: `trackers remove <id>` and `sensors <id>`. Incomplete data from an older firmware is not exported as a successful report.

### Nearby WiFi APs and channel reference (v1.1.7)

The Web Serial scan shows nearby 2.4 GHz BSSID counts, strongest signals and channels, with a reference suggestion among 1/6/11. Scores are a heuristic based on AP signal strength and frequency overlap, primarily assuming 20 MHz channels. They do not measure interference percentage, traffic or throughput, and cannot measure Bluetooth, wireless audio or USB 3.0 noise. A suggestion does not guarantee stability.

Turn all Trackers off, disconnect WiFi clients and wait at least the default 6-second receiver timeout. Scan over USB with your normal VR equipment operating and the Dongle in its usual position. Connected clients prevent scanning; a joining client aborts it. The radio temporarily changes channels, so do not scan during tracking.

Saving is manual and only takes effect after Dongle restart, when Trackers reconnect. Results expire after 5 minutes; no APs means no suggestion. Console: `channels start`, `channels status`, `channels cancel`; `channels apply <generation> <channel>` saves only the fresh suggested candidate. Results contain no AP names or MAC addresses.

After applying, turn Trackers on and compare diagnostics under the same VR conditions. Also compare moving the Dongle away from the PC/USB 3.0 devices with a USB extension. PPS and last-reception age are observations, not direct radio-loss or interference-source measurements. No changes to official Tracker firmware are required.

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

### USB 排程與診斷（v1.1.5）

每毫秒最多提交一批 USB 封包，新感測器會先註冊再送姿態。提交失敗時保留註冊／狀態等待重試，姿態則更新為最新值。每個感測器使用一個最新姿態欄位，避免因取樣頻率增加而累積舊動作。狀態佇列仍有容量上限，超出時會計數。

在 Web Serial 按「更新狀態」可更新每台 Tracker 診斷表，或在終端輸入 `diagnostics`。欄位包括在線狀態、RSSI（dBm；0 表示尚未回報）、UDP 收包頻率、距最後有效收包時間、感測器 ID、累積收包、姿態合併、USB 提交失敗嘗試、UDP 格式拒絕及控制佇列丟棄。計數自開機累積；PPS 需先累積取樣時間，停止收包後歸零。姿態合併是以新值替換未送舊值，不等於無線丟包；USB 接受也不代表 Server 已收到。格式拒絕指已支援封包的格式不合法，此處不估算 RF 丟包率。無法歸屬已知 Tracker 的拒絕封包只列入 `status` 的全域 `Rejected UDP packets`。


各欄 ? 支援滑鼠停留、鍵盤聚焦或點按，按 Escape、再次點按或點擊外側可關閉。RX 是累積收包，USB 失敗是提交失敗嘗試，皆不代表無線丟包。

本接收器以官方 SlimeVR ESP Tracker 韌體為目標。USB 排程、診斷、保存 ID 與 Dongle WiFi 設定不需要改 Tracker 韌體。RSSI、電池、溫度與副感測器資料則取決於該 Tracker 的編譯設定與硬體實際送出什麼。核對的官方 UDP 實作有 PingPong 與特定感測器設定旗標，但本 Dongle 尚未提供這些操作。ESB/ESP-NOW OTA、TDMA、doze/wake、nRF 客製版校正／電源事件協定不能直接套用。官方 Tracker 自己有 OTA 實作，若要整合需另行設計及驗證。單憑沒有收到封包，無法判斷是休眠、關機或無線斷線。

### 個別刪除、感測器資訊與報告匯出（v1.1.6）

在已儲存 Tracker 旁按「移除綁定」，只移除一台離線裝置。請先關閉目標 Tracker，並等待預設至少 6 秒。其他 ID 與 Tracker 本身的 WiFi 設定不變。被刪除的 MAC 本次開機不接受重新註冊，ID／接收槽位也保留到 Dongle 重開機才釋出；重開機後，相同 WiFi 設定的 Tracker 仍可能重新加入。

選擇 Tracker 可讀取各感測器獨立的最後姿態時間、IMU 型號、溫度、回報狀態與姿態數。未回報顯示 —。「近期姿態」僅表示 1.5 秒內收到姿態，不代表硬體健康或休眠判定。電池與 RSSI 仍共用實體 Tracker 的資訊。

「匯出診斷報告」會下載本機 JSON，不含 WiFi 名稱、密碼、MAC、USB 序號與終端紀錄。資料依序讀取，並非同一瞬間的完整快照。終端新增 `trackers remove <id>` 與 `sensors <id>`。若舊韌體未提供完整資料，不會當作正常報告匯出。

### WiFi AP 分布與選頻參考（v1.1.7）

Web Serial 掃描會列出附近 2.4GHz AP 的 BSSID 數量、最強訊號與頻道，從 1／6／11 提供參考候選。分數依訊號強弱與頻道重疊估算，主要假設 20MHz 頻寬；不是干擾率、實際流量或速度測量，無法測得藍牙、無線音訊、USB 3.0 雜訊。建議頻道不保證最穩。

請關閉所有 Tracker、中斷其他 WiFi 用戶端，等待最後收包後至少預設的 6 秒，再透過 USB 掃描。保持平常 VR 設備運作，Dongle 放在實際位置。有連線裝置時拒絕開始，掃描中有裝置加入則中止。掃描會暫時切換無線電頻道，請勿在追蹤使用中操作。

手動儲存後，目前頻道不變；重啟 Dongle 才套用，Tracker 會重新連接。結果有效 5 分鐘，沒有 AP 時不提出建議。指令：`channels start`、`channels status`、`channels cancel`；`channels apply <generation> <channel>` 只接受新結果的建議候選。結果不包含 AP 名稱或 MAC。

套用後開啟 Tracker，在相同 VR 情境比較診斷；也可用 USB 延長線移離主機與 USB 3.0 設備再比較。PPS、最後收包時間都是觀測值，不等於無線丟包率，也不能直接辨識干擾來源。不需要修改官方 Tracker 韌體。

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
