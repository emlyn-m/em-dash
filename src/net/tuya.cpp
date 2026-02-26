#include "src/log.hpp"
#include "src/net/tuya.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <cinttypes>


void tuya_led_new(
    tuya_led_t* led,
    char* id, 
    char* name,
    uint32_t ip,
    unsigned char* key
) {
    led->id = id;
    led->name = name;
    led->ip = ip;
    led->key = key;
    
    led->power = 0;
    led->hue = 0;
    led->sat = 0;
    led->val = 0;
    
    led->seqno = 1;
    led->sock = 0;
    srand(time(0));
    led->random_id = ( rand() % 0xffffffff );
}

void tuya_msg_free(tuya_msg_t* msg) { if (msg && msg->payload) { free(msg->payload); msg->payload = NULL; } }

int _tuya_socket_open(tuya_led_t* led) {
    LOG(PRI_DBG, "connecting to led socket...\n");
    int status, sock;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        close(sock);
        return ERR_SOCK_CREATE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = led->ip;
    serv_addr.sin_port = htons(TUYA_PORT);
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    if ((status = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) { close(sock); return ERR_SOCK_FAIL; }
    LOG(PRI_DBG, "connection success!\n");
    led->sock = sock;
    return 0;
}

static uint32_t _crc32(unsigned char *buf, size_t len) {
    uint32_t crc = ~0xffffffff;
    int k;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++) { crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320 : (crc >> 1); }
    }
    return ~crc;
}

unsigned char* _pad(unsigned char* buf, int len, int target_len, int* out_len) {
    int padnum = target_len - len % target_len;
    LOG(PRI_DBG, "padding %d bytes for a length of %d\n", padnum, len+padnum);
    *out_len = len + padnum;
    unsigned char* output_buf;
    while (!(output_buf = (unsigned char*) malloc(len + padnum))) {};
    memcpy(output_buf, buf, len);
    for (int i=0; i < padnum; i++) {
        output_buf[len+i] = padnum;
    }
    return output_buf;
}

int _unpad(unsigned char* ibuf, unsigned char* obuf, size_t ibuf_len) {
    unsigned char padding_char = ibuf[ibuf_len - 1];
    memcpy(obuf, ibuf, ibuf_len - padding_char);
    return ibuf_len - padding_char;
}

int _popen_crypt(unsigned char* pt_buf, int pt_len, unsigned char* key, unsigned char* ct_buf, int decrypt) {
    uint64_t key_low = 0, key_high = 0;
    for (int i=0; i < 8; i++) {
        key_high += ((uint64_t) key[i]) << 8*(7-i); key_low += ((uint64_t) key[8+i]) << 8*(7-i);
    }
    char cmdbuf[679]; memset(cmdbuf, 0, 679);
    char hexbuf[512]; memcpy(hexbuf, pt_buf, pt_len); hexbuf[511] = 0;
    for (int i=0; i < pt_len; i++) {
        if (!(i%30) && i) { hexbuf[2*(i+(i/30)-1)] = '\\'; hexbuf[2*(i+(i/30)-1)+1] = 'n'; }
        snprintf(hexbuf+(2*(i+i/30)), 3, "%02x", pt_buf[i]);
    }
    snprintf(cmdbuf, 679, "echo -en '%s' | xxd -r -p -c30 | openssl enc %s -nopad -aes-128-ecb -nosalt -K %016" PRIx64 "%016" PRIx64 " -in /dev/stdin -out /dev/stdout | xxd -p | tr -d \'\\n\'", hexbuf, decrypt ? "-d" : "-e", key_high, key_low);
    LOG(PRI_DBG, "cmd: <%s>\n", cmdbuf);
    FILE* enc_fp = popen(cmdbuf, "r");
    if (!enc_fp) { return 0; }
    
    char ct_hexbuf[512]; memset(ct_hexbuf, 0, 2*pt_len+1);
    int ct_hlen = fread(ct_hexbuf, 1, 512, enc_fp);
    pclose(enc_fp);
    if (ct_hlen <= 0) { 
        LOG(PRI_WRN, "_popen_crypt read 0 bytes\n");
        return 0;
    } else {
        LOG(PRI_DBG, "_popen_crypt read %d bytes: ", ct_hlen);
    }
    
    for (int i=0; i < ct_hlen - 1; i+=2) {
        sscanf(ct_hexbuf+i, "%02hhx", &(ct_buf[i / 2]));
        printf("%02hhx", ct_buf[i / 2]);
    }
    printf("\n");
    return ct_hlen/2;
}

int _encrypt(unsigned char* pt_buf, int pt_len, unsigned char* key, unsigned char* ct_buf) { return _popen_crypt(pt_buf, pt_len, key, ct_buf, 0); }
int _decrypt(unsigned char* pt_buf, int pt_len, unsigned char* key, unsigned char* ct_buf) { return _popen_crypt(pt_buf, pt_len, key, ct_buf, 1); }

