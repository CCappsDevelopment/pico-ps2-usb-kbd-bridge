/*
 * PS/2 to USB HID Keyboard Bridge
 * PS/2 Decoder Header
 */

#ifndef PS2_H_
#define PS2_H_

#include <stdint.h>
#include <stdbool.h>

// PS/2 Pin Configuration (matching your Python code)
// CLK on GP16 (brown wire), DATA on GP17 (white wire)
#define PS2_CLOCK_PIN  16
#define PS2_DATA_PIN   17

// Initialize PS/2 interface (GPIO pins with pull-ups)
void ps2_init(void);

// Poll PS/2 interface for incoming scancodes
// Call this from the main loop
void ps2_task(void);

// Get current keyboard state for USB HID report
uint8_t ps2_get_modifiers(void);
const uint8_t* ps2_get_keys(void);

// Check if keyboard state has changed since last report
bool ps2_state_changed(void);

// Clear the state changed flag (call after sending HID report)
void ps2_clear_changed(void);

#endif /* PS2_H_ */
