/*
 * PS/2 to USB HID Keyboard Bridge
 * 
 * Converts PS/2 keyboard input to USB HID keyboard output.
 * Designed for BMC64 compatibility using Boot Keyboard protocol.
 * 
 * Based on TinyUSB HID example by Ha Thach (tinyusb.org)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"
#include "ps2.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void hid_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  
  // Initialize PS/2 keyboard interface
  ps2_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    
    // Poll PS/2 keyboard for incoming scancodes
    ps2_task();
    
    // Send HID reports when needed
    hid_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// Send keyboard HID report with current PS/2 state
static void send_hid_report(void)
{
  // skip if hid is not ready yet
  if ( !tud_hid_ready() ) return;
  
  // Send keyboard report using Boot Keyboard format (no report ID)
  // The tud_hid_keyboard_report function handles this correctly when
  // the descriptor doesn't include a report ID
  tud_hid_keyboard_report(0, ps2_get_modifiers(), (uint8_t*)ps2_get_keys());
}

// Send HID report periodically or when state changes
void hid_task(void)
{
  // Poll every 10ms to send reports
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  // If suspended, don't send reports
  if ( tud_suspended() )
  {
    // Could implement remote wakeup here if needed
    return;
  }

  // Send report if state changed, or periodically for reliability
  if (ps2_state_changed())
  {
    send_hid_report();
    ps2_clear_changed();
  }
}

// Invoked when sent REPORT successfully to host
// For a single keyboard device, we don't need to chain reports
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) report;
  (void) len;
  // Nothing to do - we only have one report type
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  (void) instance;
  (void) report_id;
  
  // For Boot Keyboard, return current keyboard state
  if (report_type == HID_REPORT_TYPE_INPUT)
  {
    if (reqlen >= 8)
    {
      buffer[0] = ps2_get_modifiers();
      buffer[1] = 0; // Reserved
      memcpy(&buffer[2], ps2_get_keys(), 6);
      return 8;
    }
  }

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;
  (void) report_id;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    // For Boot Keyboard without report ID, buffer[0] is the LED state
    if ( bufsize < 1 ) return;

    uint8_t const kbd_leds = buffer[0];

    if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
    {
      // Capslock On: disable blink, turn led on
      blink_interval_ms = 0;
      board_led_write(true);
    }else
    {
      // Caplocks Off: back to normal blink
      board_led_write(false);
      blink_interval_ms = BLINK_MOUNTED;
    }
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
