"use strict";

const USB_FILTERS = [{ usbVendorId: 0x1209, usbProductId: 0x7690 }];
const BAUD_RATE = 115200;
const RESPONSE_IDLE_MS = 180;
const RESPONSE_TIMEOUT_MS = 2200;
const MAX_TRACKERS = 10;
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const LANGUAGE_STORAGE_KEY = "slimevr-dongle-language";

const translations = {
  "zh-Hant": {
    pageDescription: "SlimeVR WiFi Dongle 的免安裝 Web Serial 控制台。", backToTop: "回到頂端", privacy: "本機 USB 連線，不上傳資料",
    heroTitle: '<span class="hero-title-accent">SlimeVR ESP</span><br><span class="hero-title-accent">WiFi Dongle</span><br><span class="hero-title-neutral">管理工具</span>', heroLead: "從瀏覽器管理 Dongle，查看裝置狀態、設定 SoftAP，以及管理 Tracker。",
    dongleStatus: "Dongle 狀態", disconnected: "尚未連接", connected: "已連接", connectDongle: "連接 Dongle", disconnect: "中斷連線", connectHelp: "瀏覽器會請你選擇 SlimeVR WiFi Dongle 的序列埠。", dashboard: "Dongle 控制面板",
    deviceInfo: "裝置資訊", refreshStatus: "更新狀態", statusUpdated: "運作狀態已更新", infoUpdated: "裝置資訊已更新", meowComplete: "喵！", helpShown: "指令列表已顯示於終端", product: "產品", firmware: "韌體", usbSerial: "USB 序號", chip: "晶片",
    operatingStatus: "運作狀態", uptime: "運作時間", softApChannel: "SoftAP 頻道", connectedDevices: "已連線裝置", chipTemperature: "晶片溫度", packetStats: "封包統計", ready: "就緒",
    wifiSettings: "WiFi 設定", restartPending: "等待重新啟動", password: "密碼", passwordPlaceholder: "至少 8 個字元", showPassword: "顯示", hidePassword: "隱藏", passwordHelp: "8–63 bytes，儲存後需重新啟動", channel: "頻道", autoChannel: "自動選擇 1 / 6 / 11", saveWifi: "儲存 WiFi 設定", readAgain: "重新讀取",
    infoDescription: "裝置與版本資料", statusDescription: "目前運作狀態", meowDescription: "喵。", helpDescription: "顯示所有指令", storedTrackers: "已儲存的 Tracker", trackerConnectPrompt: "連接後即可讀取", noStoredTrackers: "沒有已儲存的 Tracker。", trackerDisplay: "Tracker {number}（ID {id}）  {mac}", clearTracker: "清除 Tracker", connectedCapacity: "目前連線", storedCapacity: "已儲存",
    advancedTitle: "終端與系統操作", clearScreen: "清除畫面", terminalWaiting: "等待連接 Dongle…", commandPlaceholder: "輸入指令，例如 status", serialCommand: "序列指令", send: "送出", systemActions: "系統操作", rebootDongle: "重新啟動 Dongle", enterBootloader: "進入 Bootloader", restoreWifi: "恢復預設 WiFi 設定",
    cantConnect: "連不上？", cantConnectHelp: "請先關閉 nRF Connect、PuTTY 或其他占用序列埠的程式。", dataSafety: "資料安全", dataSafetyHelp: "此頁面不需要登入，也不會將密碼傳送到網路。", browser: "瀏覽器", browserHelp: "請使用桌面版 Chrome 或 Edge，並透過 HTTPS 開啟。", pleaseConfirm: "請確認", confirmAction: "確認操作", cancel: "取消", confirm: "確認",
    terminalConnected: "[已連接] SlimeVR WiFi Dongle\n", connectionInterrupted: "序列連線中斷：{error}", incompatibleDevice: "選取的裝置不是相容的 SlimeVR WiFi Dongle", connectedToast: "Dongle 已連接", portBusy: "無法開啟序列埠，請關閉其他序列工具後再試。", connectionFailed: "連線失敗：{error}", disconnectedToast: "已中斷 Dongle 連線", dongleNotConnected: "Dongle 尚未連接", previousPending: "上一個指令仍在處理", serialClosed: "序列埠已關閉", packetValue: "控制丟棄 {dropped} / USB 失敗嘗試 {failed}", readFailed: "讀取失敗：{error}", invalidLineBreak: "內容不能包含換行或 NUL", invalidQuotes: "內容不能同時以引號開頭或結尾並包含兩種引號", ssidInvalid: "SSID 必須是 1–32 bytes。", passwordInvalid: "密碼必須是 8–63 bytes。", noReply: "Dongle 沒有回覆", wifiSaved: "WiFi 設定已儲存，重新啟動後生效", saveFailed: "儲存失敗：{error}", commandComplete: "指令完成", commandSent: "指令已送出，Dongle 正在重新連線", rebooting: "Dongle 正在重新啟動", bootTitle: "進入 Bootloader？", bootMessage: "Dongle 將中斷目前的 HID 與序列連線，並以 ESP32 ROM Download Mode 重新出現。", trackerClearTitle: "清除所有 Tracker？", trackerClearMessage: "所有已儲存的 Tracker 對應會被刪除，Dongle 接著會重新啟動。", clearAndReboot: "清除並重啟", wifiResetTitle: "恢復預設 WiFi？", wifiResetMessage: "已儲存的 SSID、密碼與頻道會被清除；需要重新啟動才會套用。", restoreDefault: "恢復預設", wifiRestored: "已恢復預設 WiFi，重新啟動後生效", operationFailed: "操作失敗：{error}", unsupportedBrowser: "此瀏覽器不支援 Web Serial。請改用桌面版 Chrome 或 Edge。", secureContextRequired: "Web Serial 需要 HTTPS 安全連線。請從 GitHub Pages 網址開啟此頁。"
  },
  ja: {
    pageDescription: "SlimeVR WiFi Dongle 用のインストール不要な Web Serial コントロール画面です。", backToTop: "ページ上部へ戻る", privacy: "ローカル USB 接続・データ送信なし",
    heroTitle: '<span class="hero-title-accent">SlimeVR ESP</span><br><span class="hero-title-accent">WiFi Dongle</span><br><span class="hero-title-neutral">管理ツール</span>', heroLead: "ブラウザから Dongle の状態確認、SoftAP 設定、Tracker 管理を行えます。",
    dongleStatus: "Dongle の状態", disconnected: "未接続", connected: "接続済み", connectDongle: "Dongle に接続", disconnect: "切断", connectHelp: "ブラウザに表示される SlimeVR WiFi Dongle のシリアルポートを選択してください。", dashboard: "Dongle コントロールパネル",
    deviceInfo: "デバイス情報", refreshStatus: "状態を更新", statusUpdated: "動作状態を更新しました", infoUpdated: "デバイス情報を更新しました", meowComplete: "にゃー！", helpShown: "コマンド一覧をターミナルに表示しました", product: "製品", firmware: "ファームウェア", usbSerial: "USB シリアル", chip: "チップ",
    operatingStatus: "動作状態", uptime: "稼働時間", softApChannel: "SoftAP チャンネル", connectedDevices: "接続中のデバイス", chipTemperature: "チップ温度", packetStats: "パケット統計", ready: "準備完了",
    wifiSettings: "WiFi 設定", restartPending: "再起動待ち", password: "パスワード", passwordPlaceholder: "8 文字以上", showPassword: "表示", hidePassword: "非表示", passwordHelp: "8～63 bytes・保存後に再起動が必要", channel: "チャンネル", autoChannel: "1 / 6 / 11 から自動選択", saveWifi: "WiFi 設定を保存", readAgain: "再読み込み",
    infoDescription: "デバイスとバージョン情報", statusDescription: "現在の動作状態", meowDescription: "にゃー。", helpDescription: "すべてのコマンドを表示", storedTrackers: "保存済み Tracker", trackerConnectPrompt: "接続後に読み込めます", noStoredTrackers: "保存済み Tracker はありません。", trackerDisplay: "Tracker {number}（ID {id}）  {mac}", clearTracker: "Tracker を消去", connectedCapacity: "現在の接続数", storedCapacity: "保存済み",
    advancedTitle: "ターミナルとシステム操作", clearScreen: "画面を消去", terminalWaiting: "Dongle の接続を待っています…", commandPlaceholder: "コマンドを入力（例：status）", serialCommand: "シリアルコマンド", send: "送信", systemActions: "システム操作", rebootDongle: "Dongle を再起動", enterBootloader: "Bootloader に入る", restoreWifi: "デフォルト WiFi 設定に戻す",
    cantConnect: "接続できない場合", cantConnectHelp: "nRF Connect、PuTTY など、シリアルポートを使用しているアプリを閉じてください。", dataSafety: "データ保護", dataSafetyHelp: "ログインは不要で、パスワードがネットワークへ送信されることもありません。", browser: "ブラウザ", browserHelp: "デスクトップ版 Chrome または Edge から HTTPS で開いてください。", pleaseConfirm: "確認してください", confirmAction: "操作の確認", cancel: "キャンセル", confirm: "確認",
    terminalConnected: "[接続済み] SlimeVR WiFi Dongle\n", connectionInterrupted: "シリアル接続が切断されました：{error}", incompatibleDevice: "選択したデバイスは対応する SlimeVR WiFi Dongle ではありません", connectedToast: "Dongle に接続しました", portBusy: "シリアルポートを開けません。他のシリアルツールを閉じてから再試行してください。", connectionFailed: "接続に失敗しました：{error}", disconnectedToast: "Dongle との接続を切断しました", dongleNotConnected: "Dongle が接続されていません", previousPending: "前のコマンドを処理中です", serialClosed: "シリアルポートが閉じられました", packetValue: "制御破棄 {dropped} / USB 失敗試行 {failed}", readFailed: "読み込みに失敗しました：{error}", invalidLineBreak: "改行または NUL は使用できません", invalidQuotes: "両方の引用符を含み、引用符で開始または終了する値は使用できません", ssidInvalid: "SSID は 1～32 bytes にしてください。", passwordInvalid: "パスワードは 8～63 bytes にしてください。", noReply: "Dongle から応答がありません", wifiSaved: "WiFi 設定を保存しました。再起動後に反映されます", saveFailed: "保存に失敗しました：{error}", commandComplete: "コマンドが完了しました", commandSent: "コマンドを送信しました。Dongle の再接続を待っています", rebooting: "Dongle を再起動しています", bootTitle: "Bootloader に入りますか？", bootMessage: "現在の HID とシリアル接続を切断し、ESP32 ROM Download Mode として再接続します。", trackerClearTitle: "すべての Tracker を消去しますか？", trackerClearMessage: "保存済みの Tracker マッピングをすべて削除し、Dongle を再起動します。", clearAndReboot: "消去して再起動", wifiResetTitle: "WiFi をデフォルトに戻しますか？", wifiResetMessage: "保存済みの SSID、パスワード、チャンネルを消去します。反映には再起動が必要です。", restoreDefault: "デフォルトに戻す", wifiRestored: "デフォルト WiFi 設定に戻しました。再起動後に反映されます", operationFailed: "操作に失敗しました：{error}", unsupportedBrowser: "このブラウザは Web Serial に対応していません。デスクトップ版 Chrome または Edge を使用してください。", secureContextRequired: "Web Serial には HTTPS 接続が必要です。GitHub Pages の URL から開いてください。"
  },
  en: {
    pageDescription: "An install-free Web Serial control panel for the SlimeVR WiFi Dongle.", backToTop: "Back to top", privacy: "Local USB connection, no data uploads",
    heroTitle: '<span class="hero-title-accent">SlimeVR ESP</span><br><span class="hero-title-accent">WiFi Dongle</span><br><span class="hero-title-neutral">Manager</span>', heroLead: "Manage your Dongle in the browser: view device status, configure the SoftAP, and manage Trackers.",
    dongleStatus: "Dongle status", disconnected: "Not connected", connected: "Connected", connectDongle: "Connect Dongle", disconnect: "Disconnect", connectHelp: "Your browser will ask you to select the SlimeVR WiFi Dongle serial port.", dashboard: "Dongle control panel",
    deviceInfo: "Device information", refreshStatus: "Update status", statusUpdated: "Operating status updated", infoUpdated: "Device information updated", meowComplete: "Meow!", helpShown: "Command list shown in the terminal", product: "Product", firmware: "Firmware", usbSerial: "USB serial", chip: "Chip",
    operatingStatus: "Operating status", uptime: "Uptime", softApChannel: "SoftAP channel", connectedDevices: "Connected devices", chipTemperature: "Chip temperature", packetStats: "Packet statistics", ready: "Ready",
    wifiSettings: "WiFi settings", restartPending: "Restart pending", password: "Password", passwordPlaceholder: "At least 8 characters", showPassword: "Show", hidePassword: "Hide", passwordHelp: "8–63 bytes; restart after saving", channel: "Channel", autoChannel: "Automatically select 1 / 6 / 11", saveWifi: "Save WiFi settings", readAgain: "Read again",
    infoDescription: "Device and version details", statusDescription: "Current operating status", meowDescription: "Meow.", helpDescription: "Show all commands", storedTrackers: "Stored Trackers", trackerConnectPrompt: "Connect to read", noStoredTrackers: "No stored Trackers.", trackerDisplay: "Tracker {number} (ID {id})  {mac}", clearTracker: "Clear Trackers", connectedCapacity: "Connected now", storedCapacity: "Stored",
    advancedTitle: "Terminal and system actions", clearScreen: "Clear screen", terminalWaiting: "Waiting for Dongle connection…", commandPlaceholder: "Enter a command, for example status", serialCommand: "Serial command", send: "Send", systemActions: "System actions", rebootDongle: "Restart Dongle", enterBootloader: "Enter Bootloader", restoreWifi: "Restore default WiFi settings",
    cantConnect: "Can't connect?", cantConnectHelp: "Close nRF Connect, PuTTY, or any other application using the serial port.", dataSafety: "Data safety", dataSafetyHelp: "No login is required, and passwords are never sent over the network.", browser: "Browser", browserHelp: "Use desktop Chrome or Edge and open this page over HTTPS.", pleaseConfirm: "Please confirm", confirmAction: "Confirm action", cancel: "Cancel", confirm: "Confirm",
    terminalConnected: "[Connected] SlimeVR WiFi Dongle\n", connectionInterrupted: "Serial connection interrupted: {error}", incompatibleDevice: "The selected device is not a compatible SlimeVR WiFi Dongle", connectedToast: "Dongle connected", portBusy: "Unable to open the serial port. Close other serial tools and try again.", connectionFailed: "Connection failed: {error}", disconnectedToast: "Dongle disconnected", dongleNotConnected: "Dongle is not connected", previousPending: "The previous command is still being processed", serialClosed: "Serial port closed", packetValue: "Control drops {dropped} / USB failed attempts {failed}", readFailed: "Read failed: {error}", invalidLineBreak: "The value cannot contain a line break or NUL", invalidQuotes: "The value cannot begin or end with a quote while containing both quote types", ssidInvalid: "SSID must be 1–32 bytes.", passwordInvalid: "Password must be 8–63 bytes.", noReply: "The Dongle did not reply", wifiSaved: "WiFi settings saved; restart to apply", saveFailed: "Save failed: {error}", commandComplete: "Command complete", commandSent: "Command sent; waiting for the Dongle to reconnect", rebooting: "Dongle is restarting", bootTitle: "Enter Bootloader?", bootMessage: "The Dongle will disconnect its current HID and serial interfaces and reappear in ESP32 ROM Download Mode.", trackerClearTitle: "Clear all Trackers?", trackerClearMessage: "All stored Tracker mappings will be deleted and the Dongle will restart.", clearAndReboot: "Clear and restart", wifiResetTitle: "Restore default WiFi?", wifiResetMessage: "The saved SSID, password, and channel will be cleared. Restart to apply the defaults.", restoreDefault: "Restore defaults", wifiRestored: "Default WiFi settings restored; restart to apply", operationFailed: "Operation failed: {error}", unsupportedBrowser: "This browser does not support Web Serial. Use desktop Chrome or Edge.", secureContextRequired: "Web Serial requires a secure HTTPS connection. Open this page from its GitHub Pages URL."
  }
};

