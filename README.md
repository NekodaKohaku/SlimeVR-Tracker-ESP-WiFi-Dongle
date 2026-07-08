# SlimeVR HID ESPNow Dongle

This is a project that implements an alternative communication method for
SlimeVR trackers using the [ESPNow Protocol](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) 
available on ESP devices.

## Building and Flashing

To firmware currently only supports ESP32-S2 based dongles, however ESP32-S3
might be added in the future. To get a board working, add the necessary JSON file 
in the `boards/` directory and create a new directory and `pins_arduino.h` file
under `variants/`. After that, adding a new `env` definition in the
platformio.ini file should work.

To flash the dongle, run the `pio run -t upload` command.

## Usage

To use the dongle, it needs to be connected to a PC through USB. You also need
your trackers to have [compatible firmware](https://github.com/gorbit99/SlimeVR-Tracker-ESP/tree/dongle-support)
flashed onto them.

The trackers will require pairing the first time you set them up. To achieve
this, first you need to put the dongle into pairing mode by holding the button
on it for about two seconds, then reset the trackers three times in a row,
either by turning them off and on again in quick succession or by using the 
physical reset button on the tracker. If the pairing was successful, the tracker 
will blink three times.

After pairing, when you turn the tracker on from that point on, it will attempt to
connect to its saved dongle. A successful connection will be indicated by two
quick flashes on both the dongle and the tracker itself.

If everything went as expected, the tracker should now appear in the SlimeVR
server.

If you ever want to pair the tracker to a different dongle, just redo the
pairing procedure.

## Errors

In case something goes wrong, the dongle should indicate an error by flashing
its LED. Long blinks (0.5 seconds) represent a 1 bit, short blinks (0.1 seconds)
represent a 0 bit. The following error codes are currently possible:

|Code|Error|
|---|---|
|0x01|Failed to initialize ESPNow|
|0x02|Failed to add broadcast address as a sending destination|
|0x03|Failed to register a callback for receiving messages|
