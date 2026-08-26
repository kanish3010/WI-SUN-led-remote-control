/***************************************************************************//**
 * @file udp_server.h
 * @brief UDP server used by Board #2 to receive LED commands from Board #1
 *        and reply with acknowledgements, over the Wi-SUN raw socket API.
 ******************************************************************************/

#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/// UDP port this server listens on. Must match Board #1's UDP_CLIENT_TARGET_PORT.
#define UDP_SERVER_PORT   5000

/**************************************************************************//**
 * @brief Create the UDP server socket, bind it to UDP_SERVER_PORT and
 *        register the socket event callback.
 * @details Must be called after the Wi-SUN network is operational.
 *          Non-blocking: uses SL_WISUN_SOCKET_EVENT_MODE_INDICATION so
 *          received datagrams are handled from the Wi-SUN event dispatch
 *          path (no blocking recvfrom()).
 *****************************************************************************/
void udp_server_init(void);

#ifdef __cplusplus
}
#endif

#endif // UDP_SERVER_H