Object.assign(translations["zh-Hant"], { diagnosticsTitle:"Tracker 診斷", diagnosticsHelp:"按更新狀態讀取；PPS 是 UDP 收包頻率，合併姿態不等於無線丟包。USB 失敗為可重試的提交失敗。", diagOnline:"在線 (1/0)", diagAge:"最後收包 (ms)", diagSensors:"感測器 ID", diagCoalesced:"姿態合併", diagUsb:"USB 失敗嘗試", diagUnavailable:"裝置未提供完整診斷資料，請更新韌體或重新讀取。", diagSummary:"USB 成功：{sent}｜失敗嘗試：{failed}｜控制佇列丟棄：{dropped}｜姿態合併：{coalesced}" });
Object.assign(translations.ja, { diagnosticsTitle:"Tracker 診断", diagnosticsHelp:"状態更新で取得。PPS は UDP 受信頻度です。姿勢の統合は無線パケット損失ではありません。USB 送信失敗は再試行されます。", diagOnline:"接続 (1/0)", diagAge:"最終受信 (ms)", diagSensors:"センサー ID", diagCoalesced:"姿勢統合", diagUsb:"USB 失敗試行", diagUnavailable:"診断データが不完全です。ファーム更新または再読み込みを行ってください。", diagSummary:"USB 成功：{sent}｜失敗試行：{failed}｜制御キュー破棄：{dropped}｜姿勢統合：{coalesced}" });
Object.assign(translations.en, { diagnosticsTitle:"Tracker diagnostics", diagnosticsHelp:"Use Update status to refresh. PPS counts received UDP datagrams. Coalesced poses are not radio packet loss. Failed USB submissions are retried.", diagOnline:"Online (1/0)", diagAge:"Last RX (ms)", diagSensors:"Sensor IDs", diagCoalesced:"Coalesced poses", diagUsb:"USB failed attempts", diagUnavailable:"Incomplete diagnostics. Update the firmware or read again.", diagSummary:"USB accepted: {sent} | Failed attempts: {failed} | Control queue drops: {dropped} | Coalesced poses: {coalesced}" });

