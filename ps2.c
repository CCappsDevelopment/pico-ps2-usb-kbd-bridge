/*
 * PS/2 to USB HID Keyboard Bridge
 * PS/2 Decoder Implementation
 * 
 * Converts PS/2 Set 2 scancodes to USB HID keycodes
 * Designed for BMC64 compatibility (Boot Keyboard protocol)
 */

#include "ps2.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>

//--------------------------------------------------------------------+
// HID Keycode Definitions (from USB HID Usage Tables)
//--------------------------------------------------------------------+

// Letters A-Z (0x04 - 0x1D)
#define HID_KEY_A               0x04
#define HID_KEY_B               0x05
#define HID_KEY_C               0x06
#define HID_KEY_D               0x07
#define HID_KEY_E               0x08
#define HID_KEY_F               0x09
#define HID_KEY_G               0x0A
#define HID_KEY_H               0x0B
#define HID_KEY_I               0x0C
#define HID_KEY_J               0x0D
#define HID_KEY_K               0x0E
#define HID_KEY_L               0x0F
#define HID_KEY_M               0x10
#define HID_KEY_N               0x11
#define HID_KEY_O               0x12
#define HID_KEY_P               0x13
#define HID_KEY_Q               0x14
#define HID_KEY_R               0x15
#define HID_KEY_S               0x16
#define HID_KEY_T               0x17
#define HID_KEY_U               0x18
#define HID_KEY_V               0x19
#define HID_KEY_W               0x1A
#define HID_KEY_X               0x1B
#define HID_KEY_Y               0x1C
#define HID_KEY_Z               0x1D

// Numbers 1-0 (0x1E - 0x27)
#define HID_KEY_1               0x1E
#define HID_KEY_2               0x1F
#define HID_KEY_3               0x20
#define HID_KEY_4               0x21
#define HID_KEY_5               0x22
#define HID_KEY_6               0x23
#define HID_KEY_7               0x24
#define HID_KEY_8               0x25
#define HID_KEY_9               0x26
#define HID_KEY_0               0x27

// Special keys
#define HID_KEY_ENTER           0x28
#define HID_KEY_ESCAPE          0x29
#define HID_KEY_BACKSPACE       0x2A
#define HID_KEY_TAB             0x2B
#define HID_KEY_SPACE           0x2C
#define HID_KEY_MINUS           0x2D
#define HID_KEY_EQUAL           0x2E
#define HID_KEY_BRACKET_LEFT    0x2F
#define HID_KEY_BRACKET_RIGHT   0x30
#define HID_KEY_BACKSLASH       0x31
#define HID_KEY_SEMICOLON       0x33
#define HID_KEY_APOSTROPHE      0x34
#define HID_KEY_GRAVE           0x35
#define HID_KEY_COMMA           0x36
#define HID_KEY_PERIOD          0x37
#define HID_KEY_SLASH           0x38
#define HID_KEY_CAPS_LOCK       0x39

// Function keys F1-F12
#define HID_KEY_F1              0x3A
#define HID_KEY_F2              0x3B
#define HID_KEY_F3              0x3C
#define HID_KEY_F4              0x3D
#define HID_KEY_F5              0x3E
#define HID_KEY_F6              0x3F
#define HID_KEY_F7              0x40
#define HID_KEY_F8              0x41
#define HID_KEY_F9              0x42
#define HID_KEY_F10             0x43
#define HID_KEY_F11             0x44
#define HID_KEY_F12             0x45

// Print Screen, Scroll Lock, Pause
#define HID_KEY_PRINT_SCREEN    0x46
#define HID_KEY_SCROLL_LOCK     0x47
#define HID_KEY_PAUSE           0x48

// Navigation cluster
#define HID_KEY_INSERT          0x49
#define HID_KEY_HOME            0x4A
#define HID_KEY_PAGE_UP         0x4B
#define HID_KEY_DELETE          0x4C
#define HID_KEY_END             0x4D
#define HID_KEY_PAGE_DOWN       0x4E
#define HID_KEY_ARROW_RIGHT     0x4F
#define HID_KEY_ARROW_LEFT      0x50
#define HID_KEY_ARROW_DOWN      0x51
#define HID_KEY_ARROW_UP        0x52

