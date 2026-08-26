# WI-SUN-led-remote-control

Wi-SUN FAN 1.1 mesh on EFR32ZG28 (BRD2705A): an event-driven UDP command/ACK protocol for remote LED control, running on top of Silicon Labs' Wi-SUN stack, with a standalone SoC Border Router and per-packet RPL link-quality diagnostics (RSSI/ETX).

## What is Wi-SUN, and how does this network actually come up?

Wi-SUN (Wireless Smart Utility Network) is an IPv6-native mesh networking standard for sub-GHz 802.15.4 radios, built for large-scale field-deployed sensor/actuator networks (utility metering, street lighting, industrial IoT). A Wi-SUN **FAN** (Field Area Network) is not a peer-to-peer mesh where any two nodes just talk to each other — it has a strict role hierarchy:

- **Border Router (BR)** — the *only* device that can create a PAN (Personal Area Network). It broadcasts PAN Advertisement frames, runs the 802.1X-style authenticator (EAP-TLS/PSK) that validates every joining node's certificate, runs the DHCPv6 server that hands out the network's global IPv6 prefix, and is the fixed root of the RPL (RFC 6550) DODAG that the whole mesh's routing tree is built from.
- **Router (FFN — Full Function Node)** — joins an existing PAN and can also relay traffic for other nodes once joined. Both application nodes in this repo are Routers.
- **Leaf (LFN — Low-power Function Node)** — joins but never relays; optimized for duty-cycled/battery devices.

A joining node works through a well-defined join-state machine, visible directly in this project's logs:

```
Select PAN → Authenticate → Acquire PAN Config → Configure Routing → DHCP → EARO → DAO → Operational
```

| State | What's happening |
|---|---|
| Select PAN | Node scans channels for a PAN Advertisement whose network name matches its configured one. Only the Border Router transmits these. |
| Authenticate | 802.1X/EAP-TLS handshake against the Border Router's authenticator, using a device certificate chained to a shared trusted CA. |
| Acquire PAN Config | Node receives PAN-wide operating parameters (channel plan, timing, security material) from the Border Router. |
| Configure Routing / DAO | Node attaches to the RPL DODAG rooted at the Border Router and advertises its own reachability upward (Destination Advertisement Object). |
| DHCP | Node is assigned a global IPv6 address out of the Border Router's DHCPv6 pool. |
| Operational | Node has a working global IPv6 address and can send/receive IP traffic across the mesh. |

Two Router nodes **cannot** form a network between themselves — none of the above (PAN creation, authentication, addressing, routing root) is something a Router is capable of doing. This is why this repo includes a Border Router project as first-class infrastructure, not an optional extra.

## What this project does

Three EFR32ZG28 (BRD2705A) roles work together to demonstrate remote actuator control over a real Wi-SUN mesh:

```
 ┌──────────────┐      PAN Advertisement / EAP-TLS / DHCPv6 / RPL      ┌──────────────┐
 │ Border Router │ ───────────────────────────────────────────────────▶│   Receiver   │
 │ (coordinator) │◀───────────────────────────────────────────────────│  (LED node)  │
 └──────┬───────┘                                                      └──────┬───────┘
        │                       UDP :5000, "LED_ON" / "LED_OFF" /             │
        │                       "LED_TOGGLE" / "STATUS" → "*_ACK"             │
        ▼                                                                     ▼
 ┌──────────────┐                                                     onboard LED
 │  Commander   │  (typed serial commands → same UDP payloads)
 └──────────────┘
```

