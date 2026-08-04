#pragma once

#include <cstddef>
#include <cstdint>

namespace WifiDongleConfig {

// info コマンドで表示するユーザー向けファームウェアバージョンです。
static constexpr const char *firmwareVersion = "v1.1.4";

// ===== SoftAP =====
// デフォルト SSID の基本名です。MAC サフィックス有効時は
// SlimeDongle-5598C0 のように表示されます。
static constexpr char apSsid[] = "SlimeDongle";

// true: デフォルト SSID に WiFi MAC の末尾 3 bytes を追加します。
// wifi set でカスタム SSID を保存した場合、サフィックスは追加されません。
static constexpr bool appendMacSuffixToDefaultSsid = false;

// ランダムパスワード無効時、または NVS への保存に失敗した場合に
// 使用するフォールバックパスワードです。
static constexpr char apPassword[] = "slimedongle12345";

// true: 初回起動時にデバイス固有のパスワードを生成し、独立した NVS に
// 保存します。wifi reset では変更されず、NVS 全消去後に再生成されます。
static constexpr bool generateRandomDefaultPassword = false;
static constexpr uint8_t randomDefaultPasswordLength = 12;

// 0: 起動時に 1 / 6 / 11 から自動選択します。
static constexpr uint8_t apChannel = 0;

// 同時接続できる Tracker 数です。上限は 10 です。
static constexpr uint8_t maxTrackers = 10;

// true: SSID を非表示にします。
static constexpr bool apHidden = false;

// ===== USB =====
// チップの MAC から一意な USB シリアル番号を生成します。
// 複数の Dongle を同時に使用するため、true を推奨します。
static constexpr bool autoUniqueUsbSerial = true;

// ===== 通信 =====
// Tracker が使用する UDP ポートと同じ値にしてください。
static constexpr uint16_t udpPort = 6969;

// SoftAP の Beacon 間隔です。通常は変更不要です。
static constexpr uint16_t beaconIntervalMs = 100;

// DTIM 周期です。通常は変更不要です。
static constexpr uint8_t dtimPeriod = 3;

// 公式 Tracker へ定期的に Heartbeat を送信します。
static constexpr bool officialHeartbeatEnabled = true;
static constexpr uint32_t officialHeartbeatIntervalMs = 2400;

// この時間データを受信しない場合、Tracker をオフラインとして扱います。
static constexpr uint32_t officialTrackerTimeoutMs = 6000;

} // namespace WifiDongleConfig