void _pack_u32_be(uint32_t value, uint8_t* buf) {
    buf[0] = (uint8_t)((value >> 24) & 0xFF);
    buf[1] = (uint8_t)((value >> 16) & 0xFF);
    buf[2] = (uint8_t)((value >> 8) & 0xFF);
    buf[3] = (uint8_t)(value & 0xFF);
}

void _unpack_u32_be(uint8_t* buf, uint32_t* value) {
    *value = 0;
    *value += buf[3];
    *value += buf[2] << 8;
    *value += buf[1] << 16;
    *value += buf[0] << 24;
}

unsigned char* _tuya_payload_encode(tuya_led_t* led, uint32_t command, unsigned char* payload, unsigned char* header, uint32_t* msg_size) {
    int encrypt = 1;
    
    int true_msg_size = *msg_size;
    unsigned char *ct_buf, *padded;
    if (encrypt) {
        padded = _pad(payload, *msg_size, 16, &true_msg_size);
        if (!padded) {
            return NULL;
        }
    }
    ct_buf = (unsigned char*) malloc(true_msg_size + (header ? 15 : 0));
    if (!ct_buf) { return NULL; }
    memset(ct_buf, 0, true_msg_size + (header ? 15 : 0));
    unsigned char* ct_buf_write = ct_buf;
    if (header) {
        memcpy(ct_buf, header, 15);
        ct_buf_write += 15;
    }
    int ct_len;
    if (encrypt) { ct_len = _encrypt((unsigned char*) padded, true_msg_size, (unsigned char*) led->key, ct_buf_write); free(padded); }
    else { ct_len = true_msg_size; memcpy(ct_buf, payload, true_msg_size); }
    if (header) { ct_len += 15; }

    *msg_size = 16 + ct_len + 8;  // 16 byte header, 8 byte tail 
    
    uint8_t* msg_buf = (uint8_t*) malloc(*msg_size);
    if (!msg_buf) { return NULL; }
    _pack_u32_be(PREFIX_55AA_VALUE, msg_buf);
    _pack_u32_be(led->seqno, msg_buf+4);
    led->seqno++;
    _pack_u32_be(command, msg_buf+8);
    _pack_u32_be(*msg_size - 16, msg_buf+12);

    memcpy((char*) msg_buf+16, (char*) ct_buf, ct_len);
    uint8_t* tail_ptr = msg_buf+16+ct_len;
    free(ct_buf);

    _pack_u32_be(_crc32(msg_buf, tail_ptr - msg_buf), tail_ptr);
    _pack_u32_be(SUFFIX_VALUE, tail_ptr+4);

    return msg_buf;
}

void _tuya_generate_header(unsigned char* buf) {
    buf[0] = '3';
    buf[1] = '.';
    buf[2] = '3';
    _pack_u32_be(0, buf+3);
}

int tuya_cmd_send(tuya_led_t* led, uint32_t command, char* dps) {
    printf("\n");
    unsigned char payload[256];
    uint32_t msg_size = snprintf((char*) payload, 256, "{\"gwId\":\"%s\",\"devId\":\"%s\",\"uid\":\"%s\",\"t\":\"%d\"", led->id, led->id, led->id, (int) time(NULL));
    if (dps) { msg_size += snprintf((char*) payload + msg_size, 256 - msg_size, ",\"dps\":%s}",  dps ? dps : "{}"); }
    else { msg_size += snprintf((char*) payload + msg_size, 256 - msg_size, "}"); }
    
    LOG(PRI_DBG, "header (%zu bytes): %s\n", strlen((char*) payload), payload);
    unsigned char header_buf[VERSION_HEADER_SIZE] = { 0 };
    unsigned char* header = header_buf;
    if (!( ( command == COMMAND_QUERY) )) { _tuya_generate_header(header); }
    else { header = NULL; }
    uint8_t* encoded = _tuya_payload_encode(led, command, payload, header, &msg_size);
    if (!encoded) { return ERR_ENCODE_FAIL; }
    
    LOG(PRI_DBG, "encoded (%d bytes): ", msg_size);
    for (size_t i=0; i < msg_size; i++) { printf("%02x", encoded[i]); }
    printf("\n");
    
    int success = send(led->sock, encoded, msg_size, 0);
    int retries = 0;
    while (retries <= MAX_RETRIES && success <= 0) {
        LOG(PRI_WRN, "send() in tuya_cmd_send returned %d\n", errno);
        if (errno) {
            int sock_open_status;
            if (( sock_open_status = _tuya_socket_open(led) )) { LOG(PRI_ERR, "failed to open socket: %d\n", sock_open_status); };
        }
        success = send(led->sock, encoded, msg_size, 0);
    }
    if (success < 0) { return ERR_SOCK_FAIL; }
    
    LOG(PRI_INF, "sent %d bytes\n", success);
    free(encoded);
    return 0;
}