Object.assign(translations["zh-Hant"], {diagRejected:"UDP 格式拒絕",diagDrops:"控制佇列丟棄"});
Object.assign(translations.ja, {diagRejected:"UDP 形式拒否",diagDrops:"制御キュー破棄"});
Object.assign(translations.en, {diagRejected:"Rejected UDP",diagDrops:"Control queue drops"});
Object.assign(translations["zh-Hant"], {"diagId": "Tracker ID", "diagRssi": "訊號 (dBm)", "diagPps": "收包頻率 (PPS)", "diagRx": "累積收包 (RX)", "diagUsb": "USB 失敗嘗試", "helpLabel": "說明：{field}", "diagnosticsHelp": "按「更新狀態」重新讀取。各欄的 ? 支援滑鼠停留、鍵盤聚焦或點按；累積計數會在 Dongle 重開機後歸零。", "diagIdHelp": "Dongle 分配給實體 Tracker 的編號。副感測器沿用該 Tracker，並以感測器 ID 區分。", "diagOnlineHelp": "1 表示近期收到 Tracker 封包，0 表示已判定離線。預設約 6 秒未收包才逾時；不代表 SlimeVR Server 的連線狀態，也無法判斷是關機、休眠或無線中斷。", "diagRssiHelp": "Tracker 回報的 WiFi 訊號強度，單位 dBm。負值越接近 0 通常越強，例如 −50 比 −75 強。0 表示尚未回報；離線時可能保留舊值。它不是延遲或丟包率。", "diagPpsHelp": "最近取樣期間每秒收到的有效 UDP 封包數，包含姿態、心跳與其他資訊。不是畫面 FPS 或單一感測器更新率，沒有通用的合格門檻。啟動需先累積樣本，超過 1.5 秒無收包會顯示 0。", "diagAgeHelp": "距離最後一次收到有效封包的毫秒數。持續更新通常會維持較小；持續增加表示暫時未收到新封包。這不是 Tracker 到電腦的傳輸延遲。", "diagSensorsHelp": "已觀察到姿態資料的感測器編號，0 通常是主感測器，1 通常是副感測器。列表會保留到 Dongle 重開機，不代表每個感測器目前都有新資料。", "diagRxHelp": "本次 Dongle 開機後，從此 Tracker 接受的 UDP 封包累積數，包含姿態與控制資訊。不是 Tracker 發送總數，也不能單靠它算無線丟包率。", "diagCoalescedHelp": "新姿態到達時，舊姿態尚未送出，因此以最新值替換的次數。這是避免延遲的正常設計，不代表無線丟包；快速增加也可能表示 USB 供應速度跟不上，請搭配其他項目觀察。", "diagUsbHelp": "包含此 Tracker 資料的 USB 提交失敗嘗試數。同一份資料重試多次會重複計數；註冊及狀態會保留重試，姿態改送最新值。不是已遺失封包數，也不是 Server 回覆的錯誤。持續增加時可檢查 USB 連線與主機接收情況。", "diagRejectedHelp": "來自此 Tracker 的已支援 UDP 封包因長度、感測器 ID 或數值不合法而被拒絕的次數。正常通常為 0；持續增加可檢查韌體版本與協定相容性。不是 RF 丟包計數。無法辨識來源的拒絕只計入全域 status。", "diagDropsHelp": "Dongle 的有界狀態佇列已滿，無法加入新控制／狀態封包的次數。通常應為 0；增加代表內部壅塞，部分狀態通知可能未送出。不是無線丟包，也不包含正常的姿態合併。"});
Object.assign(translations["en"], {"diagId": "Tracker ID", "diagRssi": "Signal (dBm)", "diagPps": "Received UDP/s", "diagRx": "Received total (RX)", "diagUsb": "USB failed attempts", "helpLabel": "About {field}", "diagnosticsHelp": "Use Update status to refresh. Hover, focus or tap ? for help. Cumulative counters reset when the Dongle restarts.", "diagIdHelp": "ID assigned by the Dongle to the physical Tracker. Auxiliary sensors share this Tracker and have separate sensor IDs.", "diagOnlineHelp": "1 means recently received Tracker packets; 0 means offline. The default timeout is about 6 seconds. This is not Server connection status and cannot distinguish sleep, power-off or a radio interruption.", "diagRssiHelp": "WiFi signal strength reported by the Tracker in dBm. A value closer to zero is generally stronger: −50 is stronger than −75. Zero means not yet reported; offline values may be stale. This is not latency or packet loss.", "diagPpsHelp": "Accepted UDP datagrams per second over the recent sampling window, including poses, heartbeats and information. Not display FPS or a single sensor rate; there is no universal pass threshold. Needs initial samples and becomes zero after 1.5 seconds without reception.", "diagAgeHelp": "Milliseconds since the last accepted datagram. A growing value means no recent packets. This is not end-to-end transmission latency.", "diagSensorsHelp": "Sensor IDs observed in pose packets: usually 0 for main and 1 for auxiliary. The list persists until the Dongle restarts and does not prove each sensor is currently updating.", "diagRxHelp": "Total accepted UDP datagrams from this Tracker since Dongle boot, including poses and control information. Not the total sent by the Tracker; it cannot alone measure radio loss.", "diagCoalescedHelp": "Times a new pose replaced an older unsent pose. This intentionally prevents stale motion backlog and is not radio loss. Rapid growth can also indicate insufficient USB throughput; compare other counters.", "diagUsbHelp": "Failed USB submission attempts involving this Tracker. Multiple retries count repeatedly. Registration/status records are retained and poses refresh to the latest value. Not lost packets or a Server-reported error. Persistent growth warrants checking USB and the receiving host.", "diagRejectedHelp": "Supported UDP datagrams rejected for invalid length, sensor ID or values. Normally zero; persistent growth warrants checking firmware/protocol compatibility. Not radio loss. Unattributed rejections appear only in global status.", "diagDropsHelp": "Control/status packets that could not enter the full bounded queue. Normally zero; growth indicates internal congestion and potentially missing state notifications. Not radio loss or normal pose coalescing."});
Object.assign(translations["ja"], {"diagId": "Tracker ID", "diagRssi": "信号 (dBm)", "diagPps": "受信頻度 (PPS)", "diagRx": "累積受信 (RX)", "diagUsb": "USB 受付失敗試行", "helpLabel": "説明：{field}", "diagnosticsHelp": "「状態を更新」で取得。? にマウスを重ねる、フォーカスする、またはタップすると説明を表示します。累積値は Dongle 再起動で 0 に戻ります。", "diagIdHelp": "Dongle が物理 Tracker に割り当てた番号です。拡張センサーは同じ Tracker に属し、センサー ID で区別します。", "diagOnlineHelp": "1 は最近 Tracker のパケットを受信、0 はオフラインです。既定では約 6 秒でタイムアウトします。Server の接続状態ではなく、電源断・休止・無線断を区別できません。", "diagRssiHelp": "Tracker が報告した WiFi 信号強度（dBm）。通常は 0 に近い負値ほど強く、−50 は −75 より強い値です。0 は未報告、切断後は古い値の場合があります。遅延や損失率ではありません。", "diagPpsHelp": "直近の観測期間の UDP 受信数／秒です。姿勢・Heartbeat・情報を含み、画面 FPS や単一センサーの更新率ではありません。共通の合格基準はありません。初期観測が必要で、1.5 秒無受信で 0 になります。", "diagAgeHelp": "最後の有効受信からの経過ミリ秒です。増え続ける場合は新しい受信がありません。端末間の伝送遅延ではありません。", "diagSensorsHelp": "姿勢パケットで観測したセンサー ID。通常 0 がメイン、1 が拡張です。再起動まで保持され、各センサーが現在更新中である証拠ではありません。", "diagRxHelp": "Dongle 起動後にこの Tracker から受理した UDP パケットの累積数。姿勢・制御情報を含みます。Tracker の送信総数ではなく、これだけでは無線損失率を算出できません。", "diagCoalescedHelp": "未送信の古い姿勢を新しい値で置き換えた回数です。遅延を防ぐ通常の処理で、無線損失ではありません。急増時は USB の処理不足も考えられるため、他の値と合わせて確認してください。", "diagUsbHelp": "この Tracker を含む USB 送信受付の失敗試行数。再試行は毎回数えます。登録・状態は保持し、姿勢は最新値に更新します。損失数や Server のエラーではありません。増え続ける場合は USB と受信側を確認してください。", "diagRejectedHelp": "対応 UDP パケットの長さ・ID・数値が不正で拒否した回数。通常は 0 です。増え続ける場合はファームとプロトコルの互換性を確認してください。無線損失ではありません。送信元不明の拒否は全体 status のみに含みます。", "diagDropsHelp": "上限のある状態キューが満杯で追加できなかった制御・状態パケット数。通常は 0 です。増加は内部混雑と通知欠落の可能性を示します。無線損失や通常の姿勢統合ではありません。"});
Object.assign(translations["zh-Hant"], {diagScrollHint:"表格可左右捲動，查看其餘欄位。"});
Object.assign(translations.en, {diagScrollHint:"Scroll the table horizontally to see the remaining columns."});
Object.assign(translations.ja, {diagScrollHint:"表を左右にスクロールすると残りの列を確認できます。"});
Object.assign(translations["zh-Hant"], {"exportDiagnostics": "匯出診斷報告", "exportPrivacy": "匯出版本、狀態與計數，不包含 WiFi 名稱、密碼、MAC 或 USB 序號。", "sensorDetails": "感測器獨立資訊", "chooseTracker": "選擇 Tracker", "sensorHelp": "各感測器分別回報；— 表示未回報。近期姿態表示 1.5 秒內收到姿態，並非硬體故障判定。", "sensorId": "感測器 ID", "recentPose": "近期姿態 (1/0)", "poseAge": "最後姿態 (ms)", "reportedState": "回報狀態", "sensorTemperature": "溫度 (°C)", "poseTotal": "累積姿態", "removeOne": "移除綁定", "removeTitle": "移除此 Tracker 綁定？", "removeMessage": "請先關閉 Tracker 並等待至少 6 秒。此操作只刪除 Dongle 的記錄，其他 ID 不變，不會清除 Tracker 的 WiFi 設定。本次 Dongle 開機會忽略該裝置；重開機後它若仍使用相同 WiFi，可能重新加入。已移除的 ID 本次開機暫不重用。", "removed": "綁定已移除", "exported": "診斷報告已匯出", "sensorUnavailable": "未收到完整的感測器資料，請更新韌體或重新讀取。", "exportFailed": "匯出失敗：{error}", "stateOffline": "離線回報 (0)", "stateOk": "正常回報 (1)", "stateError": "錯誤回報 (2)"});
Object.assign(translations["en"], {"exportDiagnostics": "Export diagnostics", "exportPrivacy": "Exports version, status and counters. Excludes WiFi SSID, password, MAC addresses and USB serial.", "sensorDetails": "Per-sensor observations", "chooseTracker": "Select Tracker", "sensorHelp": "Values are reported per sensor; — means not reported. Recent pose means a pose within 1.5 seconds, not a hardware health verdict.", "sensorId": "Sensor ID", "recentPose": "Recent pose (1/0)", "poseAge": "Last pose (ms)", "reportedState": "Reported state", "sensorTemperature": "Temperature (°C)", "poseTotal": "Pose total", "removeOne": "Remove binding", "removeTitle": "Remove this Tracker binding?", "removeMessage": "Turn off the Tracker and wait at least 6 seconds. Only its Dongle record is removed; other IDs and the Tracker WiFi settings stay unchanged. This Tracker is ignored until Dongle reboot. It may join again after reboot if using the same WiFi. Removed IDs are not reused during this boot.", "removed": "Binding removed", "exported": "Diagnostics exported", "sensorUnavailable": "Incomplete sensor data. Update firmware or read again.", "exportFailed": "Export failed: {error}", "stateOffline": "Reported offline (0)", "stateOk": "Reported OK (1)", "stateError": "Reported error (2)"});
Object.assign(translations["ja"], {"exportDiagnostics": "診断をエクスポート", "exportPrivacy": "バージョン・状態・計数を出力します。WiFi SSID・パスワード・MAC・USB シリアルは含みません。", "sensorDetails": "センサー別情報", "chooseTracker": "Tracker を選択", "sensorHelp": "センサーごとの報告値です。— は未報告です。直近姿勢は 1.5 秒以内の受信を示し、ハード故障の判定ではありません。", "sensorId": "センサー ID", "recentPose": "直近姿勢 (1/0)", "poseAge": "最終姿勢 (ms)", "reportedState": "報告状態", "sensorTemperature": "温度 (°C)", "poseTotal": "累積姿勢", "removeOne": "登録を削除", "removeTitle": "この Tracker の登録を削除しますか？", "removeMessage": "Tracker を切り、6 秒以上待ってください。Dongle の記録のみ削除し、他の ID と Tracker の WiFi 設定は変えません。Dongle 再起動まではこの端末を無視します。同じ WiFi 設定なら再起動後に再登録される場合があります。削除 ID は今回の起動中は再利用しません。", "removed": "登録を削除しました", "exported": "診断を保存しました", "sensorUnavailable": "センサー情報が不完全です。ファーム更新または再取得してください。", "exportFailed": "保存失敗：{error}", "stateOffline": "オフライン報告 (0)", "stateOk": "正常報告 (1)", "stateError": "エラー報告 (2)"});
Object.assign(translations["zh-Hant"], {"channelTitle": "附近 WiFi AP 分布與選頻參考", "channelIntro": "掃描前請關閉所有 Tracker，並保持平常 VR 使用的電腦、頭顯、路由器與無線設備運作。將 Dongle 放在實際使用位置。", "channelLimits": "只依附近 2.4GHz WiFi AP 的訊號與頻道估算；不是干擾率或實際速度。無法完整偵測藍牙、無線音訊、USB 3.0 雜訊與 AP 實際流量。分數越低只表示此模型下 AP 影響較小，仍需開啟 Tracker 實測。", "channelStart": "開始掃描", "channelCancel": "取消掃描", "channelRead": "讀取結果", "channelSave": "儲存建議頻道", "channelConfirmTitle": "開始 WiFi AP 掃描？", "channelConfirm": "請關閉所有 Tracker，並中斷其他裝置與 Dongle WiFi 的連線。掃描會暫時切換無線電頻道，可能影響重連；USB 管理仍可使用。有裝置加入時會中止。", "channelApplyTitle": "儲存建議頻道？", "channelApply": "將儲存頻道 {channel}；目前頻道不會立即改變。請結束使用後重啟 Dongle 套用，Tracker 會暫時斷線並重新連接。掃描結果超過 5 分鐘需重新掃描。", "channelSaved": "建議頻道已儲存，重啟 Dongle 後生效", "channelIdle": "尚未掃描", "channelRunning": "掃描中，請保持 Tracker 關機…", "channelReady": "掃描完成；候選為 1／6／11，分數相近時保留目前候選頻道。", "channelEmpty": "未找到有效 AP；沒有足夠資料提出建議，保留目前設定。", "channelError": "掃描失敗或逾時，請重新讀取或再試一次。", "channelInterrupted": "有裝置加入，掃描已中止；請關閉 Tracker 並中斷 WiFi 連線後重試。", "channelCancelled": "掃描已取消", "channelUnavailable": "韌體未提供完整頻道結果，請更新韌體或重新讀取。", "channelSummary": "目前頻道 {current}｜已儲存 {saved}｜參考建議 {recommended}｜AP 數 {total}｜結果距今 {age} 秒", "channelNumber": "頻道", "channelApCount": "AP 數量", "channelSignal": "最強 AP (dBm)", "channelScore": "AP 影響分數（估算）", "channelRange": "— 表示不在韌體回報的掃描範圍；訊號 — 表示未發現 AP。AP 以不同 BSSID 計數，多個虛擬 AP 不等於多台實體路由器。", "channelTesting": "套用後請開啟 Tracker，在相同 VR 情境比較穩定性。若懷疑 USB 3.0 干擾，可比較使用 USB 延長線將 Dongle 移遠後的結果。", "channelAge": "結果有效 5 分鐘；儲存前會再次驗證。", "channelAuto": "自動", "channelNone": "無", "channelFailure": "頻道分析失敗：{error}"});
Object.assign(translations["en"], {"channelTitle": "Nearby WiFi APs and channel reference", "channelIntro": "Turn off all Trackers before scanning. Keep your usual VR PC, headset, router and wireless devices operating, and place the Dongle where you use it.", "channelLimits": "Estimates nearby 2.4 GHz AP pressure, not interference percentage or throughput. It does not fully measure Bluetooth, wireless audio, USB 3.0 noise or AP traffic. Lower scores mean less AP pressure in this model; verify with real Tracker use.", "channelStart": "Start scan", "channelCancel": "Cancel scan", "channelRead": "Read result", "channelSave": "Save suggested channel", "channelConfirmTitle": "Start WiFi AP scan?", "channelConfirm": "Turn off all Trackers and disconnect other WiFi clients from the Dongle. Scanning temporarily changes radio channels and can affect reconnection. USB remains available. A joining client aborts the scan.", "channelApplyTitle": "Save suggested channel?", "channelApply": "Save channel {channel}. The active channel stays unchanged until you restart the Dongle after use; Trackers will briefly disconnect and reconnect. Results expire after 5 minutes.", "channelSaved": "Channel saved. Restart the Dongle to apply.", "channelIdle": "Not scanned", "channelRunning": "Scanning. Keep Trackers off…", "channelReady": "Complete. Candidates are 1/6/11; near ties retain the current candidate.", "channelEmpty": "No valid APs found. Insufficient information for a suggestion; keep existing settings.", "channelError": "Scan failed or timed out. Read again or retry.", "channelInterrupted": "A client joined; scan aborted. Turn Trackers off and disconnect WiFi clients before retrying.", "channelCancelled": "Scan cancelled", "channelUnavailable": "Incomplete channel data. Update firmware or read again.", "channelSummary": "Active {current} | Saved {saved} | Suggested {recommended} | APs {total} | Result age {age}s", "channelNumber": "Channel", "channelApCount": "AP count", "channelSignal": "Strongest AP (dBm)", "channelScore": "Estimated AP pressure", "channelRange": "— marks channels outside the reported scan range, or no observed AP signal. APs count distinct BSSIDs; virtual APs are not necessarily separate routers.", "channelTesting": "After applying, test with Trackers and your usual VR setup. For suspected USB 3.0 interference, compare moving the Dongle away with a USB extension.", "channelAge": "Results expire after 5 minutes and are checked again before saving.", "channelAuto": "Auto", "channelNone": "None", "channelFailure": "Channel analysis failed: {error}"});
Object.assign(translations["ja"], {"channelTitle": "周辺 WiFi AP とチャンネル選択の参考", "channelIntro": "Tracker をすべて切り、通常の VR 用 PC・HMD・ルーター・無線機器を動かした状態で、普段の Dongle 設置位置で測定してください。", "channelLimits": "周辺 2.4GHz AP の信号とチャンネルからの推定で、干渉率や速度ではありません。Bluetooth・無線音声・USB 3.0 雑音・AP の実トラフィックは十分測定できません。低スコアはこのモデル上の AP 影響が小さいだけで、Tracker 使用時の確認が必要です。", "channelStart": "スキャン開始", "channelCancel": "スキャン中止", "channelRead": "結果を取得", "channelSave": "推奨を保存", "channelConfirmTitle": "WiFi AP をスキャンしますか？", "channelConfirm": "全 Tracker を切り、Dongle の WiFi クライアントを切断してください。無線チャンネルを一時切替し、再接続に影響する場合があります。USB は利用できます。接続があれば中止します。", "channelApplyTitle": "推奨チャンネルを保存しますか？", "channelApply": "チャンネル {channel} を保存します。使用終了後に Dongle を再起動するまで現在のチャンネルは変わりません。Tracker は一時切断・再接続します。結果は 5 分で失効します。", "channelSaved": "保存しました。Dongle 再起動後に適用されます。", "channelIdle": "未測定", "channelRunning": "スキャン中。Tracker は切ったままにしてください…", "channelReady": "完了。候補は 1/6/11。僅差なら現在の候補を維持します。", "channelEmpty": "有効 AP が見つからず、推奨の根拠がありません。現在の設定を維持します。", "channelError": "失敗またはタイムアウトです。再取得または再試行してください。", "channelInterrupted": "端末が接続したため中止しました。Tracker と WiFi クライアントを切断して再試行してください。", "channelCancelled": "中止しました", "channelUnavailable": "結果が不完全です。ファーム更新または再取得してください。", "channelSummary": "現在 {current}｜保存 {saved}｜参考推奨 {recommended}｜AP {total}｜経過 {age} 秒", "channelNumber": "チャンネル", "channelApCount": "AP 数", "channelSignal": "最強 AP (dBm)", "channelScore": "AP 影響スコア（推定）", "channelRange": "— は報告されたスキャン範囲外、または AP 信号未観測です。異なる BSSID を数えるため、仮想 AP は別の物理ルーターとは限りません。", "channelTesting": "適用後は通常の VR 環境と Tracker で安定性を比較してください。USB 3.0 干渉を疑う場合、USB 延長で Dongle を離した状態も比較してください。", "channelAge": "結果は 5 分で失効し、保存時にも再確認します。", "channelAuto": "自動", "channelNone": "なし", "channelFailure": "チャンネル分析失敗：{error}"});
function detectLanguage() {
  try {
    const saved = localStorage.getItem(LANGUAGE_STORAGE_KEY);
    if (translations[saved]) return saved;
  } catch (_) {}
  for (const language of navigator.languages || [navigator.language]) {
    const normalized = String(language).toLowerCase();
    if (normalized.startsWith("zh")) return "zh-Hant";
    if (normalized.startsWith("ja")) return "ja";
  }
  return "en";
}

