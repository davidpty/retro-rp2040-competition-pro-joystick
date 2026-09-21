# Retro RP2040 USB Joystick

Turn a classic C64, Amiga, Atari, Competition Pro, or other DE-9 joystick into a modern USB controller with an RP2040-Zero.

The original switches and enclosure can be kept while the firmware adds faster input response and flexible controls for computers, games, and emulators. It can send button presses to a USB gamepad, type keyboard keys, or do both depending on the selected mapping.

### Key features

- Four persistent button-mapping profiles shown via LED Red, Green, Blue, and Yellow.
- Each profile can assign all four fire buttons and all four directions to joystick directions, gamepad buttons, keyboard keys, or keyboard combinations.
- Optional autofire can be assigned to any mapped input, either immediately or after a configurable hold delay.
- Hold both small buttons and move the joystick Up, Down, Left, or Right to select a profile.
- Fast polling for responsive games and slow polling for compatibility with older systems.
- A simple USB configuration drive for editing all four profiles without rebuilding the firmware.
- Status LED feedback for the active mapping, autofire, configuration mode, and compatibility mode.
- Settings are saved across reboots, with a factory reset that restores the default profile mapping and leaves the other profiles untouched.

## Download the firmware

The compiled UF2 firmware is available from the [latest GitHub release](https://github.com/davidpty/retro-rp2040-competition-pro-joystick/releases/latest). Download `rp2040_zero_hid_joystick.uf2` and copy it to the `RPI-RP2` drive while the RP2040-Zero is in BOOTSEL mode.

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
| Big Fire 1 | `JOY1` autofire after 500 ms |
| Big Fire 2 | `SPACE` keyboard key |
| Small Fire 1 | Fixed 25 Hz `JOY1` autofire |
| Small Fire 2 | Fixed 5 Hz `JOY1` autofire |

Every output can be remapped at runtime through the built-in configuration
drive — see [Configuring the mappings](#configuring-the-mappings).

### 2. Build the firmware

Install CMake, the ARM GCC toolchain, and the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk). On Debian or Ubuntu:

```sh
sudo apt install cmake ninja-build gcc-arm-none-eabi \
  libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

Set `PICO_SDK_PATH` to the SDK directory and run the included build script. It
checks for a working native CMake executable before configuring the project:

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

The device presents two USB interfaces: a **gamepad** with two axes and four
buttons, and a **keyboard**. Fire buttons map to gamepad buttons or keyboard
keys depending on the configured codes.

- The default mode reports input quickly, approximately every 1 ms.
- Direction pairs report center when both directions on the same axis are pressed.
- By default Big Fire 1 autofires `JOY1` after 500 ms at the adjustable global
  rate. Small Fire 1 provides fixed 25 Hz autofire and Small Fire 2 provides
  fixed 5 Hz autofire. The default global rate is 20 Hz and can be adjusted
  from 1–100 Hz; autofire can be moved to any output via the config drive.
- The USB identity is manufacturer `Retro 2040`, product `Competition Pro`, with the board ID as its serial number.

### Status LED

The onboard RGB LED mostly shows the active profile color at full brightness.
The four profiles are named after their colors: Red, Blue, Green, and Yellow.

| LED | Meaning |
|---|---|
| Profile color | Active profile: any fire button held |
| Profile color dimmed | Slow compatibility polling mode active |
| Profile color pulsing | An autofire input is held; pulse rate matches the configured autofire rate |
| Purple | Configuration drive is active |
| Magenta | Firmware update selected at 6 s while Small Fire 1 + 2 remain held |
| Three red flashes | A rejected or incomplete `JOYSTICK.INI` before reboot |
| Off | Idle, or LED feedback toggled off |

### Button gestures

Gestures use the physical fire buttons and cannot be remapped. Hold a pair for
500 ms where noted.

| Buttons held | Action |
|---|---|
| Big Fire 1 + Small Fire 1 | Decrease autofire rate |
| Big Fire 2 + Small Fire 1 | Increase autofire rate |
| Big Fire 1 + Small Fire 2 | Select slow compatibility mode |
| Big Fire 2 + Small Fire 2 | Select fast mode |
| Small Fire 1 + Small Fire 2 for 3 seconds, release | Enter configuration mode (USB drive with `JOYSTICK.INI`) |
| Small Fire 1 + Small Fire 2 for 6 seconds | Immediately enter BOOTSEL update mode |
| Small Fire 1 + Small Fire 2 + joystick Up | Select Red profile after 500 ms |
| Small Fire 1 + Small Fire 2 + joystick Down | Select Blue profile after 500 ms |
| Small Fire 1 + Small Fire 2 + joystick Left | Select Green profile after 500 ms |
| Small Fire 1 + Small Fire 2 + joystick Right | Select Yellow profile after 500 ms |
| Big Fire 1 + Big Fire 2 for 3 seconds | Toggle normal LED feedback |
| All four fire buttons for 3 seconds | Select Red, reset only its mapping to factory defaults, and reset global settings |

Settings are saved and restored after reboot. Releasing the buttons cancels a gesture before its hold time is reached.

After the autofire-rate gesture activates, the profile LED blinks at the
current adjustable rate. This indication is independent of the mappings of the
buttons being held; fixed `:HZ` autofire mappings are not changed.

## Configuring the mappings

The four fire buttons and four joystick directions can be remapped independently
for each button-mapping profile without rebuilding the firmware. Profiles can
send joystick directions, gamepad buttons, keyboard keys, or keyboard
combinations, and any mapped input can use autofire. `:AUTOFIRE` starts pulsing
immediately; `:AUTOFIRE:500MS` keeps the output held normally for 500 ms,
then starts pulsing while the input remains held. Add `:20HZ` to use a fixed
frequency for that input. `MS` and `HZ` parameters may appear in either order;
delays range from 1 to 60000 ms and frequencies from 1 to 100 Hz.

1. Hold **Small Fire 1 + Small Fire 2 for 3 seconds**, then release. While
   holding, the status LED lights once config mode is selected. On release the
   board reboots into configuration mode: the joystick is disconnected and a
   USB drive appears. Holding for 6 seconds enters BOOTSEL immediately, even
   while the buttons remain pressed.
2. Open the `JOYSTICK.INI` file and edit the eight mapping lines in each
   profile section:

   ```ini
   [RED]
   button1=JOY1:AUTOFIRE:500MS ; Adjustable global rate after 500 ms
   button2=SPACE                ; Keyboard key for Big Fire 2
   button3=JOY1:AUTOFIRE:25HZ   ; Fixed fast autofire for Small Fire 1
   button4=JOY1:AUTOFIRE:5HZ    ; Fixed slow autofire for Small Fire 2
   up=UP
   down=DOWN
   left=LEFT
   right=RIGHT

   [BLUE]
   button1=JOY1                ; Normal A / Big Fire 1
   button2=JOY2                ; Normal B / Big Fire 2
   button3=JOY1:AUTOFIRE       ; Adjustable autofire A / Small Fire 1
   button4=JOY2:AUTOFIRE       ; Adjustable autofire B / Small Fire 2
   up=UP
   down=DOWN
   left=LEFT
   right=RIGHT

   [GREEN]
   button1=JOY1                 ; A
   button2=JOY2                 ; B
   button3=JOY3                 ; Normal C
   button4=JOY3:AUTOFIRE:25HZ         ; Fixed-rate autofire C
   up=UP
   down=DOWN
   left=LEFT
   right=RIGHT

   [YELLOW]
   button1=JOY1
   button2=JOY2
   button3=JOY3
   button4=JOY4
   up=UP
   down=DOWN
   left=LEFT
   right=RIGHT
   ```

   A delayed autofire mapping can be written as, for example,
   `button1=JOY1:AUTOFIRE:500MS:20HZ`.

   | Input | GPIO | Output names |
   |---|---:|---|
   | Big Fire 1 | 4 | `button1`, `JOY1` .. `JOY4` |
   | Big Fire 2 | 5 | `button2`, `JOY1` .. `JOY4` |
   | Small Fire 1 | 6 | `button3`, `JOY1` .. `JOY4` |
   | Small Fire 2 | 7 | `button4`, `JOY1` .. `JOY4` |
   | Up / Down / Left / Right | 0 / 1 / 2 / 3 | `up`, `down`, `left`, `right` |

   Valid outputs:

   | Group | Codes |
   |---|---|
   | Joystick directions | `UP`, `DOWN`, `LEFT`, `RIGHT` |
   | Gamepad buttons | `JOY1` .. `JOY4` |
   | Letters | `A` .. `Z` |
   | Digits | `0` .. `9` |
   | Keys | `ENTER`, `ESC`, `BACKSPACE`, `TAB`, `SPACE` |
   | Function keys | `F1` .. `F12` |
   | Modifiers | `SHIFT`, `CTRL`, `ALT` |
   | Off | `NONE` |

3. Save and unmount (or eject) the drive. After about a second the board
   applies the new mapping and reboots automatically into joystick mode. The
   LED remains purple while the configuration drive is active.

Notes:

- Names and values are case-insensitive; lines starting with `;` are comments;
  `\\r\\n` or `\\n` line endings are accepted.
- All four profile sections and all eight mapping lines must be present and valid,
  otherwise the previous configuration is kept.
- Use `SHIFT+A`, `CTRL+ALT+B`, or similar plus-separated combinations for one
  physical input. Add `:AUTOFIRE` to any mapping, including a direction, to
  repeat it at the configured rate while held. Add `:20HZ` to pin that input
  to 20 Hz; fixed frequencies are not changed by the rate-adjustment gesture.
- To reverse the joystick in a profile, swap its axis values, for example
  `up=DOWN`, `down=UP`, `left=RIGHT`, and `right=LEFT`.
- Autofire takes priority once active: if an input with `:AUTOFIRE` or
  `:AUTOFIRE:500MS` and another input share the same output, the shared output
  follows the autofire pattern after the delay has elapsed. Multiple autofire
  inputs sharing one output combine their independent pulses.
- The board reboots after an eject. If `JOYSTICK.INI` has changed the mapping,
  the new settings are used; an unchanged file exits config mode without an
  error indication.
- An unreadable or incomplete file (still being written, or malformed) keeps
  the old mapping. After the save becomes idle or the drive is unmounted, the
  status LED flashes red three times and the board remains in config mode so
  the file can be corrected and saved again. A valid file, or an unchanged
  file after eject, reboots into joystick mode as described above.
- While the configuration drive is open the gamepad/keyboard are disconnected;
  joystick directions and fire buttons have no joystick or keyboard effect.
  Holding Small Fire 1 + Small Fire 2 for 3 seconds while in the drive
  immediately exits configuration mode and reboots into normal joystick mode.
  Unsaved edits are discarded. The six-second BOOTSEL gesture is available
  from normal joystick mode only.

## Build-time configuration

Edit [`config.h`](config.h) before building to change:

- GPIO assignments
- Factory mappings in [`default_joystick.ini`](default_joystick.ini)
- Autofire rate limits and default rate
- Config/firmware gesture buttons and hold times
- Fast/slow report intervals and debounce time
- Profile, status LED colors, and USB identity

To build firmware that restores the embedded [`default_joystick.ini`](default_joystick.ini)
once on its first boot, use:

```sh
./build-firmware.sh --overwrite-settings
```

Normal builds preserve saved settings. Use `./build-firmware.sh -h` for build
options and environment variables.

The default slow mode reports ordinary joystick input approximately every 80 ms. Autofire temporarily uses the fast report interval so its selected rate can be transmitted accurately.

## Testing on Linux

After flashing, reconnect the board normally. Install the optional joystick tools and inspect the device:

```sh
sudo apt install evtest joystick
evtest
jstest /dev/input/jsX
```

Check the two axes and all four buttons. The exact `/dev/input/jsX` number depends on other controllers connected to the system. SDL applications and emulators should recognize the device as a generic joystick, although an emulator may need its joystick mapping configured once.

Host-side logic tests do not require the Pico SDK or a connected board:

```sh
./build-host-tests.sh
```

The test script builds the host test executable with CMake and runs it
directly. This avoids relying on a Python `ctest` wrapper that may be installed
ahead of the native CMake tools on `PATH`.

## Notes

- The project uses the generic `pico` SDK board definition because the RP2040-Zero uses the same RP2040 USB controller and GPIO numbering needed by this firmware.
- The current VID/PID (`0xCafe`/`0x4022`) is a development identifier. Replace it with an appropriately assigned VID/PID before commercial distribution.