- **`border-router/`** — Silicon Labs' stock `wisun_soc_brcli` sample. Runs entirely on one EFR32 chip (no Linux host, no RCP, no external hardware) and creates the FAN 1.1 PAN both other nodes join. Its own CLI (`wisun udp_client`, `wisun socket_write`) can also be used directly to send the same UDP command payloads the commander sends, which is how this network was first validated end-to-end.
- **`receiver/`** — joins the PAN, obtains a global IPv6 address, opens a UDP server on port 5000 using the raw Wi-SUN socket API (indication-mode sockets, non-blocking), parses `LED_ON` / `LED_OFF` / `LED_TOGGLE` / `STATUS` commands, drives the board's onboard LED via the Simple LED driver, and replies to the sender with the matching `*_ACK` payload (`LED_ON_ACK`, `LED_OFF_ACK`, `LED_TOGGLE_ACK`, `STATUS:ON`/`STATUS:OFF`). On every received packet it also logs RPL neighbor link-quality metrics — RSSI, RSL in/out (EWMA), ETX, routing cost, PAN size — for empirical range testing. Modular: `wisun_node.c/h` (join + IPv6), `udp_server.c/h` (socket + command parsing + link-quality logging), `led_controller.c/h` (Simple LED driver wrapper).
- **Commander** *(to be added)* — a serial-CLI-driven UDP client: types `LED ON` / `LED OFF` / `LED TOGGLE` / `STATUS` / `SET_TARGET <ipv6>` over VCOM, sends the corresponding UDP payload to the receiver, and prints back whatever ACK arrives.

### Design notes

- **Entirely event-driven, no blocking I/O.** Every node registers callbacks via `app_wisun_em_custom_callback_register()` for join-state, connection, and socket-data events, and sockets run in `SL_WISUN_SOCKET_EVENT_MODE_INDICATION` so incoming data arrives as an event body — no blocking `recvfrom()` anywhere in the application loop.
- **Automatic reconnection.** `SL_WISUN_MSG_CONNECTION_LOST_IND_ID` is handled by the SDK's own `sl_wisun_app_core` before any application callback runs, and it already calls `sl_wisun_join()` again — no custom reconnect/reboot logic was written or needed.
- **Bounded, defensive parsing.** Receive buffers are fixed-size and always NUL-terminated; oversized or malformed payloads are truncated/logged rather than causing overflow or a crash.
- **Shared demo PKI.** All nodes are provisioned with the Wi-SUN SDK's built-in demo device certificate and CA (`Wi-SUN Demo Root CA` / `Wi-SUN Demo Device`), so authentication works out of the box across every node in this repo with zero manual certificate provisioning.

## Repo layout

```
.
├── border-router/   Wi-SUN SoC Border Router (wisun_soc_brcli) — creates the PAN
├── receiver/         LED node — UDP server, LED control, link-quality logging
└── commander/         (coming) LED node — UDP client, serial CLI
```

Each folder is a self-contained Simplicity SDK project (its own `.slcp`, `cmake_gcc/`, and a locally vendored subset of `simplicity_sdk_2026.6.1` needed to build it standalone).

## Building

Each project builds independently with CMake + Ninja (no Simplicity Studio GUI or `slc` CLI required once generated):

```bash
cd <project-folder>/cmake_gcc
cmake --workflow --preset project
```

Output artifacts (`.hex`, `.bin`, `.s37`, `.out`) land in `cmake_gcc/build/base/`.

## Bringing the network up (field test)

1. Flash `border-router/` to one board. Over its VCOM console:
   ```
   wisun start_fan11
   wisun get wisun.state        # should report: operational (1)
   ```
2. Flash `receiver/` to a second board and wait for its `Operational` banner and printed global IPv6 address.
3. From the Border Router's CLI, open a UDP connection to that address and drive it directly (useful before the commander firmware is added, or for scripted testing):
   ```
   wisun udp_client <receiver-ipv6> 5000
   wisun socket_write <socket-id> "STATUS"
   ```
4. For range characterization, repeat `STATUS` at increasing physical separation, recording the receiver's per-packet `[LINK]` log line (rssi / rsl_in / rsl_out / etx / routing_cost / pan_size) against the actual measured distance at each step, until the join is lost or ACKs stop arriving.