let currentLanguage = detectLanguage();

function t(key, variables = {}) {
  const template = translations[currentLanguage][key] ?? translations.en[key] ?? key;
  return Object.entries(variables).reduce((text, [name, value]) => text.replaceAll(`{${name}}`, value), template);
}

function applyLanguage(language, save = false) {
  currentLanguage = translations[language] ? language : "en";
  document.documentElement.lang = currentLanguage;
  document.querySelector('meta[name="description"]').content = t("pageDescription");
  document.querySelectorAll("[data-i18n]").forEach((element) => {
    if (state.port && (element.id === "tracker-list" || element.id === "terminal-output")) return;
    element.textContent = t(element.dataset.i18n);
  });
  document.querySelectorAll("[data-i18n-html]").forEach((element) => { element.innerHTML = t(element.dataset.i18nHtml); });
  document.querySelectorAll("[data-i18n-placeholder]").forEach((element) => { element.placeholder = t(element.dataset.i18nPlaceholder); });
  document.querySelectorAll("[data-i18n-aria]").forEach((element) => { element.setAttribute("aria-label", t(element.dataset.i18nAria)); });
  document.querySelectorAll("[data-diag-help]").forEach(button => {
    button.setAttribute("aria-label", t("helpLabel", {field:t(button.dataset.diagHelp)}));
  });
  closeDiagnosticHelp();
  const selector = document.querySelector("#language-select");
  if (selector) selector.value = currentLanguage;
  if (save) {
    try { localStorage.setItem(LANGUAGE_STORAGE_KEY, currentLanguage); } catch (_) {}
  }
}

