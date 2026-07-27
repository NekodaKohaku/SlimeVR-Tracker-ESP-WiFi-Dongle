import os
import re
import secrets

Import("env")

SSID_PREFIX = "SlimeDongle"
SSID_SUFFIX_LEN = 4
PASSWORD_LEN = 10
CHARSET = "23456789ABCDEFGHJKMNPQRSTUVWXYZ"
IDENTITY_FILE = "device_identity.txt"

def _config_path():
    return os.path.join(env["PROJECT_DIR"], "src", "WifiDongleConfig.h")

def _identity_path():
    return os.path.join(env["PROJECT_DIR"], IDENTITY_FILE)

def _parse_config():
    if not os.path.isfile(_config_path()):
        return {}
    with open(_config_path(), encoding="utf-8") as f:
        text = f.read()

    def find(pattern, default="?"):
        match = re.search(pattern, text)
        return match.group(1) if match else default

    return {
        "generate": find(r"generateUniqueIdentity\s*=\s*(true|false)", "false"),
        "ssid": find(r'apSsid\s*=\s*"([^"]*)"'),
        "password": find(r'apPassword\s*=\s*"([^"]*)"'),
        "channel": find(r"apChannel\s*=\s*(\d+)"),
        "udp_port": find(r"udpPort\s*=\s*(\d+)"),
        "max_trackers": find(r"maxTrackers\s*=\s*(\d+)"),
    }

def _load_identity():
    if not os.path.isfile(_identity_path()):
        return None
    data = {}
    with open(_identity_path(), encoding="utf-8") as f:
        for line in f:
            if "=" in line and not line.lstrip().startswith("#"):
                key, value = line.split("=", 1)
                data[key.strip()] = value.strip()
    if data.get("ssid") and data.get("password"):
        return data
    return None

def _generate_identity():
    suffix = "".join(secrets.choice(CHARSET) for _ in range(SSID_SUFFIX_LEN))
    password = "".join(secrets.choice(CHARSET) for _ in range(PASSWORD_LEN))
    identity = {"ssid": f"{SSID_PREFIX}-{suffix}", "password": password}
    with open(_identity_path(), "w", encoding="utf-8") as f:
        f.write("# この dongle の SoftAP 識別です。\\n")
        f.write("# 新しい dongle を書き込む前に、このファイルを削除してください。\\n")
        f.write(f"ssid={identity['ssid']}\\n")
        f.write(f"password={identity['password']}\\n")
    return identity

def _main():
    config = _parse_config()
    identity = None
    fresh = False

    if config.get("generate") == "true":
        identity = _load_identity()
        if identity is None:
            identity = _generate_identity()
            fresh = True
        env.Append(CPPDEFINES=[
            ("DONGLE_AP_SSID", '\\\"%s\\\"' % identity["ssid"]),
            ("DONGLE_AP_PASSWORD", '\\\"%s\\\"' % identity["password"]),
        ])

    ssid = identity["ssid"] if identity else config.get("ssid", "?")
    password = identity["password"] if identity else config.get("password", "?")
    channel = config.get("channel", "?")
    channel = "auto (1/6/11)" if channel == "0" else channel

    print("")
    print("=" * 58)
    print("  SlimeVR WiFi Dongle - SoftAP settings (compile-time)")
    print("-" * 58)
    print(f"  SSID         : {ssid}")
    print(f"  Password     : {password}")
    print(f"  WiFi channel : {channel}")
    print(f"  UDP port     : {config.get('udp_port', '?')}")
    print(f"  Max trackers : {config.get('max_trackers', '?')}")
    if identity:
        print("-" * 58)
        if fresh:
            print("  New identity generated")
        else:
            print(f"  Reusing {IDENTITY_FILE}")
    print("=" * 58)
    print("")

_main()
