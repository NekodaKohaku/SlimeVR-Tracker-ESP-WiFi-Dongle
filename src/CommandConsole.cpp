#include "CommandConsole.h"

#include "WifiDongleConfig.h"
#include "configuration.h"
#include "packetHandling.h"
#include "pins_arduino.h"

#include <Preferences.h>
#include <WiFi.h>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <driver/temperature_sensor.h>
#include <esp32-hal-tinyusb.h>
#include <esp_timer.h>

namespace {

float readChipTemperature() {
    static temperature_sensor_handle_t sensor = nullptr;
    static bool attempted = false;
    if (!attempted) {
        attempted = true;
        temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
        if (temperature_sensor_install(&config, &sensor) != ESP_OK
            || temperature_sensor_enable(sensor) != ESP_OK) {
            if (sensor != nullptr) {
                temperature_sensor_uninstall(sensor);
                sensor = nullptr;
            }
        }
    }

    float temperature = NAN;
    if (sensor != nullptr) {
        temperature_sensor_get_celsius(sensor, &temperature);
    }
    return temperature;
}

} // namespace

CommandConsole::CommandConsole(Stream &port, HIDDevice &newHidDevice)
    : port(port), hidDevice(newHidDevice) {}

void CommandConsole::begin(const char *newUsbSerial, const char *newActiveSsid,
                           const char *newActivePassword, uint8_t newActiveChannel) {
    usbSerial = newUsbSerial;
    activeSsid = newActiveSsid;
    activePassword = newActivePassword;
    activeChannel = newActiveChannel;
}

void CommandConsole::update() {
    size_t processed = 0;
    while (port.available() > 0 && processed++ < 64) {
        int incoming = port.read();
        if (incoming < 0) break;
        uint8_t byte = static_cast<uint8_t>(incoming);
        char character = static_cast<char>(byte);
        if (character == '\r' || character == '\n') {
            if (discardingLine) {
                discardingLine = false;
                lineLength = 0;
            } else if (lineLength > 0) {
                line[lineLength] = '\0';
                execute(trim(line));
                lineLength = 0;
            }
            continue;
        }
        if (discardingLine) continue;
        if ((character == '\b' || character == 0x7f) && lineLength > 0) {
            lineLength--;
            continue;
        }
        if (byte >= 0x20 && byte != 0x7f) {
            if (lineLength + 1 < lineCapacity) {
                line[lineLength++] = character;
            } else {
                lineLength = 0;
                discardingLine = true;
                port.println("Error: command is too long.");
            }
        }
    }
}

char *CommandConsole::trim(char *text) {
    while (*text == ' ' || *text == '\t') text++;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';
    return text;
}

char *CommandConsole::valueWithoutOptionalQuotes(char *text) {
    text = trim(text);
    size_t length = strlen(text);
    bool startsDouble = length > 0 && text[0] == '"';
    bool startsSingle = length > 0 && text[0] == '\'';
    bool endsDouble = length > 0 && text[length - 1] == '"';
    bool endsSingle = length > 0 && text[length - 1] == '\'';
    if (startsDouble != endsDouble || startsSingle != endsSingle
        || (startsDouble && endsSingle) || (startsSingle && endsDouble)) {
        return nullptr;
    }
    if (length >= 2 && ((startsDouble && endsDouble)
                        || (startsSingle && endsSingle))) {
        text[length - 1] = '\0';
        text++;
    }
    return text;
}

