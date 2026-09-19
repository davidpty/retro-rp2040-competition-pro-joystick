# Retro RP2040 USB Joystick

Open-source **RP2040-Zero firmware for converting classic digital joysticks into USB HID game controllers**. Designed for DIY retro-computing and gaming projects using Commodore 64, Amiga, Atari, Competition Pro, and other DE-9 joysticks.

This project provides a reusable USB interface for classic digital joysticks. With a suitable DE-9 connector or adapter harness, compatible C64, Amiga, Atari, and other 9-pin joysticks can be connected to a Raspberry Pi Pico-compatible RP2040-Zero board and used as standard USB HID controllers on Linux, Windows, and other USB host systems.

The board can also replace the original USB electronics inside retro-style joysticks such as the older Speedlink Competition Pro SL-6602. That hardware reports input changes at only about 12.5 times per second, which can cause noticeable lag in fast games. Replacing the controller with this RP2040-based firmware allows the original switches and enclosure to be retained while adding faster USB reporting, configurable button mappings, autofire, and selectable fast or slow compatibility modes.

Useful for **retro joystick USB adapters**, **Competition Pro upgrades**, **replacement joystick electronics**, and custom arcade or emulator controllers. The firmware supports configurable GPIO and button mappings, autofire, non-volatile settings, status LED feedback, BOOTSEL update mode, and selectable fast or compatibility reporting modes.

## Quick start

### 1. Wire the controls

Connect the RP2040-Zero `GND` to the common switch ground. Connect the other side of each switch to the GPIO shown below. The firmware uses internal pull-ups, so each input is active-low: it is pressed when connected to `GND`.

| Control | GPIO |
|---|---:|
| Up | GP0 |
| Down | GP1 |
| Left | GP2 |
| Right | GP3 |
| Big Fire 1 | GP4 |
| Big Fire 2 | GP5 |
| Small Fire 1 | GP6 |
| Small Fire 2 | GP7 |

The onboard status LED uses GP16. Do not use GP16 for a joystick input.

Default USB mapping:

| Physical control | USB output |
|---|---|
| Big Fire 1 | Button 1 |
| Big Fire 2 | Button 2 |
| Small Fire 1 | Autofire for Button 1 |
| Small Fire 2 | Button 3 |

### 2. Build the firmware

Install the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk), then set its path and run the build script:

```sh
PICO_SDK_PATH=/path/to/pico-sdk ./build-firmware.sh
```

The firmware is written to:

```text
build/rp2040_zero_hid_joystick.uf2
```

### 3. Flash the board

1. Hold the board's **BOOT** button while plugging it into USB. If the board has RESET, you can hold **BOOT** and tap **RESET** instead.
2. Release **BOOT** when the `RPI-RP2` drive appears.
3. Copy the firmware to that drive:

```sh
cp build/rp2040_zero_hid_joystick.uf2 /media/$USER/RPI-RP2/
```

The board reboots automatically and should appear as `Retro 2040 Competition Pro`.

## Using the joystick

- The default mode reports input quickly, approximately every 1 ms.
- Direction pairs report center when both directions on the same axis are pressed.
- Hold Small Fire 1 to enable autofire on Button 1. The default rate is 20 Hz and can be adjusted from 1–60 Hz.
- The USB identity is manufacturer `Retro 2040`, product `Competition Pro`, with the board ID as its serial number.

### Button gestures

Gestures use the physical fire buttons. Hold a pair for 500 ms where noted.

| Buttons held | Action |
|---|---|
| Big Fire 1 + Small Fire 1 | Decrease autofire rate |
| Big Fire 2 + Small Fire 1 | Increase autofire rate |
| Big Fire 1 + Small Fire 2 | Select slow compatibility mode |
| Big Fire 2 + Small Fire 2 | Select fast mode |
| Small Fire 1 + Small Fire 2 for 3 seconds | Enter BOOTSEL update mode |
| Big Fire 1 + Big Fire 2 for 3 seconds | Toggle normal LED feedback |
| All four fire buttons for 3 seconds | Restore fast mode, 20 Hz autofire, and LED feedback |

Settings are saved and restored after reboot. Releasing the buttons cancels a gesture before its hold time is reached.

## Configuration

Edit [`config.h`](config.h) before building to change:

- GPIO assignments and USB button mappings
- Autofire input, target button, rate limits, and default rate
- BOOTSEL gesture buttons and hold time
- Fast/slow report intervals and debounce time
- LED colors and USB identity

The default slow mode reports ordinary joystick input approximately every 80 ms. Autofire temporarily uses the fast report interval so its selected rate can be transmitted accurately.

## Testing on Linux

After flashing, reconnect the board normally. Install the optional joystick tools and inspect the device:

```sh
sudo apt install evtest joystick
evtest
jstest /dev/input/jsX
```

Check the two axes and all three buttons. The exact `/dev/input/jsX` number depends on other controllers connected to the system. SDL applications and emulators should recognize the device as a generic joystick, although an emulator may need its joystick mapping configured once.

Host-side logic tests do not require the Pico SDK or a connected board:

```sh
cmake -S tests -B build-host-tests
cmake --build build-host-tests
ctest --test-dir build-host-tests --output-on-failure
```

## Notes

- The project uses the generic `pico` SDK board definition because the RP2040-Zero uses the same RP2040 USB controller and GPIO numbering needed by this firmware.
- The current VID/PID (`0xCafe`/`0x4022`) is a development identifier. Replace it with an appropriately assigned VID/PID before commercial distribution.