function updateBrowserWarning() {
  const warningKey = !("serial" in navigator)
    ? "unsupportedBrowser"
    : (!window.isSecureContext ? "secureContextRequired" : null);
  ui.connect.disabled = Boolean(warningKey);
  ui.warning.hidden = !warningKey;
  if (warningKey) ui.warning.textContent = t(warningKey);
}

const state = {
  port: null,
  reader: null,
  writer: null,
  reading: false,
  closing: false,
  pending: null,
  commandQueue: Promise.resolve(),
  trackerEntries: null,
  trackerFallback: null,
  diagnosticRows: [],
  diagnosticsAvailable: false,
  sensorRows: [],
  channelResult: null,
  channelTimer: null,
  channelBusy: false,
};

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];

const ui = {
  connect: $("#connect-button"),
  disconnect: $("#disconnect-button"),
  connectionLabel: $("#connection-label"),
  connectionCard: $(".connect-card"),
  warning: $("#browser-warning"),
  terminal: $("#terminal-output"),
  terminalInput: $("#terminal-command"),
  terminalSend: $("#terminal-send"),
  wifiForm: $("#wifi-form"),
  wifiSsid: $("#wifi-ssid"),
  wifiPassword: $("#wifi-password"),
  wifiChannel: $("#wifi-channel"),
  wifiError: $("#wifi-error"),
  restartBadge: $("#restart-badge"),
  trackerList: $("#tracker-list"),
  trackerConnectedCapacity: $("#tracker-connected-capacity"),
  trackerStoredCapacity: $("#tracker-stored-capacity"),
  dialog: $("#confirm-dialog"),
  dialogTitle: $("#dialog-title"),
  dialogMessage: $("#dialog-message"),
  dialogConfirm: $("#dialog-confirm"),
  toast: $("#toast"),
};

let toastTimer = 0;

function updateCapacity(element, count) {
  const validCount = Number.isFinite(count) ? count : null;
  element.textContent = `${validCount ?? "—"} / ${MAX_TRACKERS}`;
  element.classList.toggle("is-full", validCount !== null && validCount >= MAX_TRACKERS);
}

function showToast(message, error = false) {
  clearTimeout(toastTimer);
  ui.toast.textContent = message;
  ui.toast.classList.toggle("is-error", error);
  ui.toast.classList.add("is-visible");
  toastTimer = setTimeout(() => ui.toast.classList.remove("is-visible"), 3200);
}

function setControlsEnabled(enabled) {
  $$('[data-command], #refresh-button, #wifi-save, #wifi-read, #password-toggle, #tracker-read, #tracker-clear, #terminal-command, #terminal-send, #reboot-button, #bootloader-button, #wifi-reset, #wifi-ssid, #wifi-password, #wifi-channel, #export-diagnostics, #sensor-tracker, #sensor-read, #channel-start, #channel-cancel, #channel-read, #channel-save, [data-remove-tracker]')
    .forEach((element) => { element.disabled = !enabled; });
}

function setConnected(connected) {
  ui.connectionCard.classList.toggle("is-connected", connected);
  ui.connectionLabel.textContent = t(connected ? "connected" : "disconnected");
  ui.connect.hidden = connected;
  ui.disconnect.hidden = !connected;
  setControlsEnabled(connected);
  if (!connected) {
    ui.restartBadge.hidden = true;
  }
}

function appendTerminal(text, prefix = "") {
  if (ui.terminal.textContent === t("terminalWaiting")) ui.terminal.textContent = "";
  ui.terminal.textContent += prefix + text;
  ui.terminal.scrollTop = ui.terminal.scrollHeight;
}

function finishPending() {
  const pending = state.pending;
  if (!pending) return;
  clearTimeout(pending.idleTimer);
  clearTimeout(pending.timeoutTimer);
  state.pending = null;
  pending.resolve(pending.text.trim());
}

function rejectPending(error) {
  const pending = state.pending;
  if (!pending) return;
  clearTimeout(pending.idleTimer);
  clearTimeout(pending.timeoutTimer);
  state.pending = null;
  pending.reject(error);
}

function receiveText(text) {
  if (!state.pending || state.pending.logToTerminal) appendTerminal(text);
  if (!state.pending) return;
  state.pending.text += text;
  clearTimeout(state.pending.idleTimer);
  state.pending.idleTimer = setTimeout(finishPending, RESPONSE_IDLE_MS);
}

async function readLoop() {
  state.reading = true;
  try {
    while (state.reader) {
      const { value, done } = await state.reader.read();
      if (done) break;
      if (value?.length) receiveText(decoder.decode(value, { stream: true }));
    }
  } catch (error) {
    if (!state.closing) showToast(t("connectionInterrupted", { error: error.message }), true);
  } finally {
    state.reading = false;
    if (!state.closing && state.port) await closePort(false);
  }
}

async function connectPort() {
  if (!("serial" in navigator)) return;
  try {
    const port = await navigator.serial.requestPort({ filters: USB_FILTERS });
    await port.open({ baudRate: BAUD_RATE, bufferSize: 4096 });
    try {
      await port.setSignals({ dataTerminalReady: false, requestToSend: false });
    } catch (_) {
      // Signal control is optional; firmware output does not depend on DTR.
    }
    state.port = port;
    state.writer = port.writable.getWriter();
    state.reader = port.readable.getReader();
    state.closing = false;
    setConnected(true);
    setControlsEnabled(false);
    ui.terminal.textContent = "";
    appendTerminal(t("terminalConnected"));
    void readLoop();
    const info = await refreshInfo();
    if (!/^Product:\s*SlimeVR WiFi Dongle\s*$/mi.test(info)) {
      throw new Error(t("incompatibleDevice"));
    }
    await refreshStatus();
    await refreshWifi();
    await refreshTrackers();
    await refreshDiagnostics();
    await refreshChannels();
    setControlsEnabled(true);
    renderChannels();
    showToast(t("connectedToast"));
  } catch (error) {
    if (state.port) await closePort(false);
    const busy = /Failed to open|Access denied|NetworkError/i.test(error.message);
    showToast(busy ? t("portBusy") : t("connectionFailed", { error: error.message }), true);
  }
}

