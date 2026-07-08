# SlimeVR Tracker ESP WiFi Dongle

SlimeVR ESP tracker WiFi dongle firmware.

This project turns an ESP32-S3 dongle into a dedicated WiFi receiver for SlimeVR
ESP trackers. The dongle creates its own SoftAP and forwards tracker data to the
PC through USB HID, so trackers can connect to the dongle directly instead of
using a home WiFi router.

Default WiFi settings:

|Setting|Value|
|---|---|
|SSID|`SlimeDongle`|
|Password|`slimedongle12345`|
|UDP port|`6969`|
|Recommended max trackers|`10`|

## Modes

### Official-compatible mode

This is the main mode.

Stock SlimeVR ESP tracker firmware can connect to the dongle without tracker
firmware modifications. The dongle emulates the SlimeVR UDP server protocol and
then converts the received tracker data into HID packets for SlimeVR Server.

Enable this mode with:

```ini
-DUSE_OFFICIAL_PROXY
```

### Custom WiFi tracker mode

This mode is for custom tracker firmware that sends compact UDP packets directly
to the dongle.

Disable `USE_OFFICIAL_PROXY` in `platformio.ini` to build this mode. Custom
trackers need to be paired with the dongle before use.

## Configuration

User-facing settings are in:

```text
src/WifiDongleConfig.h
```

Common settings include:

- SoftAP SSID and password
- WiFi channel
- DTIM period
- official-mode heartbeat interval
- tracker timeout
- maximum tracker count

The default maximum tracker count is set to `10` for ESP32 SoftAP stability.

## Building and Flashing

The main target is:

```text
slime_dongle_s3
```

Build:

```sh
pio run -e slime_dongle_s3
```

Upload:

```sh
pio run -e slime_dongle_s3 -t upload
```

After building, PlatformIO outputs firmware files under:

```text
.pio/build/slime_dongle_s3/
```

The combined factory image is:

```text
.pio/build/slime_dongle_s3/firmware.factory.bin
```

## Usage

1. Flash the dongle firmware.
2. Plug the dongle into the PC over USB.
3. Configure trackers to connect to the dongle WiFi:

```text
SSID: SlimeDongle
Password: slimedongle12345
```

4. Start SlimeVR Server.
5. Trackers should appear through the dongle as HID tracker data.

For official-compatible mode, tracker firmware pairing is not required. For
custom WiFi tracker mode, hold the dongle button to enter pairing mode, then
pair the custom tracker firmware.

## Button Actions

|Action|Result|
|---|---|
|Long press|Enter or exit custom tracker pairing mode|
|Press 5 times|Reset saved custom tracker pairing data|

The 5-press reset is mainly useful for custom WiFi tracker mode. It is usually
not needed for official-compatible mode.

## Troubleshooting

- If a tracker cannot connect, unplug and replug the dongle first.
- If SlimeVR Server still shows a tracker as disconnected after tracker reboot,
  make sure you are using the latest firmware; the dongle sends reconnect status
  packets and refreshes tracker registration when a tracker comes back online.
- ESP32 SoftAP is most stable with up to 10 connected trackers.
- In official-compatible mode, heartbeat should normally stay enabled.

## License

This project is based on SlimeVR firmware work and is distributed under the
included MIT / Apache-2.0 license files.
