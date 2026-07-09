#pragma once

#include <cstddef>
#include <cstdint>

namespace WifiDongleConfig {

// ===== WiFi SoftAP 設定 =====
// Dongle が作成する WiFi AP の名前です。通常は変更しなくて大丈夫です。
static constexpr const char *apSsid = "SlimeDongle";

// WiFi AP のパスワードです。8文字以上が必要です。
static constexpr const char *apPassword = "slimedongle12345";

// WiFi チャンネルです。
// 0 にすると、起動時に 1/6/11 から空いているチャンネルを自動で選びます。
static constexpr uint8_t apChannel = 0;

// 同時接続できる tracker 数です。最大 10 です。
static constexpr uint8_t maxTrackers = 10;

// AP を隠す場合は true にします。通常は false のままで大丈夫です。
static constexpr bool apHidden = false;

// UDP ポートです。tracker 側と同じ値にしてください。
static constexpr uint16_t udpPort = 6969;

// ===== SoftAP 省電力設定 =====
// beacon 間隔です。通常は 100ms のまま変更しないでください。
static constexpr uint16_t beaconIntervalMs = 100;

// DTIM 周期です。100ms x 3 = 約 300ms ごとに下り通信を確認します。
// 大きくすると省電力になる場合がありますが、公式 tracker では効果が小さいことがあります。
static constexpr uint8_t dtimPeriod = 3;

// ===== 公式 tracker 互換モード =====
// 公式 tracker は server からの heartbeat が長時間ないと切断扱いになります。
// 2400ms は省電力と安定性のバランスを見た現在の推奨値です。
static constexpr bool officialHeartbeatEnabled = true;
static constexpr uint32_t officialHeartbeatIntervalMs = 2400;

// Dongle 側で tracker をオフライン扱いにするまでの時間です。
static constexpr uint32_t officialTrackerTimeoutMs = 3000;

// ===== 客製 WiFi tracker モード =====
// 実験用: 客製 tracker へ下り heartbeat を送って消費電流を比較するための設定です。
// 通常運用では false のままにしてください。
static constexpr bool customExperimentHeartbeat = false;
static constexpr uint32_t customExperimentHeartbeatIntervalMs = 1000;

} // namespace WifiDongleConfig