async function closePort(showMessage = true) {
  if (!state.port) return;
  state.closing = true;
  rejectPending(new Error(t("serialClosed")));
  const port = state.port;
  try {
    if (state.reader) {
      await state.reader.cancel().catch(() => {});
      state.reader.releaseLock();
    }
    state.reader = null;
    if (state.writer) state.writer.releaseLock();
    state.writer = null;
    await port.close().catch(() => {});
  } finally {
    state.port = null;
    state.closing = false;
    state.trackerEntries = null;
    state.trackerFallback = null;
    ui.trackerList.textContent = t("trackerConnectPrompt");
    updateCapacity(ui.trackerConnectedCapacity, null);
    updateCapacity(ui.trackerStoredCapacity, null);
    setConnected(false);
    $("#diagnostics-body").replaceChildren();
    $("#diagnostics-summary").textContent = "—";
    state.diagnosticRows = []; state.sensorRows = [];
    clearTimeout(state.channelTimer); state.channelResult = null; state.channelBusy = false; renderChannels();
    $("#sensor-body").replaceChildren(); $("#sensor-tracker").replaceChildren();
    if (showMessage) showToast(t("disconnectedToast"));
  }
}

async function sendCommandNow(command, logToTerminal = true) {
  if (!state.writer || !state.port) throw new Error(t("dongleNotConnected"));
  if (state.pending) throw new Error(t("previousPending"));
  if (logToTerminal) appendTerminal(`\n> ${command}\n`);
  const response = new Promise((resolve, reject) => {
    state.pending = {
      text: "",
      logToTerminal,
      resolve,
      reject,
      idleTimer: 0,
      timeoutTimer: setTimeout(() => {
        if (state.pending) finishPending();
      }, RESPONSE_TIMEOUT_MS),
    };
  });
  try {
    await state.writer.write(encoder.encode(`${command}\n`));
  } catch (error) {
    rejectPending(error);
    throw error;
  }
  return response;
}

function sendCommand(command, logToTerminal = true) {
  const operation = state.commandQueue.then(() => sendCommandNow(command, logToTerminal));
  state.commandQueue = operation.catch(() => {});
  return operation;
}

async function sendWithoutReply(command) {
  if (!state.writer || !state.port) throw new Error(t("dongleNotConnected"));
  appendTerminal(`\n> ${command}\n`);
  await state.writer.write(encoder.encode(`${command}\n`));
  await new Promise((resolve) => setTimeout(resolve, 120));
  await closePort(false);
}

function parseKeyValues(text) {
  const values = {};
  for (const line of text.split(/\r?\n/)) {
    const match = line.match(/^\s*([^:]+?)\s*:\s*(.*)$/);
    if (match) values[match[1].trim().toLowerCase()] = match[2].trim();
  }
  return values;
}

function updateInfo(text) {
  const info = parseKeyValues(text);
  $("#device-product").textContent = info.product || "—";
  $("#device-firmware").textContent = info.firmware || "—";
  $("#device-serial").textContent = info["usb serial"] || "—";
  $("#device-chip").textContent = info.chip || "—";
}

function updateStatus(text) {
  const status = parseKeyValues(text);
  $("#status-uptime").textContent = status.uptime || "—";
  $("#status-hid").textContent = status["hid ready"] === "yes" ? t("ready") : (status["hid ready"] || "—");
  $("#status-channel").textContent = status.channel || "—";
  $("#status-stations").textContent = status["connected stations"] || "—";
  updateCapacity(ui.trackerConnectedCapacity, Number.parseInt(status["connected stations"], 10));
  updateCapacity(ui.trackerStoredCapacity, Number.parseInt(status["stored trackers"], 10));
  $("#status-temperature").textContent = status["chip temperature"] || "—";
  const dropped = status["dropped packets"] ?? "—";
  const failed = status["failed hid reports"] ?? "—";
  $("#status-packets").textContent = t("packetValue", { dropped, failed });
  ui.restartBadge.hidden = status["wifi restart required"] !== "yes";
  $("#diagnostics-summary").textContent = t("diagSummary", {sent:status["successful hid reports"] ?? "—", failed, dropped, coalesced:status["coalesced poses"] ?? "—"});
}

function updateWifi(text) {
  const wifi = parseKeyValues(text);
  if (wifi.ssid !== undefined) ui.wifiSsid.value = wifi.ssid;
  if (wifi.password !== undefined) ui.wifiPassword.value = wifi.password;
  if (wifi.channel !== undefined) {
    ui.wifiChannel.value = wifi.channel.startsWith("auto") ? "auto" : wifi.channel;
  }
  ui.restartBadge.hidden = wifi["reboot required"] !== "yes";
}

async function refreshInfo(logToTerminal = false) {
  const response = await sendCommand("info", logToTerminal);
  updateInfo(response);
  return response;
}

async function refreshStatus(logToTerminal = false) {
  const response = await sendCommand("status", logToTerminal);
  updateStatus(response);
  return response;
}

async function refreshWifi(logToTerminal = false) {
  const response = await sendCommand("wifi show", logToTerminal);
  updateWifi(response);
  return response;
}

function renderTrackerList() {
  if (state.trackerEntries === null) return;
  ui.trackerList.replaceChildren();
  if (!state.trackerEntries.length) {
    ui.trackerList.textContent = state.trackerFallback || t("noStoredTrackers"); return;
  }
  for (const {id,mac} of state.trackerEntries) {
    const row = document.createElement("div"); row.className = "tracker-entry";
    const label = document.createElement("span");
    label.textContent = t("trackerDisplay", {number:id+1,id,mac});
    const button = document.createElement("button"); button.type = "button";
    button.className = "button button-quiet"; button.textContent = t("removeOne");
    button.dataset.removeTracker = String(id); button.disabled = !state.port;
    button.addEventListener("click", () => removeTrackerBinding(id));
    row.append(label,button); ui.trackerList.append(row);
  }
}
async function removeTrackerBinding(id) {
  if (!Number.isInteger(id) || id < 0 || id >= MAX_TRACKERS) return;
  if (!(await confirmAction(t("removeTitle"),t("removeMessage"),t("removeOne")))) return;
  try {
    const response = await sendCommand(`trackers remove ${id}`, false);
    if (!response.startsWith("Removed.")) throw new Error(response.trim() || t("noReply"));
    await refreshTrackers(); await refreshStatus(); await refreshDiagnostics();
    showToast(t("removed"));
  } catch(error) {showToast(t("operationFailed", {error:error.message}),true);}
}

async function refreshTrackers(logToTerminal = false) {
  const response = await sendCommand("trackers list", logToTerminal);
  const lines = response
    .replace(/^Stored trackers:\s*/i, "")
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean);
  const trackers = lines
    .map((line) => line.match(/^(\d+)\s*:\s*(.+)$/))
    .filter(Boolean)
    .map((match) => ({
      id: Number.parseInt(match[1], 10),
      mac: match[2],
    }));
  const empty = lines.some((line) => /^No stored trackers\.?$/i.test(line));
  state.trackerEntries = trackers;
  state.trackerFallback = !trackers.length && !empty && lines.length
    ? lines.join("\n")
    : null;
  renderTrackerList();
  updateCapacity(ui.trackerStoredCapacity, trackers.length);
  return response;
}

async function refreshCurrentStatus() {
  try {
    await refreshStatus();
    await refreshDiagnostics();
    showToast(t("statusUpdated"));
  } catch (error) {
    showToast(t("readFailed", { error: error.message }), true);
  }
}

async function refreshDiagnostics() {
  const response = await sendCommand("diagnostics", false);
  const body = $("#diagnostics-body");
  body.replaceChildren();
  state.diagnosticRows = [];
  state.diagnosticsAvailable = false;
  if (!response.includes("Diagnostics v1:") || !response.includes("Diagnostics end")) {
    const row = body.insertRow(); const cell = row.insertCell(); cell.colSpan = 11;
    cell.textContent = t("diagUnavailable"); return;
  }
  state.diagnosticsAvailable = true;
  for (const line of response.split(/\r?\n/)) {
    if (!/^D,/.test(line)) continue;
    const values = line.slice(2).split(",");
    if (values.length !== 11 || !values.every(value => /^-?\d+$/.test(value))) continue;
    state.diagnosticRows.push(values.map(Number));
    const mask = Number(values[5]);
    values[5] = Array.from({length:16}, (_, i) => i).filter(i => mask & (1 << i)).join(", ");
    const row = body.insertRow();
    for (const value of values) row.insertCell().textContent = value;
  }
  updateSensorChoices();
  await refreshSensors();
}