// Numpad
#define HID_KEY_NUM_LOCK        0x53
#define HID_KEY_KEYPAD_DIVIDE   0x54
#define HID_KEY_KEYPAD_MULTIPLY 0x55
#define HID_KEY_KEYPAD_SUBTRACT 0x56
#define HID_KEY_KEYPAD_ADD      0x57
#define HID_KEY_KEYPAD_ENTER    0x58
#define HID_KEY_KEYPAD_1        0x59
#define HID_KEY_KEYPAD_2        0x5A
#define HID_KEY_KEYPAD_3        0x5B
#define HID_KEY_KEYPAD_4        0x5C
#define HID_KEY_KEYPAD_5        0x5D
#define HID_KEY_KEYPAD_6        0x5E
#define HID_KEY_KEYPAD_7        0x5F
#define HID_KEY_KEYPAD_8        0x60
#define HID_KEY_KEYPAD_9        0x61
#define HID_KEY_KEYPAD_0        0x62
#define HID_KEY_KEYPAD_DECIMAL  0x63

// Modifier key bits (for the modifier byte)
#define HID_MOD_LEFT_CTRL       0x01
#define HID_MOD_LEFT_SHIFT      0x02
#define HID_MOD_LEFT_ALT        0x04
#define HID_MOD_LEFT_GUI        0x08
#define HID_MOD_RIGHT_CTRL      0x10
#define HID_MOD_RIGHT_SHIFT     0x20
#define HID_MOD_RIGHT_ALT       0x40
#define HID_MOD_RIGHT_GUI       0x80

// Application/Menu key
#define HID_KEY_APPLICATION     0x65

//--------------------------------------------------------------------+
// PS/2 Scancode to HID Keycode Mapping Tables
//--------------------------------------------------------------------+

// PS/2 Set 2 scancode -> HID Keycode (standard, non-extended keys)
// Index is PS/2 scancode, value is HID keycode (0 = unmapped)
static const uint8_t scancode_to_hid[256] = {
    // 0x00-0x0F
    [0x01] = HID_KEY_F9,
    [0x03] = HID_KEY_F5,
    [0x04] = HID_KEY_F3,
    [0x05] = HID_KEY_F1,
    [0x06] = HID_KEY_F2,
    [0x07] = HID_KEY_F12,
    [0x09] = HID_KEY_F10,
    [0x0A] = HID_KEY_F8,
    [0x0B] = HID_KEY_F6,
    [0x0C] = HID_KEY_F4,
    [0x0D] = HID_KEY_TAB,
    [0x0E] = HID_KEY_GRAVE,
    
    // 0x10-0x1F
    [0x11] = 0xFF,  // Left Alt (handled as modifier)
    [0x12] = 0xFE,  // Left Shift (handled as modifier)
    [0x14] = 0xFD,  // Left Ctrl (handled as modifier)
    [0x15] = HID_KEY_Q,
    [0x16] = HID_KEY_1,
    [0x1A] = HID_KEY_Z,
    [0x1B] = HID_KEY_S,
    [0x1C] = HID_KEY_A,
    [0x1D] = HID_KEY_W,
    [0x1E] = HID_KEY_2,
    
    // 0x20-0x2F
    [0x21] = HID_KEY_C,
    [0x22] = HID_KEY_X,
    [0x23] = HID_KEY_D,
    [0x24] = HID_KEY_E,
    [0x25] = HID_KEY_4,
    [0x26] = HID_KEY_3,
    [0x29] = HID_KEY_SPACE,
    [0x2A] = HID_KEY_V,
    [0x2B] = HID_KEY_F,
    [0x2C] = HID_KEY_T,
    [0x2D] = HID_KEY_R,
    [0x2E] = HID_KEY_5,
    
    // 0x30-0x3F
    [0x31] = HID_KEY_N,
    [0x32] = HID_KEY_B,
    [0x33] = HID_KEY_H,
    [0x34] = HID_KEY_G,
    [0x35] = HID_KEY_Y,
    [0x36] = HID_KEY_6,
    [0x3A] = HID_KEY_M,
    [0x3B] = HID_KEY_J,
    [0x3C] = HID_KEY_U,
    [0x3D] = HID_KEY_7,
    [0x3E] = HID_KEY_8,
    
    // 0x40-0x4F
    [0x41] = HID_KEY_COMMA,
    [0x42] = HID_KEY_K,
    [0x43] = HID_KEY_I,
    [0x44] = HID_KEY_O,
    [0x45] = HID_KEY_0,
    [0x46] = HID_KEY_9,
    [0x49] = HID_KEY_PERIOD,
    [0x4A] = HID_KEY_SLASH,
    [0x4B] = HID_KEY_L,
    [0x4C] = HID_KEY_SEMICOLON,
    [0x4D] = HID_KEY_P,
    [0x4E] = HID_KEY_MINUS,
    
    // 0x50-0x5F
    [0x52] = HID_KEY_APOSTROPHE,
    [0x54] = HID_KEY_BRACKET_LEFT,
    [0x55] = HID_KEY_EQUAL,
    [0x58] = 0xFC,  // Caps Lock (handled specially)
    [0x59] = 0xFB,  // Right Shift (handled as modifier)
    [0x5A] = HID_KEY_ENTER,
    [0x5B] = HID_KEY_BRACKET_RIGHT,
    [0x5D] = HID_KEY_BACKSLASH,
    
    // 0x60-0x6F
    [0x66] = HID_KEY_BACKSPACE,
    [0x69] = HID_KEY_KEYPAD_1,
    [0x6B] = HID_KEY_KEYPAD_4,
    [0x6C] = HID_KEY_KEYPAD_7,
    
    // 0x70-0x7F
    [0x70] = HID_KEY_KEYPAD_0,
    [0x71] = HID_KEY_KEYPAD_DECIMAL,
    [0x72] = HID_KEY_KEYPAD_2,
    [0x73] = HID_KEY_KEYPAD_5,
    [0x74] = HID_KEY_KEYPAD_6,
    [0x75] = HID_KEY_KEYPAD_8,
    [0x76] = HID_KEY_ESCAPE,
    [0x77] = HID_KEY_SCROLL_LOCK,
    [0x78] = HID_KEY_F11,
    [0x79] = HID_KEY_KEYPAD_ADD,
    [0x7A] = HID_KEY_KEYPAD_3,
    [0x7B] = HID_KEY_KEYPAD_SUBTRACT,
    [0x7C] = HID_KEY_KEYPAD_MULTIPLY,
    [0x7D] = HID_KEY_KEYPAD_9,
    [0x7E] = HID_KEY_NUM_LOCK,
    
    // 0x80-0x8F
    [0x83] = HID_KEY_F7,
};

