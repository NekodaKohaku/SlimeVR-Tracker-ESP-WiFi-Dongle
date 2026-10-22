Run `python3 test/host/test_receiver.py` from the repository root.

The script compiles the production receiver with minimal Arduino, UDP, storage,
and HID sink stubs. It checks DHCP endpoint reassignment, source port matching,
per-sensor acceleration, malformed sensor IDs, short packets, and session reset.
AddressSanitizer and UndefinedBehaviorSanitizer are enabled. This is not a radio,
USB integration, or ESP32 hardware test.

Run `python3 test/host/test_usb_scheduler.py` for failed submission / retry,
registration ordering, latest-pose replacement, updates during submission,
AUX ordering, critical status retention and 160-sensor scheduling tests.
Run `node test/host/test_diagnostics.cjs` for browser diagnostic parsing tests.

Run `python3 test/host/test_tracker_storage.py` for map removal, stable sibling
IDs, retired slots/MACs, restart behavior, count consistency and storage faults.
Run `node test/host/test_export_sensors.cjs` for sensor parsing and the report
privacy allowlist. The scheduler suite also verifies per-sensor metadata and
removal of queued poses/registrations.

Run `python3 test/host/test_channels.py` for AP scoring, allowed candidates,
scan refusal/interruption/cancellation/timeout, failed scan, deduplication,
expiry and save failures. Run `node test/host/test_channels.cjs` for channel
response validation. These use WiFi stubs and do not verify ESP32 RF behavior.
