/***************************************************************************//**
 * @file led_controller.h
 * @brief Onboard LED control for Board #2 (Wi-SUN LED receiver).
 ******************************************************************************/

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************//**
 * @brief Initialize the onboard LED driver and force the LED OFF.
 *****************************************************************************/
void led_controller_init(void);

/**************************************************************************//**
 * @brief Turn the LED on.
 *****************************************************************************/
void led_controller_on(void);

/**************************************************************************//**
 * @brief Turn the LED off.
 *****************************************************************************/
void led_controller_off(void);

/**************************************************************************//**
 * @brief Toggle the LED.
 *****************************************************************************/
void led_controller_toggle(void);

/**************************************************************************//**
 * @brief Check whether the LED is currently on.
 *****************************************************************************/
bool led_controller_is_on(void);

/**************************************************************************//**
 * @brief Get the current LED state as a string.
 * @return "ON" or "OFF" (matches the STATUS ACK contract with Board #1).
 *****************************************************************************/
const char *led_controller_state_str(void);

#ifdef __cplusplus
}
#endif

#endif // LED_CONTROLLER_H
