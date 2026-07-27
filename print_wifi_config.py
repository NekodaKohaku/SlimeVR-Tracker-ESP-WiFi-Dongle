# PlatformIO pre-build script:
#
# 1) 每台 dongle 一組「唯一 SSID + 隨機密碼」(路由器貼紙式):
#    第一次編譯時自動產生,存進 device_identity.txt 並注入韌體
#    (以 DONGLE_AP_SSID / DONGLE_AP_PASSWORD 蓋過 WifiDongleConfig.h 的預設值)。
#    之後每次編譯沿用同一組 —— 同一台 dongle 更新韌體時 SSID/密碼不會變。
#    要燒「下一台新的 dongle」:先刪掉 device_identity.txt 再編譯,
#    就會產生新的一組並顯示在 build log 上(抄下來印成貼紙)。
#
# 2) 在「編譯的當下」把實際會生效的 SoftAP 設定印到 build log 上,
#    讓你不必打開序列埠監視器就能知道要填進追蹤器的 SSID / 密碼。
#
# 註冊方式見 platformio.ini 的 [env] 段:
#   extra_scripts = pre:print_wifi_config.py

import os
import re
import secrets

Import("env")  # noqa: F821  (PlatformIO 注入)

# ===== 設定 =====
# True:啟用上述「每台一組」機制(建議)。
# False:不產生也不注入,單純使用 WifiDongleConfig.h 裡寫死的值。
GENERATE_PER_DEVICE_IDENTITY = True

SSID_PREFIX = "SlimeDongle"
SSID_SUFFIX_LEN = 4
PASSWORD_LEN = 10
# 貼紙好抄:排除 0/O、1/I/L 這些容易看錯的字元
CHARSET = "23456789ABCDEFGHJKMNPQRSTUVWXYZ"

IDENTITY_FILE = "device_identity.txt"


def _identity_path():
    return os.path.join(env["PROJECT_DIR"], IDENTITY_FILE)


def _load_identity():
    if not os.path.isfile(_identity_path()):
        return None
    data = {}
    with open(_identity_path(), encoding="utf-8") as f:
        for line in f:
            if "=" in line:
                k, v = line.split("=", 1)
                data[k.strip()] = v.strip()
    if data.get("ssid") and data.get("password"):
        return data
    return None


def _generate_identity():
    suffix = "".join(secrets.choice(CHARSET) for _ in range(SSID_SUFFIX_LEN))
    password = "".join(secrets.choice(CHARSET) for _ in range(PASSWORD_LEN))
    ident = {"ssid": f"{SSID_PREFIX}-{suffix}", "password": password}
    with open(_identity_path(), "w", encoding="utf-8") as f:
        f.write("# 這台 dongle 的 SoftAP 識別。同一台重複燒錄會沿用;\n")
        f.write("# 要燒下一台新的 dongle,請先刪除本檔再編譯(會產生新的一組)。\n")
        f.write(f"ssid={ident['ssid']}\n")
        f.write(f"password={ident['password']}\n")
    return ident


def _parse_config_defaults():
    path = os.path.join(env["PROJECT_DIR"], "src", "WifiDongleConfig.h")
    if not os.path.isfile(path):
        return {}
    with open(path, encoding="utf-8") as f:
        text = f.read()

    def find(pattern, default="?"):
        m = re.search(pattern, text)
        return m.group(1) if m else default

    return {
        "ssid": find(r'apSsid\s*=\s*"([^"]*)"'),
        "password": find(r'apPassword\s*=\s*"([^"]*)"'),
        "channel": find(r"apChannel\s*=\s*(\d+)"),
        "udp_port": find(r"udpPort\s*=\s*(\d+)"),
        "max_trackers": find(r"maxTrackers\s*=\s*(\d+)"),
    }


def _main():
    cfg = _parse_config_defaults()
    identity = None
    fresh = False

    if GENERATE_PER_DEVICE_IDENTITY:
        identity = _load_identity()
        if identity is None:
            identity = _generate_identity()
            fresh = True
        # 注入韌體:蓋過 WifiDongleConfig.h 的 apSsid / apPassword 預設值
        env.Append(CPPDEFINES=[
            ("DONGLE_AP_SSID", '\\"%s\\"' % identity["ssid"]),
            ("DONGLE_AP_PASSWORD", '\\"%s\\"' % identity["password"]),
        ])

    ssid = identity["ssid"] if identity else cfg.get("ssid", "?")
    password = identity["password"] if identity else cfg.get("password", "?")
    channel = cfg.get("channel", "?")
    channel = "auto (1/6/11)" if channel == "0" else channel

    print("")
    print("=" * 58)
    print("  SlimeVR WiFi Dongle - SoftAP settings (compile-time)")
    print("-" * 58)
    print(f"  SSID         : {ssid}")
    print(f"  Password     : {password}")
    print(f"  WiFi channel : {channel}")
    print(f"  UDP port     : {cfg.get('udp_port', '?')}")
    print(f"  Max trackers : {cfg.get('max_trackers', '?')}")
    if identity:
        print("-" * 58)
        if fresh:
            print("  ★ 新產生的識別!請把上面的 SSID / Password 抄下來(印貼紙)")
        else:
            print(f"  (沿用 {IDENTITY_FILE};要燒下一台新 dongle 請先刪除它)")
    print("=" * 58)
    print("")


_main()
