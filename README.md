# pico-ps2-usb-kbd-bridge

**PS/2 to USB HID Keyboard Bridge for Raspberry Pi Pico 2**

A firmware that converts PS/2 keyboard input to USB HID output using the Raspberry Pi Pico 2 (RP2350). Designed specifically for compatibility with BMC64 and other systems that require Boot Keyboard protocol.

## Features

- 🎹 Full PS/2 Set 2 scancode translation to USB HID keycodes
- ⌨️ USB Boot Keyboard protocol for maximum compatibility
- 🔌 Simple 2-wire connection (CLK on GP16, DATA on GP17)
- ⚡ Low-latency 10ms polling
- 🎮 BMC64 compatible for retro computing projects
- 💡 LED feedback for device status and Caps Lock

## Hardware Requirements

- Raspberry Pi Pico 2 (RP2350) or original Pico (RP2040)
- PS/2 keyboard
- PS/2 connector or breakout

## Wiring

| PS/2 Pin | Color (typical) | Pico GPIO | Pico Pin |
|----------|-----------------|-----------|----------|
| CLK      | Brown/White     | GP16      | Pin 21   |
| DATA     | White/Green     | GP17      | Pin 22   |
| VCC      | Red             | 3V3 or 5V | Pin 36 (3V3) |
| GND      | Black           | GND       | Pin 38   |

> **Note:** The Pico's GPIO pins are 3.3V but are 5V tolerant for input. Most PS/2 keyboards work fine at 3.3V, but some may require 5V on VCC.

## Building

### Prerequisites

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (v2.0+)
- CMake (v3.13+)
- ARM GCC toolchain or RISC-V toolchain (for Pico 2)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/CCappsDevelopment/pico-ps2-usb-kbd-bridge.git
cd pico-ps2-usb-kbd-bridge

# Create build directory and configure
mkdir build
cd build
cmake ..

# Build
make -j4
# or if using Ninja:
ninja
```

### Flashing

1. Hold the BOOTSEL button on your Pico while connecting it to USB
2. Copy `build/dev_hid_composite.uf2` to the RPI-RP2 drive that appears
3. The Pico will reboot and appear as a USB keyboard

Or use picotool:
```bash
picotool load build/dev_hid_composite.uf2 -f
```

## How It Works

### PS/2 Protocol

The PS/2 keyboard protocol is a synchronous serial protocol where:
- The keyboard drives the **clock line** (10-16 kHz)
- Data is sampled on the **falling edge** of clock
- Each frame consists of: 1 start bit, 8 data bits (LSB first), 1 parity bit, 1 stop bit

The firmware polls the clock line and reconstructs bytes from the bitstream.

### Scancode Translation

PS/2 keyboards send "Set 2" scancodes:
- **Make codes**: Sent when a key is pressed (e.g., `0x1C` for 'A')
- **Break codes**: `0xF0` followed by the make code when released
- **Extended codes**: `0xE0` prefix for arrow keys, navigation cluster, right modifiers, etc.

The `ps2.c` module maintains lookup tables that map PS/2 scancodes to USB HID keycodes.

### USB HID Boot Keyboard

The firmware presents itself as a **USB Boot Keyboard** (class 3, subclass 1, protocol 1). This is the simplest keyboard protocol that:
- Uses a fixed 8-byte report format
- Has no report ID (for maximum compatibility)
- Works with BIOS, embedded systems, and simple USB hosts like BMC64

Report format:
| Byte | Description |
|------|-------------|
| 0    | Modifier keys (Ctrl, Shift, Alt, GUI as bits) |
| 1    | Reserved (always 0) |
| 2-7  | Up to 6 simultaneous key codes |

## File Structure

```
├── main.c              # Main loop, USB callbacks, HID task
├── ps2.c               # PS/2 decoder and scancode translation
├── ps2.h               # PS/2 module header
├── usb_descriptors.c   # USB device and HID descriptors
├── usb_descriptors.h   # Descriptor definitions
├── tusb_config.h       # TinyUSB configuration
├── CMakeLists.txt      # Build configuration
└── pico_sdk_import.cmake
```

## Changes from Original TinyUSB Example

This project is based on the [hid_composite example](https://github.com/hathach/tinyusb/tree/master/examples/device/hid_composite) from TinyUSB. Key modifications:

### Removed
- Mouse HID interface and reports
- Consumer Control (media keys) interface
- Gamepad interface
- Report IDs (not used in Boot Keyboard protocol)
- Board button input (replaced with PS/2 input)

### Added
- **`ps2.c` / `ps2.h`**: Complete PS/2 decoder module
  - GPIO polling for clock/data lines
  - Frame reconstruction (start, data, parity, stop bits)
  - Scancode state machine (handles 0xF0 break, 0xE0 extended prefixes)
  - Full scancode-to-HID translation tables
  - Modifier key tracking
  - 6-key rollover support

### Modified
- **`usb_descriptors.c`**: 
  - Changed to Boot Keyboard protocol (`HID_ITF_PROTOCOL_KEYBOARD`)
  - Removed report ID from descriptor
  - Updated product strings
- **`main.c`**:
  - Simplified to keyboard-only HID
  - Integrated PS/2 polling in main loop
  - Reports sent based on PS/2 state changes

## Supported Keys

### Standard Keys
- All letters (A-Z)
- Numbers (0-9)
- Function keys (F1-F12)
- Punctuation and symbols
- Tab, Enter, Backspace, Space, Escape

### Modifier Keys
- Left/Right Shift
- Left/Right Ctrl
- Left/Right Alt
- Left/Right GUI (Windows key)

### Navigation Cluster (Extended)
- Arrow keys (Up, Down, Left, Right)
- Insert, Delete, Home, End
- Page Up, Page Down

### Numpad
- Numpad 0-9
- Numpad operators (+, -, *, /)
- Numpad Enter
- Num Lock

## LED Status

The onboard LED indicates device status:
- **Fast blink (250ms)**: USB not mounted
- **Slow blink (1000ms)**: USB mounted and ready
- **Very slow blink (2500ms)**: USB suspended
- **Solid on**: Caps Lock active

## Troubleshooting

### Keyboard not responding
- Check wiring connections
- Ensure pull-ups are working (CLK and DATA should be high when idle)
- Try 5V on VCC if using 3.3V

### Some keys not working
- Check if the key is in the scancode translation tables in `ps2.c`
- Extended keys require proper 0xE0 prefix handling

### Not working with BMC64
- Ensure Boot Keyboard protocol is enabled (check `usb_descriptors.c`)
- The device should enumerate as protocol 1 (keyboard)

## License

MIT License - Based on TinyUSB examples by Ha Thach.

## Credits

- [TinyUSB](https://github.com/hathach/tinyusb) - USB stack
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) - Hardware abstraction