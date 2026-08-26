/***************************************************************************//**
 * @file udp_server.c
 * @brief UDP server used by Board #2 to receive LED commands from Board #1
 *        and reply with acknowledgements, over the Wi-SUN raw socket API.
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "udp_server.h"
#include "led_controller.h"
#include "socket/socket.h"
#include "arpa/inet.h"
#include "sl_wisun_event_mgr.h"
#include "sl_wisun_api.h"
#include "sl_wisun_types.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
/// Bounded receive buffer for one UDP command payload (+1 for NUL).
#define UDP_SERVER_RX_BUF_SIZE   64
/// Maximum number of neighbors read for the link-quality diagnostic log.
#define UDP_SERVER_MAX_NEIGHBORS 8

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
static int32_t s_socket_id = SOCKET_INVALID_ID;

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static void on_socket_data(sl_wisun_evt_t *evt);
static void handle_command(const char *command, const sockaddr_in6_t *reply_addr);
static void send_ack(const char *ack, const sockaddr_in6_t *reply_addr);
static void log_link_quality(void);
static const char *neighbor_type_str(uint32_t type);

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

void udp_server_init(void)
{
  sockaddr_in6_t local_addr;

  app_wisun_em_custom_callback_register(SL_WISUN_MSG_SOCKET_DATA_IND_ID, on_socket_data);

  s_socket_id = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (s_socket_id == SOCKET_INVALID_ID) {
    printf("[ERROR] Failed to create UDP server socket\n");
    return;
  }

  memset(&local_addr, 0, sizeof(local_addr));
  local_addr.sin6_family = AF_INET6;
  local_addr.sin6_addr = in6addr_any;
  local_addr.sin6_port = htons(UDP_SERVER_PORT);

  if (bind(s_socket_id, (struct sockaddr *)&local_addr, sizeof(local_addr)) != 0) {
    printf("[ERROR] Failed to bind UDP server socket to port %d\n", UDP_SERVER_PORT);
    return;
  }

  /* The stack's default socket event mode is POLLING, which only signals
   * SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID and requires an explicit
   * recvfrom() call to fetch the payload. Switch to INDICATION mode so
   * the received command payload is delivered directly in the
   * SL_WISUN_MSG_SOCKET_DATA_IND_ID event body handled by
   * on_socket_data() - no blocking recvfrom() anywhere in this app. */
  {
    uint32_t event_mode = SL_WISUN_SOCKET_EVENT_MODE_INDICATION;

    if (setsockopt(s_socket_id, SOL_APPLICATION, SO_EVENT_MODE,
                   &event_mode, sizeof(event_mode)) != 0) {
      printf("[ERROR] Failed to set UDP socket event mode\n");
    }
  }

  printf("[UDP] Server socket created (id=%ld), listening on port %d\n",
         (long)s_socket_id, UDP_SERVER_PORT);
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

static void on_socket_data(sl_wisun_evt_t *evt)
{
  char payload[UDP_SERVER_RX_BUF_SIZE];
  char addr_str[46] = { 0 };
  uint16_t len = evt->evt.socket_data.data_length;
  sockaddr_in6_t reply_addr;

  /* Bound the copy to the local buffer to prevent any overflow regardless
   * of what the remote side sent, and always NUL-terminate. */
  if (len >= sizeof(payload)) {
    printf("[UDP] Received oversized payload (%u bytes), truncating\n", len);
    len = sizeof(payload) - 1;
  }
  memcpy(payload, evt->evt.socket_data.data, len);
  payload[len] = '\0';

  (void)inet_ntop(AF_INET6, &evt->evt.socket_data.remote_address,
                   addr_str, sizeof(addr_str));
  printf("[UDP] Packet received from [%s]:%u, %u bytes: \"%s\"\n",
         addr_str, ntohs(evt->evt.socket_data.remote_port), len, payload);

  /* Build the reply address from the sender's address/port so the ACK
   * goes back to exactly where the command came from. */
  memset(&reply_addr, 0, sizeof(reply_addr));
  reply_addr.sin6_family = AF_INET6;
  reply_addr.sin6_addr = evt->evt.socket_data.remote_address;
  reply_addr.sin6_port = evt->evt.socket_data.remote_port;

  handle_command(payload, &reply_addr);

  /* Diagnostic link-quality snapshot on every received packet, so the
   * range test can be read from this board's own VCOM without needing
   * anything extra on Board #1. */
  log_link_quality();
}

static void handle_command(const char *command, const sockaddr_in6_t *reply_addr)
{
  if (strcmp(command, "LED_ON") == 0) {
    led_controller_on();
    send_ack("LED_ON_ACK", reply_addr);
  } else if (strcmp(command, "LED_OFF") == 0) {
    led_controller_off();
    send_ack("LED_OFF_ACK", reply_addr);
  } else if (strcmp(command, "LED_TOGGLE") == 0) {
    led_controller_toggle();
    send_ack("LED_TOGGLE_ACK", reply_addr);
  } else if (strcmp(command, "STATUS") == 0) {
    char ack[32];
    (void)snprintf(ack, sizeof(ack), "STATUS:%s", led_controller_state_str());
    printf("[APP] STATUS requested, LED is %s\n", led_controller_state_str());
    send_ack(ack, reply_addr);
  } else {
    printf("[APP] Unknown command ignored: \"%s\"\n", command);
  }
}

static void send_ack(const char *ack, const sockaddr_in6_t *reply_addr)
{
  size_t len = strlen(ack);
  ssize_t sent;

  if (s_socket_id == SOCKET_INVALID_ID) {
    printf("[ERROR] UDP socket not available, cannot send ACK\n");
    return;
  }

  sent = sendto(s_socket_id, ack, len, 0,
                (const struct sockaddr *)reply_addr, sizeof(*reply_addr));
  if ((sent < 0) || ((size_t)sent != len)) {
    printf("[ERROR] Failed to send ACK: %s\n", ack);
    return;
  }

  printf("[UDP] ACK sent: %s\n", ack);
}

static void log_link_quality(void)
{
  sl_wisun_mac_address_t neighbors[UDP_SERVER_MAX_NEIGHBORS];
  uint8_t count = UDP_SERVER_MAX_NEIGHBORS;
  sl_status_t status;

  status = sl_wisun_get_neighbors(&count, neighbors);
  if (status != SL_STATUS_OK) {
    printf("[LINK] Failed to read neighbor list (status=0x%04lX)\n",
           (unsigned long)status);
    return;
  }

  printf("[LINK] Neighbor count: %u\n", count);
  for (uint8_t i = 0; i < count; i++) {
    sl_wisun_neighbor_info_t info;

    if (sl_wisun_get_neighbor_info(&neighbors[i], &info) != SL_STATUS_OK) {
      printf("[LINK]   neighbor[%u]: info unavailable\n", i);
      continue;
    }

    printf("[LINK]   neighbor[%u] type=%s rssi=%d dBm rsl_in=%u rsl_out=%u "
           "etx=%u routing_cost=%u pan_size=%u\n",
           i, neighbor_type_str(info.type), info.rssi, info.rsl_in,
           info.rsl_out, info.etx, info.routing_cost, info.pan_size);
  }
}

static const char *neighbor_type_str(uint32_t type)
{
  switch ((sl_wisun_neighbor_type_t)type) {
    case SL_WISUN_NEIGHBOR_TYPE_PRIMARY_PARENT:   return "primary-parent";
    case SL_WISUN_NEIGHBOR_TYPE_SECONDARY_PARENT: return "secondary-parent";
    case SL_WISUN_NEIGHBOR_TYPE_CHILD:            return "child";
    default:                                      return "unknown";
  }
}