// PS/2 Set 2 Extended scancodes (prefixed with 0xE0) -> HID Keycode
static const uint8_t extended_scancode_to_hid[256] = {
    [0x11] = 0xFA,  // Right Alt (handled as modifier)
    [0x14] = 0xF9,  // Right Ctrl (handled as modifier)
    [0x1F] = 0xF8,  // Left GUI (handled as modifier)
    [0x27] = 0xF7,  // Right GUI (handled as modifier)
    [0x2F] = HID_KEY_APPLICATION,  // Menu/Context key
    
    // Numpad extended
    [0x4A] = HID_KEY_KEYPAD_DIVIDE,
    [0x5A] = HID_KEY_KEYPAD_ENTER,
    
    // Navigation cluster
    [0x69] = HID_KEY_END,
    [0x6B] = HID_KEY_ARROW_LEFT,
    [0x6C] = HID_KEY_HOME,
    [0x70] = HID_KEY_INSERT,
    [0x71] = HID_KEY_DELETE,
    [0x72] = HID_KEY_ARROW_DOWN,
    [0x74] = HID_KEY_ARROW_RIGHT,
    [0x75] = HID_KEY_ARROW_UP,
    [0x7A] = HID_KEY_PAGE_DOWN,
    [0x7D] = HID_KEY_PAGE_UP,
};

//--------------------------------------------------------------------+
// Global Keyboard State
//--------------------------------------------------------------------+

static uint8_t g_modifiers = 0;        // Current modifier key state
static uint8_t g_keys[6] = {0};        // Up to 6 simultaneous key presses
static bool g_state_changed = false;   // Flag to indicate state changed

// PS/2 Frame decoding state
static uint8_t frame_bit_index = 0;
static uint8_t scancode_byte = 0;
static bool break_pending = false;     // True after receiving 0xF0
static bool extended_pending = false;  // True after receiving 0xE0
static bool last_clk = true;           // Previous clock state

//--------------------------------------------------------------------+
// Helper Functions
//--------------------------------------------------------------------+

// Check if a keycode is a modifier and return its bit mask
// Returns 0 if not a modifier
static uint8_t get_modifier_mask(uint8_t hid_code) {
    switch (hid_code) {
        case 0xFF: return HID_MOD_LEFT_ALT;     // PS/2 0x11
        case 0xFE: return HID_MOD_LEFT_SHIFT;   // PS/2 0x12
        case 0xFD: return HID_MOD_LEFT_CTRL;    // PS/2 0x14
        case 0xFC: return 0;                     // Caps Lock - not a modifier
        case 0xFB: return HID_MOD_RIGHT_SHIFT;  // PS/2 0x59
        case 0xFA: return HID_MOD_RIGHT_ALT;    // PS/2 E0 11
        case 0xF9: return HID_MOD_RIGHT_CTRL;   // PS/2 E0 14
        case 0xF8: return HID_MOD_LEFT_GUI;     // PS/2 E0 1F
        case 0xF7: return HID_MOD_RIGHT_GUI;    // PS/2 E0 27
        default:   return 0;
    }
}

