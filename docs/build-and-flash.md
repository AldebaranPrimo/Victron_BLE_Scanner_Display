# Build & Flash Notes

This branch (`feature/multi-device-mppt`, firmware **v2.0.0**) targets the
**M5StickC Plus** and was developed against the **pioarduino** fork of the
espressif32 platform (`arduino-esp32 3.x`). The notes below let you reproduce a
working build/flash, including the environment quirks we hit on Windows.

> **Status: testing branch.** Functionally validated on real hardware (boots,
> display OK, MQTT publish, web portal). Not a tagged release. See "Known
> limitations" in the README before relying on it.

## Normal path (clean PlatformIO install)

```bash
pio run -e m5stick-c-plus            # build
pio run -e m5stick-c-plus -t upload  # flash
```

On a stock M5StickC Plus (CP2104 / CH9102 USB-serial) with a healthy PlatformIO,
this usually just works. If it does, ignore the troubleshooting below.

## Platform / board notes

- The `m5stick-c-plus` env pins `framework = arduino` on `platform = espressif32`.
  With the **pioarduino** platform (55.x → arduino-esp32 3.x) installed, that
  platform ships **no `m5stick-c-plus` board id** — only `m5stick-c`. So the env
  uses `board = m5stick-c` plus `board_build.variant = m5stack_stickc_plus`, and
  selects the Plus display via the `-D M5STICKCPLUS` flag + the `M5StickCPlus`
  library. The variant name `m5stack_stickc_plus` is what that framework ships;
  on a different platform/framework version it may differ.
- On the **standard** espressif32 platform (6.x → arduino-esp32 2.x) the board id
  `m5stick-c-plus` exists and can be used directly. If you build there, the two
  arduino-esp32 3.x BLE API adaptations in `victron_ble.cpp` (`getManufacturerData()`
  returning `String`; `BLEScan::start()` returning a pointer) are harmless.

## Troubleshooting (quirks seen on this Windows dev machine)

These are local-environment issues, not firmware bugs — documented so you don't
lose time if you hit the same install.

1. **`'xtensa-esp32-elf-g++' is not recognized`** — the build calls the legacy
   toolchain name while the installed unified toolchain is `xtensa-esp-elf`.
   That package ships a compat-named binary, so prepend it to PATH for the build:
   ```powershell
   $env:PATH = "C:\Users\<you>\.platformio\packages\xtensa-esp-elf\bin;" + $env:PATH
   pio run -e m5stick-c-plus
   ```

2. **Upload stalls / `chip stopped responding` at high baud** — on a board with
   an **FTDI FT232R** USB-serial, the auto-reset survives the initial connect but
   the baud-rate bump to 460800 breaks communication. Flash at **115200** instead.
   The easiest reliable path is to write the merged image directly with esptool:
   ```powershell
   $env:PYTHONIOENCODING = "utf-8"   # see note 3
   python -m esptool --port COM5 write_flash 0x0 `
     .pio\build\m5stick-c-plus\firmware.factory.bin
   ```
   (`firmware.factory.bin` is the combined bootloader+partitions+app image,
   written at offset `0x0`.)

3. **`UnicodeEncodeError: 'charmap'`** during flash — esptool's progress bar uses
   block glyphs the Windows cp1252 console can't encode. Set
   `PYTHONIOENCODING=utf-8` (and optionally `chcp 65001`) before running esptool.

## Reading the serial boot log

Default pyserial open asserts DTR/RTS and can hold the ESP in reset (silent).
To boot the app and capture the log:

```python
import serial, time
s = serial.Serial("COM5", 115200, timeout=1)
s.dtr = False; s.rts = True; time.sleep(0.2); s.rts = False  # reset into app
while True:
    line = s.readline()
    if line:
        print(line.decode("utf-8", "replace"), end="")
```

A healthy boot prints the `Victron BLE Gateway v2.0.0` banner, then either the
configured device list or, with no config, `entering setup` + the AP details.

## Runtime warnings on arduino-esp32 3.x

With the `M5StickCPlus@0.1.1` library (written for arduino-esp32 2.x), 3.x logs
non-fatal warnings at boot: `digitalWrite()` on the TFT control pins "not set as
GPIO", `ledc` "frequency can't be zero", and a spurious `gpio_pullup_en` error.
**The display still works** (verified on hardware). A clean fix (migrate to
M5Unified, or pin arduino-esp32 2.x) is deferred.
