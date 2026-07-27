#pragma once

#include <cstddef>
#include <cstdint>

namespace WifiDongleConfig {

// ===== 個体別 WiFi 設定 =====
// true: ビルド時にランダムな SSID とパスワードを生成します。
// 生成値は device_identity.txt に保存され、以後のビルドでも再利用されます。
// 別の Dongle を書き込む前に、このファイルを削除してください。
// false: 下記の apSsid と apPassword を使用し、ファイルは生成しません。
static constexpr bool generateUniqueIdentity = false;

// ===== SoftAP =====
// generateUniqueIdentity が true の場合は、ビルド時の生成値が優先されます。
#ifdef DONGLE_AP_SSID
static constexpr const char *apSsid = DONGLE_AP_SSID;
#else
static constexpr const char *apSsid = "SlimeDongle";
#endif

// 8 文字以上のパスワードを設定してください。
#ifdef DONGLE_AP_PASSWORD
static constexpr const char *apPassword = DONGLE_AP_PASSWORD;
#else
static constexpr const char *apPassword = "slimedongle12345";
#endif

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

// ===== 公式 Tracker =====
// 公式 Tracker へ定期的に Heartbeat を送信します。
static constexpr bool officialHeartbeatEnabled = true;
static constexpr uint32_t officialHeartbeatIntervalMs = 2400;

// この時間データを受信しない場合、Tracker をオフラインとして扱います。
static constexpr uint32_t officialTrackerTimeoutMs = 6000;

// ===== カスタム WiFi Tracker =====
// 実験用 Heartbeat です。通常は false のまま使用してください。
static constexpr bool customExperimentHeartbeat = false;
static constexpr uint32_t customExperimentHeartbeatIntervalMs = 1000;

} // namespace WifiDongleConfig
