"use strict";

const USB_FILTERS = [{ usbVendorId: 0x1209, usbProductId: 0x7690 }];
const BAUD_RATE = 115200;
const RESPONSE_IDLE_MS = 180;
const RESPONSE_TIMEOUT_MS = 2200;
const encoder = new TextEncoder();
const decoder = new TextDecoder();

const state = {
  port: null,
  reader: null,
  writer: null,
  reading: false,
  closing: false,
  pending: null,
  commandQueue: Promise.resolve(),
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
  dialog: $("#confirm-dialog"),
  dialogTitle: $("#dialog-title"),
  dialogMessage: $("#dialog-message"),
  dialogConfirm: $("#dialog-confirm"),
  toast: $("#toast"),
};

let toastTimer = 0;

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
  ui.connectionLabel.textContent = connected ? "已連接" : "尚未連接";
  ui.connect.hidden = connected;
  ui.disconnect.hidden = !connected;
  setControlsEnabled(connected);
  if (!connected) {
    ui.restartBadge.hidden = true;
  }
}

function appendTerminal(text, prefix = "") {
  if (ui.terminal.textContent === "等待連接 Dongle…") ui.terminal.textContent = "";
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
  appendTerminal(text);
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
    if (!state.closing) showToast(`序列連線中斷：${error.message}`, true);
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
    appendTerminal("[Connected] SlimeVR WiFi Dongle\n");
    void readLoop();
    const info = await refreshInfo();
    if (!/^Product:\s*SlimeVR WiFi Dongle\s*$/mi.test(info)) {
      throw new Error("選取的裝置不是相容的 SlimeVR WiFi Dongle");
    }
    await refreshStatus();
    await refreshWifi();
    await refreshTrackers();
    setControlsEnabled(true);
    showToast("Dongle 已連接");
  } catch (error) {
    if (state.port) await closePort(false);
    const busy = /Failed to open|Access denied|NetworkError/i.test(error.message);
    showToast(busy ? "無法開啟序列埠，請關閉其他序列工具後再試。" : `連線失敗：${error.message}`, true);
  }
}

async function closePort(showMessage = true) {
  if (!state.port) return;
  state.closing = true;
  rejectPending(new Error("Serial port closed"));
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
    setConnected(false);
    if (showMessage) showToast("已中斷 Dongle 連線");
  }
}

