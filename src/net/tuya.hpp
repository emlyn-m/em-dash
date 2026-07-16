#pragma once

#include <stddef.h>
#include <stdint.h>

// Tuya local-network protocol for v3.3 / v3.5 smart bulbs. Ported near-verbatim
// from the old project — pure protocol + crypto (shelled out to openssl), no UI
// coupling. Operates on a tuya_led_t; higher-level device control lives in
// net/led.

#define MAX_RETRIES 5
#define ERR_DPS_LIMIT 1
#define ERR_DPS_NOT_FOUND 2
#define ERR_SOCK_CREATE 3
#define ERR_SOCK_FAIL 4
#define ERR_SOCK_CLOSE 5
#define ERR_ENCODE_FAIL 6

#define TUYA_PORT 6668
#define VERSION_HEADER_SIZE 19

#define PREFIX_55AA_VALUE 0x000055AA
#define PREFIX_6699_VALUE 0x00006699
#define SUFFIX_VALUE 0x0000AA55
#define SUFFIX_6699_VALUE 0x00009966

#define COMMAND_CTRL 0x07
#define COMMAND_STATUS 0x08
#define COMMAND_QUERY 0x0a
#define COMMAND_CTRL_NEW 0x0D
#define COMMAND_DP_QUERY_NEW 0x10
#define SESS_KEY_NEG_START 3
#define SESS_KEY_NEG_RESP 4
#define SESS_KEY_NEG_FINISH 5

#define MAX_DPS 10

namespace ui {

typedef struct tuya_dp {
  int index;
  void *value;
} tuya_dp_t;

typedef struct tuya_led {
  char *id;
  char *name;
  uint32_t ip;
  unsigned char *key;
  uint8_t version;  // supported: 33, 35

  int power;
  uint32_t hue;
  uint32_t sat;
  uint32_t val;

  int sock;
  uint32_t random_id;
  unsigned int seqno;
  int failures;

  int graph_control;

  unsigned char sesskey[16];
  int has_sesskey;
} tuya_led_t;

typedef struct _tuya_msg {
  uint32_t seqno;
  uint32_t retcode;
  uint32_t command;
  size_t payload_len;
  unsigned char *payload;
} tuya_msg_t;

void tuya_led_new(tuya_led_t *led, char *id, char *name, uint32_t ip,
                  uint8_t version, unsigned char *key);
int _tuya_socket_open(tuya_led_t *led);
void tuya_msg_free(tuya_msg_t *msg);

// Open a fresh connection (socket, plus session-key handshake for v3.5). Any
// existing socket is closed first. Returns 0 on success.
int tuya_connect(tuya_led_t *led);
// Close the socket and forget the session key.
void tuya_disconnect(tuya_led_t *led);

int tuya_cmd_send(tuya_led_t *led, uint32_t command, char *dps);
// Read exactly one message. Returns 0 (msg populated), ERR_SOCK_CLOSE on peer
// close, or ERR_SOCK_FAIL on error/timeout. Does NOT auto-reconnect — the
// caller owns the connection lifecycle.
int tuya_msg_recv(tuya_led_t *led, uint32_t expected_command, tuya_msg_t *msg);

}  // namespace ui
