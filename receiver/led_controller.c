/***************************************************************************//**
 * @file led_controller.c
 * @brief Onboard LED control for Board #2 (Wi-SUN LED receiver).
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>

#include "led_controller.h"
#include "sl_simple_led_instances.h"

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

void led_controller_init(void)
{
  sl_simple_led_init_instances();
  /* Force a known state (OFF) at startup regardless of the GPIO's
   * power-on default. */
  sl_led_turn_off(&sl_led_led0);
  printf("[LED] Initialized, state: OFF\n");
}

void led_controller_on(void)
{
  sl_led_turn_on(&sl_led_led0);
  printf("[LED] Turned ON\n");
}

void led_controller_off(void)
{
  sl_led_turn_off(&sl_led_led0);
  printf("[LED] Turned OFF\n");
}

void led_controller_toggle(void)
{
  sl_led_toggle(&sl_led_led0);
  printf("[LED] Toggled, state: %s\n", led_controller_state_str());
}

bool led_controller_is_on(void)
{
  return sl_led_get_state(&sl_led_led0) == SL_LED_CURRENT_STATE_ON;
}

const char *led_controller_state_str(void)
{
  return led_controller_is_on() ? "ON" : "OFF";
}