async function sendCommandNow(command) {
  if (!state.writer || !state.port) throw new Error("Dongle 尚未連接");
  if (state.pending) throw new Error("上一個指令仍在處理");
  appendTerminal(`\n> ${command}\n`);
  const response = new Promise((resolve, reject) => {
    state.pending = {
      text: "",
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

function sendCommand(command) {
  const operation = state.commandQueue.then(() => sendCommandNow(command));
  state.commandQueue = operation.catch(() => {});
  return operation;
}

async function sendWithoutReply(command) {
  if (!state.writer || !state.port) throw new Error("Dongle 尚未連接");
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
  $("#status-hid").textContent = status["hid ready"] === "yes" ? "Ready" : (status["hid ready"] || "—");
  $("#status-channel").textContent = status.channel || "—";
  $("#status-stations").textContent = status["connected stations"] || "—";
  $("#status-temperature").textContent = status["chip temperature"] || "—";
  const dropped = status["dropped packets"] ?? "—";
  const failed = status["failed hid reports"] ?? "—";
  $("#status-packets").textContent = `Dropped ${dropped} / HID ${failed}`;
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

async function refreshInfo() {
  const response = await sendCommand("info");
  updateInfo(response);
  return response;
}

async function refreshStatus() {
  const response = await sendCommand("status");
  updateStatus(response);
  return response;
}

async function refreshWifi() {
  const response = await sendCommand("wifi show");
  updateWifi(response);
  return response;
}

async function refreshTrackers() {
  const response = await sendCommand("trackers list");
  ui.trackerList.textContent = response.replace(/^Stored trackers:\s*/i, "").trim() || "No stored trackers.";
  return response;
}

async function refreshAll() {
  try {
    await refreshInfo();
    await refreshStatus();
    await refreshWifi();
    await refreshTrackers();
  } catch (error) {
    showToast(`讀取失敗：${error.message}`, true);
    throw error;
  }
}

function byteLength(value) {
  return encoder.encode(value).length;
}

function quoteCliValue(value) {
  if (/\r|\n|\0/.test(value)) throw new Error("內容不能包含換行或 NUL");
  if (!value.includes('"')) return `"${value}"`;
  if (!value.includes("'")) return `'${value}'`;
  if (!/^['"]|['"]$/.test(value)) return value;
  throw new Error("內容不能同時以引號開頭或結尾並包含兩種引號");
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
    ui.wifiError.textContent = "SSID 必須是 1–32 bytes。";
    return;
  }
  if (passwordBytes < 8 || passwordBytes > 63) {
    ui.wifiError.textContent = "密碼必須是 8–63 bytes。";
    return;
  }
  try {
    const commands = [
      `wifi set ssid ${quoteCliValue(ssid)}`,
      `wifi set password ${quoteCliValue(password)}`,
      `wifi set channel ${channel}`,
    ];
    for (const command of commands) {
      const response = await sendCommand(command);
      if (!/^Saved\./m.test(response)) throw new Error(response || "Dongle 沒有回覆");
    }
    ui.restartBadge.hidden = false;
    showToast("WiFi 設定已儲存，重新啟動後生效");
  } catch (error) {
    ui.wifiError.textContent = `儲存失敗：${error.message}`;
  }
}

function confirmAction(title, message, confirmLabel = "確認") {
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
    showToast("指令已送出，Dongle 正在重新連線");
  } catch (error) {
    showToast(error.message, true);
  }
}

ui.connect.addEventListener("click", connectPort);
ui.disconnect.addEventListener("click", () => closePort());
$("#refresh-button").addEventListener("click", refreshAll);
$("#wifi-read").addEventListener("click", refreshWifi);
$("#tracker-read").addEventListener("click", refreshTrackers);
ui.wifiForm.addEventListener("submit", saveWifi);

$("#password-toggle").addEventListener("click", (event) => {
  const visible = ui.wifiPassword.type === "text";
  ui.wifiPassword.type = visible ? "password" : "text";
  event.currentTarget.textContent = visible ? "顯示" : "隱藏";
});

$$('[data-command]').forEach((button) => {
  button.addEventListener("click", async () => {
    try {
      const response = await sendCommand(button.dataset.command);
      if (button.dataset.command === "info") updateInfo(response);
      if (button.dataset.command === "status") updateStatus(response);
      showToast(response.split(/\r?\n/)[0] || "指令完成");
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
    showToast("Dongle 正在重新啟動");
  } catch (error) {
    showToast(error.message, true);
  }
});
$("#bootloader-button").addEventListener("click", () => guardedAction("進入 Bootloader？", "Dongle 將中斷目前的 HID 與序列連線，並以 ESP32 ROM Download Mode 重新出現。", "bootloader", "進入 Bootloader"));
$("#tracker-clear").addEventListener("click", () => guardedAction("清除所有 Tracker？", "所有已儲存的 Tracker 對應會被刪除，Dongle 接著會重新啟動。", "trackers clear", "清除並重啟"));
$("#wifi-reset").addEventListener("click", async () => {
  if (!(await confirmAction("恢復預設 WiFi？", "已儲存的 SSID、密碼與頻道會被清除；需要重新啟動才會套用。", "恢復預設"))) return;
  try {
    const response = await sendCommand("wifi reset");
    if (!response.startsWith("Default WiFi")) throw new Error(response);
    await refreshWifi();
    ui.restartBadge.hidden = false;
    showToast("已恢復預設 WiFi，重新啟動後生效");
  } catch (error) {
    showToast(`操作失敗：${error.message}`, true);
  }
});

if ("serial" in navigator) {
  navigator.serial.addEventListener("disconnect", (event) => {
    if (state.port && (event.port === state.port || event.target === state.port) && !state.closing) {
      void closePort(false);
    }
  });
} else {
  ui.connect.disabled = true;
  ui.warning.hidden = false;
  ui.warning.textContent = "此瀏覽器不支援 Web Serial。請改用桌面版 Chrome 或 Edge。";
}

if (!window.isSecureContext) {
  ui.connect.disabled = true;
  ui.warning.hidden = false;
  ui.warning.textContent = "Web Serial 需要 HTTPS 安全連線。請從 GitHub Pages 網址開啟此頁。";
}

setConnected(false);
