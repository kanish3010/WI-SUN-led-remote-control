/***************************************************************************//**
 * @file wisun_node.h
 * @brief Wi-SUN network initialization, join handling and status helpers
 *        for Board #2 (Wi-SUN LED receiver).
 ******************************************************************************/

#ifndef WISUN_NODE_H
#define WISUN_NODE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************//**
 * @brief Initialize the Wi-SUN node: register event callbacks.
 * @details Must be called once before wisun_node_connect_and_wait().
 *****************************************************************************/
void wisun_node_init(void);

/**************************************************************************//**
 * @brief Start joining the Wi-SUN FAN 1.1 network and block the caller
 *        until the node reaches the Operational join state.
 *****************************************************************************/
void wisun_node_connect_and_wait(void);

/**************************************************************************//**
 * @brief Check whether the node is currently Operational on the network.
 * @return true if connected/operational, false otherwise.
 *****************************************************************************/
bool wisun_node_is_connected(void);

/**************************************************************************//**
 * @brief Get the device's global unicast IPv6 address as a string.
 * @param[out] buf Destination buffer (at least 46 bytes recommended).
 * @param[in]  buf_len Size of the destination buffer.
 * @return true if a valid global address was available and copied.
 *****************************************************************************/
bool wisun_node_get_global_ipv6_str(char *buf, size_t buf_len);

/**************************************************************************//**
 * @brief Get a human readable string of the current Wi-SUN join state.
 * @return Pointer to a static, constant string. Never NULL.
 *****************************************************************************/
const char *wisun_node_join_state_str(void);

#ifdef __cplusplus
}
#endif

#endif // WISUN_NODE_H