typedef struct _tuya_header {
    uint32_t seqno;
    uint32_t command;
    uint32_t payload_len;
    uint32_t total_len;
} _tuya_header_t;
_tuya_header_t _tuya_header_parse(unsigned char* header_buf, size_t header_size) {
    _tuya_header_t header;
    const uint32_t header_len = 16;
    uint32_t prefix; _unpack_u32_be(header_buf, &prefix);
    if (prefix != PREFIX_55AA_VALUE) { LOG(PRI_WRN, "unknown prefix %d\n", prefix); }
    else { LOG(PRI_DBG, "received 55AA prefix\n"); }
    
    _unpack_u32_be(header_buf+4, &header.seqno);
    _unpack_u32_be(header_buf+8, &header.command);
    _unpack_u32_be(header_buf+12, &header.payload_len);
    header.total_len = header_len + header.payload_len;
        
    return header;
}

void _tuya_payload_decode(tuya_led_t* led, uint32_t expected_command, unsigned char* encoded, tuya_msg_t* msg) {
    const uint32_t end_len = 8;
    const uint32_t header_len = 16;
    const uint32_t retcode_len = ((msg->command == expected_command)) ? 4 : 0;
    const uint32_t version_header_len = ((msg->command == COMMAND_STATUS)) ? VERSION_HEADER_SIZE : 0;

    size_t ct_offset = header_len + retcode_len + version_header_len;
    int ct_len = msg->payload_len - retcode_len - end_len - version_header_len;
    if (ct_len <= 0) { msg->payload_len = 0; return; }
    unsigned char ct[msg->payload_len]; memset(ct, 0, msg->payload_len);
    msg->payload = (unsigned char*) malloc(ct_len + 1); memset(msg->payload, 0, ct_len + 1);

    if (retcode_len) { _unpack_u32_be(encoded+header_len, &(msg->retcode)); }
    else { msg->retcode = -1; }
    
    unsigned char padded[ct_len]; memset(padded, 0, ct_len);
    LOG(PRI_DBG, "using range %zu to %zu of payload size %zu\n", ct_offset, ct_offset + ct_len, msg->payload_len);
    memcpy(ct, encoded + ct_offset, ct_len);
    if ((msg->payload_len = _decrypt(ct, ct_len, led->key, padded)) <= 0) {
        msg->payload_len = 0;
        return;
    };
    
    msg->payload_len = _unpad(padded, msg->payload, ct_len);
}


int tuya_msg_recv(tuya_led_t *led, uint32_t expected_command, tuya_msg_t* msg) {
    const uint32_t BUFSIZE = 1024;
    printf("\n");
    unsigned char output_buf[BUFSIZE]; memset(output_buf, 0, BUFSIZE);
    const uint32_t min_len = 16 + 4;
    
    int header_len = recv(led->sock, output_buf, min_len, 0);
    int retries = 0;
    while (retries <= MAX_RETRIES && header_len <= 0) {
        LOG(PRI_WRN, "recv() in tuya_cmd_send returned %d\n", errno);
        if (errno) {
            int sock_open_status;
            if (( sock_open_status = _tuya_socket_open(led) )) { LOG(PRI_ERR, "failed to open socket: %d\n", sock_open_status); };
        }
        header_len = recv(led->sock, output_buf, min_len, 0);
    }
    if (header_len < 0) { LOG(PRI_DBG, "rx header failed!\n"); return ERR_SOCK_FAIL; }
    else if (header_len == 0) { LOG(PRI_DBG, "rx closed\n"); return ERR_SOCK_CLOSE; }
    
    _tuya_header_t header = _tuya_header_parse(output_buf, (size_t) header_len);
    uint32_t remaining = header.total_len - header_len;
    if (remaining <= 0) { return 0; }
    if (msg) {
        msg->seqno = header.seqno;
        msg->command = header.command;
        msg->payload_len = remaining;
    }
    LOG(PRI_DBG, "rx %d byte initial: ", header_len);
    for (int i=0; i < header_len; i++) { printf("%02x", (unsigned char) output_buf[i]); }
    printf("\n");
    
    LOG(PRI_DBG, "rx remaining (expecting %u bytes)... ", header.total_len - header_len); fflush(stdout);
    size_t body_rx = recv(led->sock, output_buf + header_len, remaining, 0);
    if (body_rx < 0) { LOG(PRI_WRN, "\nfailed!\n"); return ERR_SOCK_FAIL; }
    else if (body_rx == 0) { LOG(PRI_WRN, "\nclosed!\n"); return ERR_SOCK_CLOSE; }
    printf("%zu bytes: ", body_rx);
    for (size_t i=0; i < body_rx; i++) { printf("%02x", (unsigned char) (output_buf + header_len)[i]); }
    printf("\n");
    
    if (msg) {
        msg->payload_len = header.payload_len;
        _tuya_payload_decode(led, expected_command, output_buf, msg);
    }
    return 0;
}