void CommandConsole::execute(char *command) {
    if (*command == '\0') return;

    char *argument = strchr(command, ' ');
    if (argument != nullptr) {
        *argument++ = '\0';
        argument = trim(argument);
    }
    for (char *p = command; *p; p++) {
        *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
    }

    if (strcmp(command, "help") == 0) {
        printHelp();
    } else if (strcmp(command, "info") == 0) {
        printInfo();
    } else if (strcmp(command, "status") == 0) {
        printStatus();
    } else if (strcmp(command, "uptime") == 0) {
        uint64_t seconds = static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;
        port.printf("Uptime: %llu days %02llu:%02llu:%02llu\n",
                    seconds / 86400ULL, (seconds / 3600ULL) % 24ULL,
                    (seconds / 60ULL) % 60ULL, seconds % 60ULL);
    } else if (strcmp(command, "reboot") == 0) {
        ESP.restart();
    } else if (strcmp(command, "bootloader") == 0) {
        usb_persist_restart(RESTART_BOOTLOADER);
    } else if (strcmp(command, "meow") == 0) {
        printMeow();
    } else if (strcmp(command, "trackers") == 0) {
        if (argument != nullptr) {
            for (char *p = argument; *p; p++) {
                *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
            }
        }
        if (argument != nullptr && strcmp(argument, "list") == 0) {
            port.println("Stored trackers:");
            Configuration::getInstance().printTrackers(port);
        } else if (argument != nullptr && strcmp(argument, "clear") == 0) {
            Configuration::getInstance().resetTrackers();
            ESP.restart();
        } else {
            port.println("Usage: trackers <list|clear>");
        }
    } else if (strcmp(command, "wifi") == 0) {
        char subcommand[8] = {0};
        if (argument != nullptr) {
            size_t length = strcspn(argument, " ");
            length = length < sizeof(subcommand) - 1 ? length : sizeof(subcommand) - 1;
            memcpy(subcommand, argument, length);
            for (char *p = subcommand; *p; p++) {
                *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
            }
        }
        if (strcmp(subcommand, "show") == 0 && argument[4] == '\0') {
            printWifi();
        } else if (strcmp(subcommand, "reset") == 0 && argument[5] == '\0') {
            if (Configuration::getInstance().resetWifi()) {
                port.println("Default WiFi settings restored. Run 'reboot' to apply.");
            } else {
                port.println("Error: could not clear saved WiFi settings.");
            }
        } else if (strcmp(subcommand, "set") == 0 && argument[3] == ' ') {
            char *field = trim(argument + 4);
            char *value = strchr(field, ' ');
            if (value == nullptr) {
                port.println("Usage: wifi set <ssid|password|channel> <value>");
            } else {
                *value++ = '\0';
                for (char *p = field; *p; p++) {
                    *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
                }
                char *cleanValue = valueWithoutOptionalQuotes(value);
                if (cleanValue == nullptr) {
                    port.println("Error: quotes must be paired.");
                } else {
                    setWifi(field, cleanValue);
                }
            }
        } else {
            port.println("Usage: wifi <show|set|reset>");
        }
    } else {
        port.println("Unknown command. Type 'help'.");
    }
}

void CommandConsole::printHelp() {
    port.println("\nGeneral:");
    port.println("  help                         Display this help text");
    port.println("  info                         Get device information");
    port.println("  status                       Show dongle status");
    port.println("  uptime                       Show device uptime");
    port.println("  reboot                       Restart the dongle");
    port.println("  bootloader                   Enter ESP32 ROM download mode");
    port.println("  meow                         Meow!");
    port.println("\nTrackers:");
    port.println("  trackers list                List stored trackers");
    port.println("  trackers clear               Clear tracker mappings and restart");
    port.println("\nWiFi:");
    port.println("  wifi show                    Show current WiFi settings");
    port.println("  wifi set ssid <name>         Save a new SoftAP name");
    port.println("  wifi set password <password> Save a new SoftAP password");
    port.println("  wifi set channel <auto|1-13> Save a new WiFi channel");
    port.println("  wifi reset                   Restore this device's default WiFi settings");
    port.println("\nWiFi changes take effect after 'reboot'.");
}