const imuNames = ["Unknown","MPU9250","MPU6500","BNO080","BNO085","BNO055","MPU6050","BNO086","BMI160","ICM20948","ICM42688","BMI270","LSM6DS3TRC","LSM6DSV","LSM6DSO","LSM6DSR","ICM45686","ICM45605","ADC_RESISTANCE"];
function parseSensorReport(response, trackerId) {
  if (!response.includes("Sensors v1:") || !response.includes("Sensors end")) return null;
  const rows = [];
  for (const line of response.split(/\r?\n/)) {
    if (!line.startsWith("S,")) continue;
    const fields = line.slice(2).split(",");
    if (fields.length !== 8 || !fields.every(v => /^-?\d+$/.test(v))) continue;
    const values = fields.map(Number);
    if (values[0] !== trackerId || values[1] < 0 || values[1] > 15) continue;
    rows.push(values);
  }
  return rows;
}
function updateSensorChoices() {
  const select = $("#sensor-tracker"), previous = select.value;
  select.replaceChildren();
  const ids = [...new Set([...(state.trackerEntries || []).map(t => t.id), ...state.diagnosticRows.map(row => row[0])])].filter(id => Number.isInteger(id) && id >= 0 && id < MAX_TRACKERS).sort((a,b)=>a-b);
  for (const id of ids) {const option=document.createElement("option"); option.value=String(id);option.textContent=`Tracker ${id}`;select.append(option);}
  if (ids.includes(Number(previous)) && previous !== "") select.value=previous;
}
function renderSensors() {
  const body=$("#sensor-body");body.replaceChildren();
  for (const values of state.sensorRows) {
    const shown=values.map(String);
    shown[3]=values[3]<0 ? "—" : String(values[3]);
    shown[4]=({0:t("stateOffline"),1:t("stateOk"),2:t("stateError")})[values[4]] || "—";
    shown[5]=values[5]<0 ? "—" : `${imuNames[values[5]] || "Unknown"} (${values[5]})`;
    shown[6]=values[6]<1 ? "—" : (values[6]/2-39).toFixed(1);
    const row=body.insertRow();for(const value of shown)row.insertCell().textContent=value;
  }
}
async function refreshSensors() {
  const value=$("#sensor-tracker").value;
  state.sensorRows=[];renderSensors();
  if (value === "") return;
  const id=Number(value);if(!Number.isInteger(id)||id<0||id>=MAX_TRACKERS)return;
  const rows=parseSensorReport(await sendCommand(`sensors ${id}`,false),id);
  if(rows===null){const cell=$("#sensor-body").insertRow().insertCell();cell.colSpan=8;cell.textContent=t("sensorUnavailable");return;}
  state.sensorRows=rows;renderSensors();
}
function createDiagnosticReport(infoText,statusText,diagnosticRows,sensorRows) {
  const info=parseKeyValues(infoText),status=parseKeyValues(statusText);
  const pick=(source,keys)=>Object.fromEntries(keys.filter(key=>source[key]!==undefined).map(key=>[key,source[key]]));
  return {
    schemaVersion:1, exportedAt:new Date().toISOString(),
    device:pick(info,["product","firmware","build","chip","cpu frequency","flash size"]),
    status:pick(status,["uptime","hid ready","channel","connected stations","stored trackers","free heap","chip temperature","dropped packets","failed hid reports","coalesced poses","successful hid reports","rejected udp packets"]),
    trackerColumns:["tracker_id","online","rssi_dbm","udp_pps","last_rx_age_ms","sensor_mask","rx_total","coalesced_poses","usb_failed_attempts","rejected_udp","control_queue_drops"],
    trackers:diagnosticRows.map(row=>[...row]),
    sensorColumns:["tracker_id","sensor_id","pose_in_last_1500ms","last_pose_age_ms","reported_state","imu_id","temperature_encoded","pose_total"],
    sensors:sensorRows.map(row=>[...row]),
    notes:["Counters are since Dongle boot. Samples are sequential, not one atomic snapshot.","Unknown sensor values use -1. Temperature encoded > 0 decodes as value / 2 - 39 C.","This report does not measure radio loss or end-to-end latency.","WiFi SSID/password, MAC addresses, USB serial and terminal logs are excluded."]
  };
}
async function exportDiagnostics() {
  const button=$("#export-diagnostics");button.disabled=true;
  try {
    const info=await sendCommand("info",false),status=await sendCommand("status",false);
    await refreshTrackers();await refreshDiagnostics();
    if (!state.diagnosticsAvailable) throw new Error(t("diagUnavailable"));
    const sensors=[];
    for(const tracker of state.trackerEntries || []) {
      if(!Number.isInteger(tracker.id)||tracker.id<0||tracker.id>=MAX_TRACKERS)continue;
      const rows=parseSensorReport(await sendCommand(`sensors ${tracker.id}`,false),tracker.id);
      if(rows===null)throw new Error(t("sensorUnavailable"));
      sensors.push(...rows);
    }
    const report=createDiagnosticReport(info,status,state.diagnosticRows,sensors);
    const url=URL.createObjectURL(new Blob([JSON.stringify(report,null,2)],{type:"application/json"}));
    const anchor=document.createElement("a");anchor.href=url;anchor.download=`slimevr-diagnostics-${new Date().toISOString().replace(/[:.]/g,"-")}.json`;
    document.body.append(anchor);anchor.click();anchor.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
    showToast(t("exported"));
  } catch(error){showToast(t("exportFailed",{error:error.message}),true);}
  finally {button.disabled=!state.port;}
}
$("#export-diagnostics").addEventListener("click",exportDiagnostics);
$("#sensor-tracker").addEventListener("change",()=>refreshSensors().catch(error=>showToast(error.message,true)));
$("#sensor-read").addEventListener("click",()=>refreshSensors().catch(error=>showToast(error.message,true)));

function parseChannelReport(text) {
  if (!text.includes("Channels v1") || !text.includes("Channels end")) return null;
  const fields=parseKeyValues(text);
  if (!["idle","running","ready","empty","error","interrupted","cancelled"].includes(fields.state)) return null;
  const result={state:fields.state,rows:[]};
  for (const key of ["generation","age_ms","current","saved","recommended","ap_total","allowed_mask"]) {
    if (!/^\d+$/.test(fields[key] || "")) return null;
    result[key]=Number(fields[key]);
    if (!Number.isSafeInteger(result[key]) || result[key]>4294967295) return null;
  }
  if(result.current>14 || result.saved>13 || ![0,1,6,11].includes(result.recommended) || result.allowed_mask>8191) return null;
  const seen=new Set();
  for(const line of text.split(/\r?\n/)) {
    if(!line.startsWith("C,"))continue;
    const values=line.slice(2).split(",");
    if(values.length!==4 || !values.every(v=>/^-?\d+$/.test(v)))return null;
    const row=values.map(Number);
    if(row[0]<1||row[0]>13||seen.has(row[0])||row[1]<0||row[2]>0||row[2]<-127||row[3]<0||!row.every(Number.isSafeInteger))return null;
    seen.add(row[0]);result.rows.push(row);
  }
  if(["ready","empty"].includes(result.state) && result.rows.length!==13)return null;
  result.observedAt=Date.now();return result;
}
function renderChannels() {
  const result=state.channelResult, connected=Boolean(state.port);
  const running=state.channelBusy || result?.state==="running";
  $("#channel-start").disabled=!connected||running;
  $("#channel-cancel").disabled=!connected||!running;
  $("#channel-read").disabled=!connected||state.channelBusy;
  const age=result ? result.age_ms+Math.max(0,Date.now()-result.observedAt) : 0;
  $("#channel-save").disabled=!connected||running||result?.state!=="ready"||!result.recommended||age>300000;
  const keys={idle:"channelIdle",running:"channelRunning",ready:"channelReady",empty:"channelEmpty",error:"channelError",interrupted:"channelInterrupted",cancelled:"channelCancelled"};
  $("#channel-state").textContent=t(running ? "channelRunning" : keys[result?.state] || "channelIdle");
  $("#channel-summary").textContent=result ? t("channelSummary",{current:result.current,saved:result.saved||t("channelAuto"),recommended:result.recommended||t("channelNone"),total:result.ap_total,age:Math.floor(age/1000)}) : "";
  const body=$("#channel-body"),cards=$("#channel-candidates");body.replaceChildren();cards.replaceChildren();
  if(!result)return;
  for(const values of result.rows) {
    const [channel,count,rssi,score]=values,allowed=Boolean(result.allowed_mask&(1<<(channel-1)));
    const row=body.insertRow();
    for(const value of [channel,allowed?count:"—",rssi||"—",allowed?score:"—"])row.insertCell().textContent=String(value);
    if([1,6,11].includes(channel)) {
      const card=document.createElement("div");card.className="channel-card";
      card.textContent=`${t("channelNumber")} ${channel}: ${allowed?score:"—"}`;
      if(channel===result.recommended)card.classList.add("is-suggested");cards.append(card);
    }
  }
}
async function refreshChannels() {
  clearTimeout(state.channelTimer);
  const result=parseChannelReport(await sendCommand("channels status",false));
  state.channelResult=result;renderChannels();
  if(!result){$("#channel-state").textContent=t("channelUnavailable");return;}
  if(result.state==="running" && state.port)
    state.channelTimer=setTimeout(()=>refreshChannels().catch(error=>showToast(error.message,true)),700);
}
async function startChannelScan() {
  if(!(await confirmAction(t("channelConfirmTitle"),t("channelConfirm"),t("channelStart"))))return;
  state.channelBusy=true;renderChannels();
  try {
    const response=await sendCommand("channels start",false);
    if(!response.startsWith("Scan started."))throw new Error(response.trim()||t("noReply"));
    state.channelResult=null;
    await refreshChannels();
  } catch(error){showToast(t("channelFailure",{error:error.message}),true);}
  finally {state.channelBusy=false;renderChannels();}
}
async function saveChannelSuggestion() {
  const result=state.channelResult;
  if(!result || result.state!=="ready" || !result.recommended)return;
  if(!(await confirmAction(t("channelApplyTitle"),t("channelApply",{channel:result.recommended}),t("channelSave"))))return;
  try {
    const response=await sendCommand(`channels apply ${result.generation} ${result.recommended}`,false);
    if(!response.startsWith("Saved."))throw new Error(response.trim()||t("noReply"));
    await refreshWifi();await refreshStatus();await refreshChannels();showToast(t("channelSaved"));
  } catch(error){showToast(t("channelFailure",{error:error.message}),true);}
}
$("#channel-start").addEventListener("click",startChannelScan);
$("#channel-read").addEventListener("click",()=>refreshChannels().catch(error=>showToast(error.message,true)));
$("#channel-cancel").addEventListener("click",async()=>{
  try {await sendCommand("channels cancel",false);await refreshChannels();}
  catch(error){showToast(error.message,true);}
});
$("#channel-save").addEventListener("click",saveChannelSuggestion);

