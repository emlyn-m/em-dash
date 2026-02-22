#include <stddef.h>
#include <stdint.h>

#define LOG_DBG "dbg"
#define LOG_INF "inf"
#define LOG_WRN "wrn"
#define LOG_ERR "err"
#define LOG_DBG_C "045"
#define LOG_INF_C "76"
#define LOG_WRN_C "209"
#define LOG_ERR_C "163"
#define LOGFMT(FMT, LVL_NAME, LVL_COLOR) "\x1b[1m\x1b[38;5;" LVL_COLOR "m" LVL_NAME "\x1b[0m " FMT 

#define MAX_RETRIES 5
#define ERR_DPS_LIMIT 1
#define ERR_DPS_NOT_FOUND 2
#define ERR_SOCK_CREATE 3
#define ERR_SOCK_FAIL 4
#define ERR_SOCK_CLOSE 5
#define ERR_ENCODE_FAIL 6

#define TUYA_PORT 6668

#define VERSION_HEADER_SIZE 19
// 19 = 3[3.3] + 16??
#define PREFIX_55AA_VALUE 0x000055AA
#define SUFFIX_VALUE 0x0000AA55

#define COMMAND_CTRL  0x07
#define COMMAND_STATUS 0x08
#define COMMAND_QUERY 0x0a

#define MAX_DPS 10

typedef struct tuya_dp {
    int index;
    void* value;
} tuya_dp_t;

typedef struct tuya_led {
    char* id;
    char* name;
    uint32_t ip;
    unsigned char* key;
    
    int power;
    uint32_t hue;
    uint32_t sat;
    uint32_t val;
    
    int sock;
    uint32_t random_id;
    unsigned int seqno;
} tuya_led_t;

typedef struct _tuya_msg {
    uint32_t seqno;
    uint32_t retcode;
    uint32_t command;
    size_t payload_len;
    unsigned char* payload;
} tuya_msg_t;

void tuya_led_new(tuya_led_t* led, char* id, char* name, uint32_t ip, unsigned char* key);
int _tuya_socket_open(tuya_led_t* led);
void tuya_msg_free(tuya_msg_t* msg);

int tuya_cmd_send(tuya_led_t* led, uint32_t command, char* dps);
int tuya_msg_recv(tuya_led_t *led, uint32_t expected_command, tuya_msg_t* msg);