// Press a key (add to the current state)
static void press_key(uint8_t hid_code) {
    if (hid_code == 0) return;
    
    // Check if it's a modifier key
    uint8_t mod_mask = get_modifier_mask(hid_code);
    if (mod_mask != 0) {
        if (!(g_modifiers & mod_mask)) {
            g_modifiers |= mod_mask;
            g_state_changed = true;
        }
        return;
    }
    
    // Handle Caps Lock specially (it's a toggle, but we just send press/release)
    if (hid_code == 0xFC) {
        hid_code = HID_KEY_CAPS_LOCK;
    }
    
    // Check if already pressed
    for (int i = 0; i < 6; i++) {
        if (g_keys[i] == hid_code) return; // Already pressed
    }
    
    // Find empty slot and add
    for (int i = 0; i < 6; i++) {
        if (g_keys[i] == 0) {
            g_keys[i] = hid_code;
            g_state_changed = true;
            return;
        }
    }
    // No empty slot - 6 keys already pressed (rollover)
}

// Release a key (remove from the current state)
static void release_key(uint8_t hid_code) {
    if (hid_code == 0) return;
    
    // Check if it's a modifier key
    uint8_t mod_mask = get_modifier_mask(hid_code);
    if (mod_mask != 0) {
        if (g_modifiers & mod_mask) {
            g_modifiers &= ~mod_mask;
            g_state_changed = true;
        }
        return;
    }
    
    // Handle Caps Lock specially
    if (hid_code == 0xFC) {
        hid_code = HID_KEY_CAPS_LOCK;
    }
    
    // Find and remove from keys array
    for (int i = 0; i < 6; i++) {
        if (g_keys[i] == hid_code) {
            g_keys[i] = 0;
            g_state_changed = true;
            return;
        }
    }
}

// Handle a complete PS/2 scancode
static void handle_scancode(uint8_t code, bool is_break, bool is_extended) {
    uint8_t hid_code;
    
    if (is_extended) {
        hid_code = extended_scancode_to_hid[code];
    } else {
        hid_code = scancode_to_hid[code];
    }
    
    if (hid_code == 0) {
        // Unknown scancode - ignore
        return;
    }
    
    if (is_break) {
        release_key(hid_code);
    } else {
        press_key(hid_code);
    }
}

//--------------------------------------------------------------------+
// Public Interface
//--------------------------------------------------------------------+

void ps2_init(void) {
    // Initialize clock pin (GP16) with pull-up
    gpio_init(PS2_CLOCK_PIN);
    gpio_set_dir(PS2_CLOCK_PIN, GPIO_IN);
    gpio_pull_up(PS2_CLOCK_PIN);
    
    // Initialize data pin (GP17) with pull-up
    gpio_init(PS2_DATA_PIN);
    gpio_set_dir(PS2_DATA_PIN, GPIO_IN);
    gpio_pull_up(PS2_DATA_PIN);
    
    // Initialize state
    frame_bit_index = 0;
    scancode_byte = 0;
    break_pending = false;
    extended_pending = false;
    last_clk = gpio_get(PS2_CLOCK_PIN);
    
    g_modifiers = 0;
    memset(g_keys, 0, sizeof(g_keys));
    g_state_changed = false;
}

void ps2_task(void) {
    // Read current clock level
    bool clk = gpio_get(PS2_CLOCK_PIN);
    
    // Detect falling edge: previous high (true) -> current low (false)
    if (last_clk && !clk) {
        bool data_bit = gpio_get(PS2_DATA_PIN);
        
        if (frame_bit_index == 0) {
            // Start bit (should be 0, ignore)
        } else if (frame_bit_index >= 1 && frame_bit_index <= 8) {
            // Data bits (LSB first)
            scancode_byte >>= 1;
            if (data_bit) {
                scancode_byte |= 0x80;
            }
        } else if (frame_bit_index == 9) {
            // Parity bit (ignored for now)
        } else if (frame_bit_index == 10) {
            // Stop bit - frame complete
            uint8_t code = scancode_byte;
            
            if (code == 0xF0) {
                // Break prefix
                break_pending = true;
            } else if (code == 0xE0) {
                // Extended prefix
                extended_pending = true;
            } else {
                // Complete scancode received
                handle_scancode(code, break_pending, extended_pending);
                break_pending = false;
                extended_pending = false;
            }
            
            // Reset for next frame
            frame_bit_index = 0;
            scancode_byte = 0;
            
            last_clk = clk;
            return;
        }
        
        frame_bit_index++;
    }
    
    last_clk = clk;
}

uint8_t ps2_get_modifiers(void) {
    return g_modifiers;
}

const uint8_t* ps2_get_keys(void) {
    return g_keys;
}

bool ps2_state_changed(void) {
    return g_state_changed;
}

void ps2_clear_changed(void) {
    g_state_changed = false;
}