void CommandConsole::printInfo() {
    port.printf("Product: %s\n", USB_PRODUCT);
    port.printf("Firmware: %s\n", WifiDongleConfig::firmwareVersion);
    port.printf("Build: %s %s\n", __DATE__, __TIME__);
    port.printf("USB serial: %s\n", usbSerial != nullptr ? usbSerial : USB_SERIAL);
    port.printf("Chip: %s revision %u, %u cores\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
    port.printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
    port.printf("Flash size: %u MB\n", ESP.getFlashChipSize() / (1024U * 1024U));
}

void CommandConsole::printStatus() {
    float temperature = readChipTemperature();
    uint64_t seconds = static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;
    PacketHandling::Stats packetStats = PacketHandling::getInstance().getStats();
    port.printf("Uptime: %llu days %02llu:%02llu:%02llu\n",
                seconds / 86400ULL, (seconds / 3600ULL) % 24ULL,
                (seconds / 60ULL) % 60ULL, seconds % 60ULL);
    port.printf("HID ready: %s\n", hidDevice.ready() ? "yes" : "no");
    port.printf("SoftAP: %s\n", activeSsid != nullptr ? activeSsid : "unknown");
    port.printf("Channel: %u\n", WiFi.channel());
    port.printf("AP address: %s\n", WiFi.softAPIP().toString().c_str());
    port.printf("Connected stations: %u\n", WiFi.softAPgetStationNum());
    port.printf("Stored trackers: %u\n", Configuration::getInstance().getSavedTrackerCount());
    port.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    port.printf("Dropped packets: %u\n", packetStats.droppedPackets);
    port.printf("Failed HID reports: %u\n", packetStats.failedHidReports);
    if (std::isnan(temperature)) {
        port.println("Chip temperature: unavailable");
    } else {
        port.printf("Chip temperature: %.1f C (internal sensor)\n", temperature);
    }
    port.printf("WiFi restart required: %s\n", wifiRestartRequired() ? "yes" : "no");
}

bool CommandConsole::wifiRestartRequired() const {
    Configuration &configuration = Configuration::getInstance();
    return activeSsid == nullptr || activePassword == nullptr
           || strcmp(configuration.getWifiSsid(), activeSsid) != 0
           || strcmp(configuration.getWifiPassword(), activePassword) != 0
           || configuration.getWifiChannel() != activeChannel;
}

void CommandConsole::printWifi() {
    Configuration &configuration = Configuration::getInstance();
    port.println("WiFi configuration:");
    port.printf("  SSID     : %s\n", configuration.getWifiSsid());
    port.printf("  Password : %s\n", configuration.getWifiPassword());
    if (configuration.getWifiChannel() == 0) {
        port.println("  Channel  : auto (1/6/11)");
    } else {
        port.printf("  Channel  : %u\n", configuration.getWifiChannel());
    }
    port.printf("  Source   : %s\n", configuration.hasWifiOverride() ? "saved" : "device default");
    port.printf("  Reboot required: %s\n", wifiRestartRequired() ? "yes" : "no");
}

void CommandConsole::setWifi(const char *field, char *value) {
    Configuration &configuration = Configuration::getInstance();
    bool saved = false;
    if (strcmp(field, "ssid") == 0) {
        size_t length = strlen(value);
        if (length < 1 || length > 32) {
            port.println("Error: SSID must be 1-32 bytes.");
            return;
        }
        saved = configuration.setWifiSsid(value);
    } else if (strcmp(field, "password") == 0) {
        size_t length = strlen(value);
        if (length < 8 || length > 63) {
            port.println("Error: password must be 8-63 bytes.");
            return;
        }
        saved = configuration.setWifiPassword(value);
    } else if (strcmp(field, "channel") == 0) {
        bool valid = false;
        if (strcmp(value, "auto") == 0) {
            valid = true;
            saved = configuration.setWifiChannel(0);
        } else {
            char *end = nullptr;
            long channel = strtol(value, &end, 10);
            if (end != value && *end == '\0' && channel >= 1 && channel <= 13) {
                valid = true;
                saved = configuration.setWifiChannel(static_cast<uint8_t>(channel));
            }
        }
        if (!valid) {
            port.println("Error: channel must be auto or 1-13.");
            return;
        }
    } else {
        port.println("Error: field must be ssid, password or channel.");
        return;
    }

    port.println(saved ? "Saved. Run 'reboot' to apply."
                       : "Error: could not save settings to NVS.");
}

void CommandConsole::printMeow() {
    static const char *sounds[] = {
        "Mew.", "Meww!", "Meow :3", "Mrrrp~", "Mrrrow?", "Purr...", "Nya :3c"
    };
    port.println(sounds[(millis() / 37U) % (sizeof(sounds) / sizeof(sounds[0]))]);
}