function byteLength(value) {
  return encoder.encode(value).length;
}

function quoteCliValue(value) {
  if (/\r|\n|\0/.test(value)) throw new Error(t("invalidLineBreak"));
  if (!value.includes('"')) return `"${value}"`;
  if (!value.includes("'")) return `'${value}'`;
  if (!/^['"]|['"]$/.test(value)) return value;
  throw new Error(t("invalidQuotes"));
}

async function saveWifi(event) {
  event.preventDefault();
  ui.wifiError.textContent = "";
  const ssid = ui.wifiSsid.value;
  const password = ui.wifiPassword.value;
  const channel = ui.wifiChannel.value;
  const ssidBytes = byteLength(ssid);
  const passwordBytes = byteLength(password);
  if (ssidBytes < 1 || ssidBytes > 32) {
    ui.wifiError.textContent = t("ssidInvalid");
    return;
  }
  if (passwordBytes < 8 || passwordBytes > 63) {
    ui.wifiError.textContent = t("passwordInvalid");
    return;
  }
  try {
    const commands = [
      `wifi set ssid ${quoteCliValue(ssid)}`,
      `wifi set password ${quoteCliValue(password)}`,
      `wifi set channel ${channel}`,
    ];
    for (const command of commands) {
      const response = await sendCommand(command, false);
      if (!/^Saved\./m.test(response)) throw new Error(response || t("noReply"));
    }
    ui.restartBadge.hidden = false;
    showToast(t("wifiSaved"));
  } catch (error) {
    ui.wifiError.textContent = t("saveFailed", { error: error.message });
  }
}

function confirmAction(title, message, confirmLabel = t("confirm")) {
  ui.dialogTitle.textContent = title;
  ui.dialogMessage.textContent = message;
  ui.dialogConfirm.textContent = confirmLabel;
  ui.dialog.showModal();
  return new Promise((resolve) => {
    ui.dialog.addEventListener("close", () => resolve(ui.dialog.returnValue === "confirm"), { once: true });
  });
}

async function guardedAction(title, message, command, label) {
  if (!(await confirmAction(title, message, label))) return;
  try {
    await sendWithoutReply(command);
    showToast(t("commandSent"));
  } catch (error) {
    showToast(error.message, true);
  }
}

ui.connect.addEventListener("click", connectPort);
ui.disconnect.addEventListener("click", () => closePort());
$("#refresh-button").addEventListener("click", refreshCurrentStatus);
$("#wifi-read").addEventListener("click", refreshWifi);
$("#tracker-read").addEventListener("click", refreshTrackers);
ui.wifiForm.addEventListener("submit", saveWifi);
$("#language-select").addEventListener("change", (event) => {
  applyLanguage(event.currentTarget.value, true);
  renderTrackerList();
  renderSensors();
  setConnected(Boolean(state.port));
  renderChannels();
  $("#password-toggle").textContent = t(ui.wifiPassword.type === "text" ? "hidePassword" : "showPassword");
  updateBrowserWarning();
});

$("#password-toggle").addEventListener("click", (event) => {
  const visible = ui.wifiPassword.type === "text";
  ui.wifiPassword.type = visible ? "password" : "text";
  event.currentTarget.textContent = t(visible ? "showPassword" : "hidePassword");
});

$$('[data-command]').forEach((button) => {
  button.addEventListener("click", async () => {
    try {
      const response = await sendCommand(button.dataset.command);
      if (button.dataset.command === "info") updateInfo(response);
      if (button.dataset.command === "status") updateStatus(response);
      const toastKey = {
        info: "infoUpdated",
        status: "statusUpdated",
        meow: "meowComplete",
        help: "helpShown",
      }[button.dataset.command] || "commandComplete";
      showToast(t(toastKey));
    } catch (error) {
      showToast(error.message, true);
    }
  });
});

$("#terminal-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const command = ui.terminalInput.value.trim();
  if (!command) return;
  ui.terminalInput.value = "";
  try {
    await sendCommand(command);
  } catch (error) {
    showToast(error.message, true);
  }
});

$("#terminal-clear").addEventListener("click", () => { ui.terminal.textContent = ""; });
$("#reboot-button").addEventListener("click", async () => {
  try {
    await sendWithoutReply("reboot");
    showToast(t("rebooting"));
  } catch (error) {
    showToast(error.message, true);
  }
});
$("#bootloader-button").addEventListener("click", () => guardedAction(t("bootTitle"), t("bootMessage"), "bootloader", t("enterBootloader")));
$("#tracker-clear").addEventListener("click", () => guardedAction(t("trackerClearTitle"), t("trackerClearMessage"), "trackers clear", t("clearAndReboot")));
$("#wifi-reset").addEventListener("click", async () => {
  if (!(await confirmAction(t("wifiResetTitle"), t("wifiResetMessage"), t("restoreDefault")))) return;
  try {
    const response = await sendCommand("wifi reset", false);
    if (!response.startsWith("Default WiFi")) throw new Error(response);
    await refreshWifi();
    ui.restartBadge.hidden = false;
    showToast(t("wifiRestored"));
  } catch (error) {
    showToast(t("operationFailed", { error: error.message }), true);
  }
});

if ("serial" in navigator) {
  navigator.serial.addEventListener("disconnect", (event) => {
    if (state.port && (event.port === state.port || event.target === state.port) && !state.closing) {
      void closePort(false);
    }
  });
} else {
  updateBrowserWarning();
}

let helpAnchor = null;
let helpPinned = false;
let helpHideTimer;
const diagnosticTooltip = document.querySelector("#diagnostic-tooltip");
function closeDiagnosticHelp() {
  clearTimeout(helpHideTimer);
  if (helpAnchor) helpAnchor.removeAttribute("aria-describedby");
  helpAnchor = null; helpPinned = false;
  diagnosticTooltip.hidden = true;
}
function showDiagnosticHelp(button) {
  clearTimeout(helpHideTimer);
  if (helpAnchor && helpAnchor !== button) helpAnchor.removeAttribute("aria-describedby");
  helpAnchor = button;
  diagnosticTooltip.textContent = t(button.dataset.diagHelp + "Help");
  diagnosticTooltip.hidden = false;
  button.setAttribute("aria-describedby", "diagnostic-tooltip");
  const anchor = button.getBoundingClientRect();
  const width = diagnosticTooltip.getBoundingClientRect().width;
  diagnosticTooltip.style.left = Math.max(12, Math.min(anchor.left, window.innerWidth - width - 12)) + "px";
  const height = diagnosticTooltip.getBoundingClientRect().height;
  const top = anchor.bottom + 8 + height <= window.innerHeight - 12 ? anchor.bottom + 8 : anchor.top - height - 8;
  diagnosticTooltip.style.top = Math.max(12, top) + "px";
}
function scheduleHelpHide() {
  if (!helpPinned) helpHideTimer = setTimeout(closeDiagnosticHelp, 150);
}
document.querySelectorAll("[data-diag-help]").forEach(button => {
  button.addEventListener("mouseenter", () => {helpPinned = false; showDiagnosticHelp(button);});
  button.addEventListener("mouseleave", scheduleHelpHide);
  button.addEventListener("focus", () => showDiagnosticHelp(button));
  button.addEventListener("blur", () => {if (!helpPinned) closeDiagnosticHelp();});
  button.addEventListener("click", () => {
    if (helpAnchor === button && helpPinned) closeDiagnosticHelp();
    else {showDiagnosticHelp(button); helpPinned = true;}
  });
});
diagnosticTooltip.addEventListener("mouseenter", () => clearTimeout(helpHideTimer));
diagnosticTooltip.addEventListener("mouseleave", scheduleHelpHide);
document.addEventListener("click", event => {
  if (!event.target.closest("[data-diag-help]") && !diagnosticTooltip.contains(event.target)) closeDiagnosticHelp();
});
document.addEventListener("keydown", event => {if (event.key === "Escape") closeDiagnosticHelp();});
window.addEventListener("resize", closeDiagnosticHelp);
window.addEventListener("scroll", () => {
  if (!helpAnchor) return;
  const rect = helpAnchor.getBoundingClientRect();
  if (rect.bottom < 0 || rect.top > window.innerHeight) closeDiagnosticHelp();
  else showDiagnosticHelp(helpAnchor);
}, true);

applyLanguage(currentLanguage);
setConnected(false);
renderChannels();
updateBrowserWarning();
