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
    terminalConnected: "[已連接] SlimeVR WiFi Dongle\n", connectionInterrupted: "序列連線中斷：{error}", incompatibleDevice: "選取的裝置不是相容的 SlimeVR WiFi Dongle", connectedToast: "Dongle 已連接", portBusy: "無法開啟序列埠，請關閉其他序列工具後再試。", connectionFailed: "連線失敗：{error}", disconnectedToast: "已中斷 Dongle 連線", dongleNotConnected: "Dongle 尚未連接", previousPending: "上一個指令仍在處理", serialClosed: "序列埠已關閉", packetValue: "遺失 {dropped} / HID {failed}", readFailed: "讀取失敗：{error}", invalidLineBreak: "內容不能包含換行或 NUL", invalidQuotes: "內容不能同時以引號開頭或結尾並包含兩種引號", ssidInvalid: "SSID 必須是 1–32 bytes。", passwordInvalid: "密碼必須是 8–63 bytes。", noReply: "Dongle 沒有回覆", wifiSaved: "WiFi 設定已儲存，重新啟動後生效", saveFailed: "儲存失敗：{error}", commandComplete: "指令完成", commandSent: "指令已送出，Dongle 正在重新連線", rebooting: "Dongle 正在重新啟動", bootTitle: "進入 Bootloader？", bootMessage: "Dongle 將中斷目前的 HID 與序列連線，並以 ESP32 ROM Download Mode 重新出現。", trackerClearTitle: "清除所有 Tracker？", trackerClearMessage: "所有已儲存的 Tracker 對應會被刪除，Dongle 接著會重新啟動。", clearAndReboot: "清除並重啟", wifiResetTitle: "恢復預設 WiFi？", wifiResetMessage: "已儲存的 SSID、密碼與頻道會被清除；需要重新啟動才會套用。", restoreDefault: "恢復預設", wifiRestored: "已恢復預設 WiFi，重新啟動後生效", operationFailed: "操作失敗：{error}", unsupportedBrowser: "此瀏覽器不支援 Web Serial。請改用桌面版 Chrome 或 Edge。", secureContextRequired: "Web Serial 需要 HTTPS 安全連線。請從 GitHub Pages 網址開啟此頁。"
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
    terminalConnected: "[接続済み] SlimeVR WiFi Dongle\n", connectionInterrupted: "シリアル接続が切断されました：{error}", incompatibleDevice: "選択したデバイスは対応する SlimeVR WiFi Dongle ではありません", connectedToast: "Dongle に接続しました", portBusy: "シリアルポートを開けません。他のシリアルツールを閉じてから再試行してください。", connectionFailed: "接続に失敗しました：{error}", disconnectedToast: "Dongle との接続を切断しました", dongleNotConnected: "Dongle が接続されていません", previousPending: "前のコマンドを処理中です", serialClosed: "シリアルポートが閉じられました", packetValue: "破棄 {dropped} / HID {failed}", readFailed: "読み込みに失敗しました：{error}", invalidLineBreak: "改行または NUL は使用できません", invalidQuotes: "両方の引用符を含み、引用符で開始または終了する値は使用できません", ssidInvalid: "SSID は 1～32 bytes にしてください。", passwordInvalid: "パスワードは 8～63 bytes にしてください。", noReply: "Dongle から応答がありません", wifiSaved: "WiFi 設定を保存しました。再起動後に反映されます", saveFailed: "保存に失敗しました：{error}", commandComplete: "コマンドが完了しました", commandSent: "コマンドを送信しました。Dongle の再接続を待っています", rebooting: "Dongle を再起動しています", bootTitle: "Bootloader に入りますか？", bootMessage: "現在の HID とシリアル接続を切断し、ESP32 ROM Download Mode として再接続します。", trackerClearTitle: "すべての Tracker を消去しますか？", trackerClearMessage: "保存済みの Tracker マッピングをすべて削除し、Dongle を再起動します。", clearAndReboot: "消去して再起動", wifiResetTitle: "WiFi をデフォルトに戻しますか？", wifiResetMessage: "保存済みの SSID、パスワード、チャンネルを消去します。反映には再起動が必要です。", restoreDefault: "デフォルトに戻す", wifiRestored: "デフォルト WiFi 設定に戻しました。再起動後に反映されます", operationFailed: "操作に失敗しました：{error}", unsupportedBrowser: "このブラウザは Web Serial に対応していません。デスクトップ版 Chrome または Edge を使用してください。", secureContextRequired: "Web Serial には HTTPS 接続が必要です。GitHub Pages の URL から開いてください。"
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
    terminalConnected: "[Connected] SlimeVR WiFi Dongle\n", connectionInterrupted: "Serial connection interrupted: {error}", incompatibleDevice: "The selected device is not a compatible SlimeVR WiFi Dongle", connectedToast: "Dongle connected", portBusy: "Unable to open the serial port. Close other serial tools and try again.", connectionFailed: "Connection failed: {error}", disconnectedToast: "Dongle disconnected", dongleNotConnected: "Dongle is not connected", previousPending: "The previous command is still being processed", serialClosed: "Serial port closed", packetValue: "Dropped {dropped} / HID {failed}", readFailed: "Read failed: {error}", invalidLineBreak: "The value cannot contain a line break or NUL", invalidQuotes: "The value cannot begin or end with a quote while containing both quote types", ssidInvalid: "SSID must be 1–32 bytes.", passwordInvalid: "Password must be 8–63 bytes.", noReply: "The Dongle did not reply", wifiSaved: "WiFi settings saved; restart to apply", saveFailed: "Save failed: {error}", commandComplete: "Command complete", commandSent: "Command sent; waiting for the Dongle to reconnect", rebooting: "Dongle is restarting", bootTitle: "Enter Bootloader?", bootMessage: "The Dongle will disconnect its current HID and serial interfaces and reappear in ESP32 ROM Download Mode.", trackerClearTitle: "Clear all Trackers?", trackerClearMessage: "All stored Tracker mappings will be deleted and the Dongle will restart.", clearAndReboot: "Clear and restart", wifiResetTitle: "Restore default WiFi?", wifiResetMessage: "The saved SSID, password, and channel will be cleared. Restart to apply the defaults.", restoreDefault: "Restore defaults", wifiRestored: "Default WiFi settings restored; restart to apply", operationFailed: "Operation failed: {error}", unsupportedBrowser: "This browser does not support Web Serial. Use desktop Chrome or Edge.", secureContextRequired: "Web Serial requires a secure HTTPS connection. Open this page from its GitHub Pages URL."
  }
};

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
  $$('[data-command], #refresh-button, #wifi-save, #wifi-read, #password-toggle, #tracker-read, #tracker-clear, #terminal-command, #terminal-send, #reboot-button, #bootloader-button, #wifi-reset, #wifi-ssid, #wifi-password, #wifi-channel')
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
    setControlsEnabled(true);
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

  ui.trackerList.textContent = state.trackerEntries.length
    ? state.trackerEntries.map(({ id, mac }) => t("trackerDisplay", {
      number: id + 1,
      id,
      mac,
    })).join("\n")
    : (state.trackerFallback || t("noStoredTrackers"));
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
    showToast(t("statusUpdated"));
  } catch (error) {
    showToast(t("readFailed", { error: error.message }), true);
  }
}

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
  setConnected(Boolean(state.port));
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

applyLanguage(currentLanguage);
setConnected(false);
updateBrowserWarning();
