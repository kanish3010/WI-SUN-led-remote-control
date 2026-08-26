/***************************************************************************//**
 * @file wisun_node.c
 * @brief Wi-SUN network initialization, join handling and status helpers
 *        for Board #2 (Wi-SUN LED receiver).
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>

#include "wisun_node.h"
#include "sl_wisun_api.h"
#include "sl_wisun_types.h"
#include "sl_wisun_event_mgr.h"
#include "sl_wisun_app_core.h"
#include "sl_wisun_app_core_util.h"
#include "arpa/inet.h"

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static const char *join_state_to_str(sl_wisun_join_state_t state);
static void on_join_state(sl_wisun_evt_t *evt);
static void on_connected(sl_wisun_evt_t *evt);
static void on_disconnected(sl_wisun_evt_t *evt);
static void on_connection_lost(sl_wisun_evt_t *evt);
static void on_error(sl_wisun_evt_t *evt);
static void on_network_update(sl_wisun_evt_t *evt);

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

void wisun_node_init(void)
{
  printf("[WISUN] Initializing Wi-SUN\n");

  /* Custom callbacks run in addition to (after) the SDK's own built-in
   * handlers, which already keep join state / address caches up to date
   * and automatically re-join on connection loss (see on_connection_lost()
   * below). These callbacks only add the application-level logging
   * required for this project. */
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_JOIN_STATE_IND_ID, on_join_state);
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_CONNECTED_IND_ID, on_connected);
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_DISCONNECTED_IND_ID, on_disconnected);
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_CONNECTION_LOST_IND_ID, on_connection_lost);
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_ERROR_IND_ID, on_error);
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_NETWORK_UPDATE_IND_ID, on_network_update);
}

void wisun_node_connect_and_wait(void)
{
  printf("[WISUN] Joining FAN 1.1 network \"Wi-SUN Network\"\n");
  /* Blocks (RTOS wait, not a busy loop) until join state reaches
   * SL_WISUN_JOIN_STATE_OPERATIONAL. */
  sl_wisun_app_core_util_connect_and_wait();
}

bool wisun_node_is_connected(void)
{
  return sl_wisun_app_core_util_network_is_connected();
}

bool wisun_node_get_global_ipv6_str(char *buf, size_t buf_len)
{
  in6_addr_t addr;
  static const in6_addr_t zero_addr = { 0 };

  if ((buf == NULL) || (buf_len == 0)) {
    return false;
  }

  if (sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_GLOBAL, &addr) != SL_STATUS_OK) {
    return false;
  }

  if (memcmp(&addr, &zero_addr, sizeof(addr)) == 0) {
    /* No global address assigned yet */
    return false;
  }

  if (inet_ntop(AF_INET6, &addr, buf, (int)buf_len) == NULL) {
    return false;
  }

  return true;
}

const char *wisun_node_join_state_str(void)
{
  return join_state_to_str(sl_wisun_app_core_get_join_state());
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

static const char *join_state_to_str(sl_wisun_join_state_t state)
{
  switch (state) {
    case SL_WISUN_JOIN_STATE_DISCONNECTED:       return "Disconnected";
    case SL_WISUN_JOIN_STATE_SELECT_PAN:         return "Select PAN";
    case SL_WISUN_JOIN_STATE_AUTHENTICATE:       return "Authenticate";
    case SL_WISUN_JOIN_STATE_ACQUIRE_PAN_CONFIG: return "Acquire PAN Config";
    case SL_WISUN_JOIN_STATE_CONFIGURE_ROUTING:  return "Configure Routing";
    case SL_WISUN_JOIN_STATE_OPERATIONAL:        return "Operational";
    case SL_WISUN_JOIN_STATE_DISCONNECTING:      return "Disconnecting";
    case SL_WISUN_JOIN_STATE_PARENT_SELECT:      return "Parent Select";
    case SL_WISUN_JOIN_STATE_DHCP:               return "DHCP";
    case SL_WISUN_JOIN_STATE_EARO:               return "EARO";
    case SL_WISUN_JOIN_STATE_DAO:                return "DAO";
    default:                                     return "Unknown";
  }
}

static void on_join_state(sl_wisun_evt_t *evt)
{
  sl_wisun_join_state_t state = (sl_wisun_join_state_t)evt->evt.join_state.join_state;

  printf("[WISUN] Join state: %s\n", join_state_to_str(state));

  if (state == SL_WISUN_JOIN_STATE_OPERATIONAL) {
    char addr_str[46] = { 0 };

    /* Board #1's user must type this address into Board #1 via its
     * SET_TARGET command, so make it impossible to miss in the log. */
    printf("\n");
    printf("=================================================================\n");
    printf("[WISUN] Operational - Board #2 is ready to receive commands\n");
    if (wisun_node_get_global_ipv6_str(addr_str, sizeof(addr_str))) {
      printf("[WISUN] >>> IPv6 address: %s <<<\n", addr_str);
      printf("[WISUN] On Board #1, run: SET_TARGET %s\n", addr_str);
    } else {
      printf("[ERROR] Wi-SUN operational but no global IPv6 address available\n");
    }
    printf("=================================================================\n");
    printf("\n");
  }
}

static void on_connected(sl_wisun_evt_t *evt)
{
  printf("[WISUN] Connected event, status: 0x%04lX\n",
         (unsigned long)evt->evt.connected.status);
}

static void on_disconnected(sl_wisun_evt_t *evt)
{
  (void)evt;
  printf("[WISUN] Disconnected from network\n");
}

static void on_connection_lost(sl_wisun_evt_t *evt)
{
  (void)evt;
  /* The SDK's built-in handler (sl_wisun_connection_lost_event_hnd), which
   * runs before this custom callback, already calls sl_wisun_join() again
   * automatically, so no manual reconnect/reboot logic is needed here. */
  printf("[WISUN] Connection lost - automatic rejoin in progress\n");
}

static void on_error(sl_wisun_evt_t *evt)
{
  printf("[ERROR] Wi-SUN stack error, status: 0x%04lX\n",
         (unsigned long)evt->evt.error.status);
}

static void on_network_update(sl_wisun_evt_t *evt)
{
  uint32_t flags = evt->evt.network_update.flags;

  printf("[WISUN] Network update");
  if (flags & (1UL << SL_WISUN_NETWORK_UPDATE_FLAGS_GLOBAL_IP)) {
    printf(" [global IP changed]");
  }
  if (flags & (1UL << SL_WISUN_NETWORK_UPDATE_FLAGS_PRIMARY_PARENT)) {
    printf(" [primary parent changed]");
  }
  if (flags & (1UL << SL_WISUN_NETWORK_UPDATE_FLAGS_SECONDARY_PARENT)) {
    printf(" [secondary parent changed]");
  }
  if (flags & (1UL << SL_WISUN_NETWORK_UPDATE_FLAGS_HOP_COUNT)) {
    printf(" [hop count changed]");
  }
  printf("\n");
}
