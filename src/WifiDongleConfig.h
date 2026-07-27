#pragma once

#include <cstddef>
#include <cstdint>

namespace WifiDongleConfig {

// ===== ランダム識別(シール印刷用) =====
// true にすると、ビルド時に各 dongle 一意の SSID とランダムパスワードを自動生成し、
// build log に表示します(シールに印刷して本体に貼るのがおすすめです)。
// 生成された値は device_identity.txt に保存され、同じ dongle への再書き込みでは
// 変わりません。次の「新しい」dongle を書き込む前に device_identity.txt を
// 削除してください(新しい一組が生成されます)。
// false の場合は下の apSsid / apPassword をそのまま使い、ファイルも生成しません。
static constexpr bool generateUniqueIdentity = false;

// ===== WiFi SoftAP 設定 =====
// 注: generateUniqueIdentity が true の場合、下の 2 つの値は使われません
// (DONGLE_AP_SSID / DONGLE_AP_PASSWORD がビルド時に注入されます)。
// Dongle が作成する WiFi AP の名前です。
#ifdef DONGLE_AP_SSID
static constexpr const char *apSsid = DONGLE_AP_SSID;
#else
static constexpr const char *apSsid = "SlimeDongle";
#endif

// WiFi AP のパスワードです。8文字以上が必要です。
#ifdef DONGLE_AP_PASSWORD
static constexpr const char *apPassword = DONGLE_AP_PASSWORD;
#else
static constexpr const char *apPassword = "slimedongle12345";
#endif

// WiFi チャンネルです。
// 0 にすると、起動時に 1/6/11 から空いているチャンネルを自動で選びます。
static constexpr uint8_t apChannel = 0;

// 同時接続できる tracker 数です。最大 10 です。
static constexpr uint8_t maxTrackers = 10;

// AP を隠す場合は true にします。通常は false のままで大丈夫です。
static constexpr bool apHidden = false;

// ===== 複数 dongle 同時利用の設定 =====
// 注: 上記の「ビルド時に各 dongle 一組の SSID/パスワードを生成する」仕組み
// (print_wifi_config.py)を使う場合、各台はもともと一意になるため、
// autoUniqueSsidSuffix / autoUniquePassword は false のままで大丈夫です。
// (autoUniqueUsbSerial は WiFi と無関係なので true のままを推奨します。)
// dongle を複数台同時に使う場合、各台に「一意な USB シリアル」と「一意な SSID」が
// 必要です。そうしないと server が同じ dongle と誤認して統合してしまい(HID デバイスの
// 統合)、tracker も別の dongle につながってしまうことがあります。
//
// autoUniqueUsbSerial: 既定 true。起動時にチップの MAC から一意な USB シリアルを
//   自動生成します。USB シリアルは tracker の設定には使わないので、有効にしても
//   既存の設定には影響せず、最も深刻な「複数台が server で統合される」問題を防げます。
//   → true のままを推奨します。
static constexpr bool autoUniqueUsbSerial = true;

// autoUniqueSsidSuffix: 既定 false。true にすると apSsid の後ろに「-XXXX」(MAC 下位)を
//   自動で付けます。例: "SlimeDongle-A1B2"。SSID が変わるので、tracker 側もその新しい
//   SSID に書き換える必要があります。
//   1 台のみの場合は false のままで大丈夫です(今の apSsid をそのまま使い、既存のペア
//   リングに影響しません)。複数台のときだけ有効にしてください。起動時に「実際に使用する
//   完全な SSID」をシリアルに出力するので、それを見て各 tracker を設定できます。
//   → 一意性と「自分の SSID が分かること」を両立します。
static constexpr bool autoUniqueSsidSuffix = false;

// autoUniquePassword: 既定 false。true にするとパスワードが「apPassword + MAC 下位」に
//   なり、各台で一意になります。autoUniqueSsidSuffix と組み合わせれば「各台で SSID も
//   パスワードも異なり、ラベルに印刷する」製品向けの構成になります。
//   tracker 側も新しいパスワードへの変更が必要です(起動時にシリアルへ出力するので、
//   それを写せば OK です)。
//   ⚠ セキュリティ注意: SoftAP の MAC(BSSID)は WiFi スキャンで見えてしまうため、
//   「MAC から生成しただけ」のパスワードはアルゴリズムを知っている人には破られます。
//   用途は「各台の初期パスワードを別々にする」(全台同一を避ける)ことで、強固な保護
//   ではありません。本当にランダムにしたい場合は、初回起動時に乱数生成して NVS に保存
//   する方式を推奨します(必要ならまた依頼してください)。
static constexpr bool autoUniquePassword = false;

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
// 3000ms は heartbeat(2400) + DTIM(約300) + WiFi のジッタに対して短すぎて、
// 一瞬の途切れで誤って「オフライン」と判定し、status を送ってしまう原因になっていました。
// heartbeat 数回分をカバーできる 6000ms に緩和します。
static constexpr uint32_t officialTrackerTimeoutMs = 6000;

} // namespace WifiDongleConfig
