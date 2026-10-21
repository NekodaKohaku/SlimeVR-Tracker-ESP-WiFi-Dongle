Run `python3 test/host/test_receiver.py` from the repository root.

The script compiles the production receiver with minimal Arduino, UDP, storage,
and HID sink stubs. It checks DHCP endpoint reassignment, source port matching,
per-sensor acceleration, malformed sensor IDs, short packets, and session reset.
AddressSanitizer and UndefinedBehaviorSanitizer are enabled. This is not a radio,
USB integration, or ESP32 hardware test.
