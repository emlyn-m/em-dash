#include "net/tuya.hpp"
#include "log.hpp"

#include <arpa/inet.h>
#include <cinttypes>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

namespace ui {

void tuya_led_new(tuya_led_t *led, char *id, char *name, uint32_t ip,
                  uint8_t version, unsigned char *key) {
  led->id = id;
  led->name = name;
  led->ip = ip;
  led->version = version;
  led->key = key;

  led->power = 0;
  led->hue = 0;
  led->sat = 0;
  led->val = 0;

  led->failures = 1;
  led->seqno = 1;
  led->sock = 0;
  srand(time(0));
  led->random_id = (rand() % 0xffffffff);

  led->graph_control = 0;

  led->has_sesskey = 0;
  memset(led->sesskey, 0, 16);
}

void tuya_msg_free(tuya_msg_t *msg) {
  if (msg && msg->payload) {
    free(msg->payload);
    msg->payload = NULL;
  }
}

int _tuya_socket_open(tuya_led_t *led) {
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

  if ((status = connect(sock, (struct sockaddr *)&serv_addr,
                        sizeof(serv_addr))) < 0) {
    close(sock);
    return ERR_SOCK_FAIL;
  }
  led->sock = sock;
  return 0;
}

static uint32_t _crc32(unsigned char *buf, size_t len) {
  uint32_t crc = ~0xffffffff;
  int k;
  while (len--) {
    crc ^= *buf++;
    for (k = 0; k < 8; k++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320 : (crc >> 1);
    }
  }
  return ~crc;
}

unsigned char *_pad(unsigned char *buf, int len, int target_len, int *out_len) {
  int padnum = target_len - len % target_len;
  *out_len = len + padnum;
  unsigned char *output_buf;
  while (!(output_buf = (unsigned char *)malloc(len + padnum))) {
  };
  memcpy(output_buf, buf, len);
  for (int i = 0; i < padnum; i++) {
    output_buf[len + i] = padnum;
  }
  return output_buf;
}

int _unpad(unsigned char *ibuf, unsigned char *obuf, size_t ibuf_len) {
  unsigned char padding_char = ibuf[ibuf_len - 1];
  memcpy(obuf, ibuf, ibuf_len - padding_char);
  return ibuf_len - padding_char;
}

int _popen_crypt(unsigned char *pt_buf, int pt_len, unsigned char *key,
                 unsigned char *ct_buf, int decrypt) {
  uint64_t key_low = 0, key_high = 0;
  for (int i = 0; i < 8; i++) {
    key_high += ((uint64_t)key[i]) << 8 * (7 - i);
    key_low += ((uint64_t)key[8 + i]) << 8 * (7 - i);
  }
  char cmdbuf[679];
  memset(cmdbuf, 0, 679);
  char hexbuf[512];
  memcpy(hexbuf, pt_buf, pt_len);
  hexbuf[511] = 0;
  for (int i = 0; i < pt_len; i++) {
    if (!(i % 30) && i) {
      hexbuf[2 * (i + (i / 30) - 1)] = '\\';
      hexbuf[2 * (i + (i / 30) - 1) + 1] = 'n';
    }
    snprintf(hexbuf + (2 * (i + i / 30)), 3, "%02x", pt_buf[i]);
  }
  snprintf(cmdbuf, 679,
           "echo -en '%s' | xxd -r -p -c30 | openssl enc %s -nopad "
           "-aes-128-ecb -nosalt -K %016" PRIx64 "%016" PRIx64
           " -in /dev/stdin -out /dev/stdout | xxd -p | tr -d \'\\n\'",
           hexbuf, decrypt ? "-d" : "-e", key_high, key_low);
  FILE *enc_fp = popen(cmdbuf, "r");
  if (!enc_fp) {
    return 0;
  }

  char ct_hexbuf[512];
  memset(ct_hexbuf, 0, 2 * pt_len + 1);
  int ct_hlen = fread(ct_hexbuf, 1, 512, enc_fp);
  pclose(enc_fp);
  if (ct_hlen <= 0) {
    LOG(PRI_WRN, "_popen_crypt read 0 bytes\n");
    return 0;
  }

  for (int i = 0; i < ct_hlen - 1; i += 2) {
    sscanf(ct_hexbuf + i, "%02hhx", &(ct_buf[i / 2]));
  }
  return ct_hlen / 2;
}

int _encrypt(unsigned char *pt_buf, int pt_len, unsigned char *key,
             unsigned char *ct_buf) {
  return _popen_crypt(pt_buf, pt_len, key, ct_buf, 0);
}
int _decrypt(unsigned char *pt_buf, int pt_len, unsigned char *key,
             unsigned char *ct_buf) {
  return _popen_crypt(pt_buf, pt_len, key, ct_buf, 1);
}

void _pack_u32_be(uint32_t value, uint8_t *buf) {
  buf[0] = (uint8_t)((value >> 24) & 0xFF);
  buf[1] = (uint8_t)((value >> 16) & 0xFF);
  buf[2] = (uint8_t)((value >> 8) & 0xFF);
  buf[3] = (uint8_t)(value & 0xFF);
}

void _unpack_u32_be(uint8_t *buf, uint32_t *value) {
  *value = 0;
  *value += buf[3];
  *value += buf[2] << 8;
  *value += buf[1] << 16;
  *value += buf[0] << 24;
}

typedef struct _tuya_header {
  uint32_t seqno;
  uint32_t command;
  uint32_t payload_len;
  uint32_t total_len;
} _tuya_header_t;

static void _key_to_hex(const unsigned char *key, int key_len, char *hex) {
  for (int i = 0; i < key_len; i++)
    snprintf(hex + 2 * i, 3, "%02x", key[i]);
  hex[2 * key_len] = '\0';
}

static int _popen_ctr(unsigned char *pt_buf, int pt_len, unsigned char *key,
                      unsigned char *iv_128, unsigned char *ct_buf) {
  char key_hex[33], iv_hex[33];
  _key_to_hex(key, 16, key_hex);
  _key_to_hex(iv_128, 16, iv_hex);

  char hexbuf[600];
  memset(hexbuf, 0, sizeof(hexbuf));
  for (int i = 0; i < pt_len; i++) {
    if (!(i % 30) && i) {
      hexbuf[2 * (i + i / 30 - 1)] = '\\';
      hexbuf[2 * (i + i / 30 - 1) + 1] = 'n';
    }
    snprintf(hexbuf + 2 * (i + i / 30), 3, "%02x", pt_buf[i]);
  }
  char cmdbuf[900];
  memset(cmdbuf, 0, sizeof(cmdbuf));
  snprintf(cmdbuf, sizeof(cmdbuf),
           "echo -en '%s' | xxd -r -p -c30 | openssl enc -e -aes-128-ctr "
           "-nosalt -nopad -K %s -iv %s -in /dev/stdin -out /dev/stdout"
           " | xxd -p | tr -d '\\n'",
           hexbuf, key_hex, iv_hex);
  FILE *fp = popen(cmdbuf, "r");
  if (!fp)
    return 0;
  char ct_hex[600];
  memset(ct_hex, 0, sizeof(ct_hex));
  int hlen = fread(ct_hex, 1, sizeof(ct_hex) - 1, fp);
  pclose(fp);
  if (hlen <= 0) {
    LOG(PRI_WRN, "_popen_ctr read 0 bytes\n");
    return 0;
  }
  for (int i = 0; i < hlen - 1; i += 2)
    sscanf(ct_hex + i, "%02hhx", &ct_buf[i / 2]);
  return hlen / 2;
}

static int _popen_hmac_sha256(unsigned char *data, int data_len,
                              unsigned char *key, int key_len,
                              unsigned char *out) {
  char key_hex[65];
  _key_to_hex(key, key_len, key_hex);
  char hexbuf[150];
  memset(hexbuf, 0, sizeof(hexbuf));
  for (int i = 0; i < data_len; i++) {
    if (!(i % 30) && i) {
      hexbuf[2 * (i + i / 30 - 1)] = '\\';
      hexbuf[2 * (i + i / 30 - 1) + 1] = 'n';
    }
    snprintf(hexbuf + 2 * (i + i / 30), 3, "%02x", data[i]);
  }
  char cmdbuf[400];
  memset(cmdbuf, 0, sizeof(cmdbuf));
  snprintf(cmdbuf, sizeof(cmdbuf),
           "echo -en '%s' | xxd -r -p -c30 | openssl dgst -sha256 -mac HMAC "
           "-macopt hexkey:%s -binary | xxd -p | tr -d '\\n'",
           hexbuf, key_hex);
  FILE *fp = popen(cmdbuf, "r");
  if (!fp)
    return 0;
  char out_hex[70];
  memset(out_hex, 0, sizeof(out_hex));
  int hlen = fread(out_hex, 1, sizeof(out_hex) - 1, fp);
  pclose(fp);
  if (hlen <= 0) {
    LOG(PRI_WRN, "_popen_hmac_sha256 read 0 bytes\n");
    return 0;
  }
  for (int i = 0; i < hlen - 1; i += 2)
    sscanf(out_hex + i, "%02hhx", &out[i / 2]);
  return hlen / 2;
}

static void _gcm_mult(const uint8_t *X, const uint8_t *Y, uint8_t *out) {
  uint8_t Z[16] = {0}, V[16];
  memcpy(V, X, 16);
  for (int i = 0; i < 128; i++) {
    if (Y[i / 8] & (0x80 >> (i % 8)))
      for (int k = 0; k < 16; k++)
        Z[k] ^= V[k];
    uint8_t lsb = V[15] & 1;
    for (int k = 15; k > 0; k--)
      V[k] = (V[k] >> 1) | ((V[k - 1] & 1) << 7);
    V[0] >>= 1;
    if (lsb)
      V[0] ^= 0xe1;
  }
  memcpy(out, Z, 16);
}

static void _ghash(uint8_t *H, uint8_t *aad, int aad_len, uint8_t *ct,
                   int ct_len, uint8_t *out) {
  uint8_t X[16] = {0}, block[16];
  for (int i = 0; i * 16 < aad_len; i++) {
    memset(block, 0, 16);
    int n = aad_len - i * 16;
    memcpy(block, aad + i * 16, n < 16 ? n : 16);
    for (int j = 0; j < 16; j++)
      X[j] ^= block[j];
    _gcm_mult(X, H, X);
  }
  for (int i = 0; i * 16 < ct_len; i++) {
    memset(block, 0, 16);
    int n = ct_len - i * 16;
    memcpy(block, ct + i * 16, n < 16 ? n : 16);
    for (int j = 0; j < 16; j++)
      X[j] ^= block[j];
    _gcm_mult(X, H, X);
  }
  memset(block, 0, 16);
  uint64_t ab = (uint64_t)aad_len * 8, cb = (uint64_t)ct_len * 8;
  for (int i = 0; i < 8; i++) {
    block[7 - i] = ab & 0xff;
    ab >>= 8;
    block[15 - i] = cb & 0xff;
    cb >>= 8;
  }
  for (int j = 0; j < 16; j++)
    X[j] ^= block[j];
  _gcm_mult(X, H, X);
  memcpy(out, X, 16);
}

static int _gcm_encrypt(unsigned char *pt, int pt_len, unsigned char *key,
                        unsigned char *iv, unsigned char *aad, int aad_len,
                        unsigned char *ct_and_tag) {
  uint8_t H[16] = {0}, zero[16] = {0}, J0[16], EK0[16], ctr[16], S[16];
  if (!_encrypt(zero, 16, key, H))
    return 0;
  memcpy(J0, iv, 12);
  J0[12] = 0;
  J0[13] = 0;
  J0[14] = 0;
  J0[15] = 1;
  if (!_encrypt(J0, 16, key, EK0))
    return 0;
  memcpy(ctr, iv, 12);
  ctr[12] = 0;
  ctr[13] = 0;
  ctr[14] = 0;
  ctr[15] = 2;
  if (!_popen_ctr(pt, pt_len, key, ctr, ct_and_tag))
    return 0;
  _ghash(H, aad, aad_len, ct_and_tag, pt_len, S);
  for (int i = 0; i < 16; i++)
    ct_and_tag[pt_len + i] = S[i] ^ EK0[i];
  return pt_len;
}

static int _gcm_decrypt(unsigned char *ct, int ct_len, unsigned char *key,
                        unsigned char *iv, unsigned char *aad, int aad_len,
                        unsigned char *tag, unsigned char *pt) {
  uint8_t H[16] = {0}, zero[16] = {0}, J0[16], EK0[16], S[16], ctr[16];
  if (!_encrypt(zero, 16, key, H))
    return 0;
  memcpy(J0, iv, 12);
  J0[12] = 0;
  J0[13] = 0;
  J0[14] = 0;
  J0[15] = 1;
  if (!_encrypt(J0, 16, key, EK0))
    return 0;
  _ghash(H, aad, aad_len, ct, ct_len, S);
  for (int i = 0; i < 16; i++) {
    if ((S[i] ^ EK0[i]) != tag[i]) {
      LOG(PRI_WRN, "_gcm_decrypt: tag mismatch\n");
      return 0;
    }
  }
  memcpy(ctr, iv, 12);
  ctr[12] = 0;
  ctr[13] = 0;
  ctr[14] = 0;
  ctr[15] = 2;
  if (!_popen_ctr(ct, ct_len, key, ctr, pt))
    return 0;
  return ct_len;
}

static _tuya_header_t _tuya_35_header_parse(unsigned char *buf) {
  _tuya_header_t header;
  uint32_t prefix;
  _unpack_u32_be(buf, &prefix);
  if (prefix != PREFIX_6699_VALUE)
    LOG(PRI_WRN, "_tuya_35_header_parse: unexpected prefix %08x\n", prefix);
  _unpack_u32_be(buf + 6, &header.seqno);
  _unpack_u32_be(buf + 10, &header.command);
  _unpack_u32_be(buf + 14, &header.payload_len);
  header.total_len = 18 + header.payload_len + 4; // +4 for suffix
  return header;
}

static uint8_t *_tuya_35_pack(tuya_led_t *led, uint32_t cmd,
                              unsigned char *payload, uint32_t payload_len,
                              uint32_t *out_size) {
  uint32_t msg_len = payload_len + 28;
  *out_size = 18 + payload_len + 28 + 4;
  uint8_t *buf = (uint8_t *)malloc(*out_size);
  if (!buf)
    return NULL;
  memset(buf, 0, *out_size);

  _pack_u32_be(PREFIX_6699_VALUE, buf);
  buf[4] = 0;
  buf[5] = 0; // unk
  _pack_u32_be(led->seqno++, buf + 6);
  _pack_u32_be(cmd, buf + 10);
  _pack_u32_be(msg_len, buf + 14);

  unsigned char iv[12];
  for (int i = 0; i < 12; i++)
    iv[i] = (unsigned char)(rand() & 0xff);
  memcpy(buf + 18, iv, 12);

  unsigned char *enc_key = led->has_sesskey ? led->sesskey : led->key;
  if (!_gcm_encrypt(payload, (int)payload_len, enc_key, iv, buf + 4, 14,
                    buf + 30)) {
    free(buf);
    return NULL;
  }
  _pack_u32_be(SUFFIX_6699_VALUE, buf + 30 + payload_len + 16);
  return buf;
}

static int _tuya_35_kex(tuya_led_t *led);
static int _tuya_35_ensure_connected(tuya_led_t *led) {
  if (led->sock > 0 && led->has_sesskey)
    return 0;
  led->has_sesskey = 0;
  int status = _tuya_socket_open(led);
  if (status)
    return status;
  return _tuya_35_kex(led);
}

static int _tuya_35_kex(tuya_led_t *led) {
  unsigned char local_nonce[16];
  memcpy(local_nonce, "0123456789abcdef", 16);

  uint32_t s1_size;
  uint8_t *s1 = _tuya_35_pack(led, SESS_KEY_NEG_START,
                              (unsigned char *)local_nonce, 16, &s1_size);
  if (!s1)
    return ERR_ENCODE_FAIL;
  int sent = send(led->sock, s1, s1_size, 0);
  free(s1);
  if (sent <= 0)
    return ERR_SOCK_FAIL;

  unsigned char hdr_buf[18];
  if (recv(led->sock, hdr_buf, 18, 0) != 18)
    return ERR_SOCK_FAIL;
  _tuya_header_t resp_hdr = _tuya_35_header_parse(hdr_buf);
  if (resp_hdr.command != SESS_KEY_NEG_RESP) {
    LOG(PRI_ERR, "_tuya_35_kex: unexpected cmd %u\n", resp_hdr.command);
    return ERR_ENCODE_FAIL;
  }

  uint32_t body_len = resp_hdr.payload_len + 4;
  unsigned char *body = (unsigned char *)malloc(body_len);
  if (!body)
    return ERR_ENCODE_FAIL;
  if (recv(led->sock, body, body_len, 0) != (int)body_len) {
    free(body);
    return ERR_SOCK_FAIL;
  }

  int ct_len = (int)resp_hdr.payload_len - 28;
  if (ct_len < 52) {
    LOG(PRI_ERR, "_tuya_35_kex: step2 too short (%d)\n", ct_len);
    free(body);
    return ERR_ENCODE_FAIL;
  }
  unsigned char *pt = (unsigned char *)malloc(ct_len + 1);
  if (!pt) {
    free(body);
    return ERR_ENCODE_FAIL;
  }
  memset(pt, 0, ct_len + 1);

  if (!_gcm_decrypt(body + 12, ct_len, led->key, body, hdr_buf + 4, 14,
                    body + 12 + ct_len, pt)) {
    LOG(PRI_ERR, "_tuya_35_kex: step2 decrypt failed\n");
    free(pt);
    free(body);
    return ERR_ENCODE_FAIL;
  }
  free(body);

  unsigned char *remote_nonce = pt + 4;
  unsigned char *device_hmac = pt + 20;

  unsigned char our_hmac[32];
  if (_popen_hmac_sha256((unsigned char *)local_nonce, 16, led->key, 16,
                         our_hmac) != 32) {
    LOG(PRI_ERR, "_tuya_35_kex: HMAC compute failed\n");
    free(pt);
    return ERR_ENCODE_FAIL;
  }
  if (memcmp(our_hmac, device_hmac, 32) != 0) {
    LOG(PRI_ERR, "_tuya_35_kex: device HMAC mismatch\n");
    free(pt);
    return ERR_ENCODE_FAIL;
  }

  unsigned char key_override[16];
  for (int i = 0; i < 16; i++)
    key_override[i] = local_nonce[i] ^ remote_nonce[i];
  uint8_t ctr_block[16], ecb_out[16];
  memcpy(ctr_block, local_nonce, 12);
  ctr_block[12] = 0;
  ctr_block[13] = 0;
  ctr_block[14] = 0;
  ctr_block[15] = 2;
  if (!_encrypt(ctr_block, 16, led->key, ecb_out)) {
    free(pt);
    return ERR_ENCODE_FAIL;
  }
  for (int i = 0; i < 16; i++)
    led->sesskey[i] = ecb_out[i] ^ key_override[i];

  unsigned char finish_hmac[32];
  if (_popen_hmac_sha256(remote_nonce, 16, led->key, 16, finish_hmac) != 32) {
    LOG(PRI_ERR, "_tuya_35_kex: finish HMAC failed\n");
    free(pt);
    return ERR_ENCODE_FAIL;
  }
  free(pt);

  uint32_t s3_size;
  uint8_t *s3 =
      _tuya_35_pack(led, SESS_KEY_NEG_FINISH, finish_hmac, 32, &s3_size);
  if (!s3)
    return ERR_ENCODE_FAIL;
  sent = send(led->sock, s3, s3_size, 0);
  free(s3);
  if (sent <= 0)
    return ERR_SOCK_FAIL;

  led->has_sesskey = 1;
  LOG(PRI_INF, "_tuya_35_kex: session key established\n");
  return 0;
}

static int _tuya_35_cmd_send(tuya_led_t *led, uint32_t command, char *dps) {
  if (_tuya_35_ensure_connected(led))
    return ERR_SOCK_FAIL;

  unsigned char payload[512];
  uint32_t payload_len, cmd;
  if (command == COMMAND_QUERY) {
    cmd = COMMAND_DP_QUERY_NEW;
    payload_len = snprintf((char *)payload, sizeof(payload), "{}");
  } else {
    cmd = COMMAND_CTRL_NEW;
    memcpy(payload, "3.5", 3);
    memset(payload + 3, 0, 12);
    payload_len =
        15 + snprintf((char *)payload + 15, sizeof(payload) - 15,
                      "{\"protocol\":5,\"t\":%d,\"data\":{\"dps\":%s}}",
                      (int)time(NULL), dps ? dps : "{}");
  }

  uint32_t frame_size;
  uint8_t *frame = _tuya_35_pack(led, cmd, payload, payload_len, &frame_size);
  if (!frame)
    return ERR_ENCODE_FAIL;

  int success = send(led->sock, frame, frame_size, 0);
  if (success <= 0) {
    free(frame);
    led->has_sesskey = 0;
    if (_tuya_35_ensure_connected(led)) {
      led->failures++;
      return ERR_SOCK_FAIL;
    }
    frame = _tuya_35_pack(led, cmd, payload, payload_len, &frame_size);
    if (!frame)
      return ERR_ENCODE_FAIL;
    success = send(led->sock, frame, frame_size, 0);
  }
  free(frame);
  if (success <= 0) {
    led->failures++;
    return ERR_SOCK_FAIL;
  }
  led->failures = 0;
  return 0;
}

static int _tuya_35_msg_recv(tuya_led_t *led, uint32_t expected_command,
                             tuya_msg_t *msg) {
  unsigned char hdr_buf[18];
  int hdr_len = recv(led->sock, hdr_buf, 18, 0);
  int retries = 0;
  while (retries <= MAX_RETRIES && hdr_len <= 0) {
    led->has_sesskey = 0;
    if (_tuya_35_ensure_connected(led)) {
      led->failures++;
      return ERR_SOCK_FAIL;
    }
    hdr_len = recv(led->sock, hdr_buf, 18, 0);
    retries++;
  }
  if (hdr_len < 0) {
    led->failures++;
    return ERR_SOCK_FAIL;
  }
  if (hdr_len == 0) {
    led->failures++;
    return ERR_SOCK_CLOSE;
  }

  _tuya_header_t header = _tuya_35_header_parse(hdr_buf);
  uint32_t body_len = header.payload_len + 4; // includes suffix
  if (body_len < 32) {
    led->failures++;
    return ERR_SOCK_FAIL;
  }

  unsigned char *body = (unsigned char *)malloc(body_len);
  if (!body)
    return ERR_ENCODE_FAIL;
  ssize_t body_rx = recv(led->sock, body, body_len, 0);
  if (body_rx <= 0) {
    free(body);
    led->failures++;
    return (body_rx == 0) ? ERR_SOCK_CLOSE : ERR_SOCK_FAIL;
  }

  if (msg) {
    msg->seqno = header.seqno;
    msg->command = header.command;
    int ct_len = (int)header.payload_len - 28;
    if (ct_len <= 0) {
      msg->payload_len = 0;
      free(body);
      return led->failures = 0;
    }
    unsigned char *pt = (unsigned char *)malloc(ct_len + 1);
    if (!pt) {
      free(body);
      return ERR_ENCODE_FAIL;
    }
    memset(pt, 0, ct_len + 1);

    unsigned char *enc_key = led->has_sesskey ? led->sesskey : led->key;
    int dec_len = _gcm_decrypt(body + 12, ct_len, enc_key, body, hdr_buf + 4,
                               14, body + 12 + ct_len, pt);
    if (!dec_len) {
      LOG(PRI_WRN, "_tuya_35_msg_recv: decrypt failed\n");
      msg->payload_len = 0;
      free(pt);
      free(body);
      return led->failures = 0;
    }

    const int retcode_len = 4;
    if (dec_len > retcode_len) {
      _unpack_u32_be(pt, &msg->retcode);
      msg->payload_len = dec_len - retcode_len;
      msg->payload = (unsigned char *)malloc(msg->payload_len + 1);
      if (msg->payload) {
        memset(msg->payload, 0, msg->payload_len + 1);
        memcpy(msg->payload, pt + retcode_len, msg->payload_len);
      }
    } else {
      msg->payload_len = 0;
    }
    free(pt);
  }
  free(body);
  return led->failures = 0;
}

unsigned char *_tuya_payload_encode(tuya_led_t *led, uint32_t command,
                                    unsigned char *payload,
                                    unsigned char *header, uint32_t *msg_size) {
  int encrypt = 1;

  int true_msg_size = *msg_size;
  unsigned char *ct_buf, *padded;
  if (encrypt) {
    padded = _pad(payload, *msg_size, 16, &true_msg_size);
    if (!padded) {
      return NULL;
    }
  }
  ct_buf = (unsigned char *)malloc(true_msg_size + (header ? 15 : 0));
  if (!ct_buf) {
    return NULL;
  }
  memset(ct_buf, 0, true_msg_size + (header ? 15 : 0));
  unsigned char *ct_buf_write = ct_buf;
  if (header) {
    memcpy(ct_buf, header, 15);
    ct_buf_write += 15;
  }
  int ct_len;
  if (encrypt) {
    ct_len = _encrypt((unsigned char *)padded, true_msg_size,
                      (unsigned char *)led->key, ct_buf_write);
    free(padded);
  } else {
    ct_len = true_msg_size;
    memcpy(ct_buf, payload, true_msg_size);
  }
  if (header) {
    ct_len += 15;
  }

  uint32_t header_size = led->version >= 35 ? 0 : 16;
  uint32_t end_size = led->version >= 35 ? 24 : 8;
  *msg_size = header_size + ct_len + end_size;

  uint8_t *msg_buf = (uint8_t *)malloc(*msg_size);
  if (!msg_buf) {
    return NULL;
  }
  _pack_u32_be(PREFIX_55AA_VALUE, msg_buf);
  _pack_u32_be(led->seqno, msg_buf + 4);
  led->seqno++;
  _pack_u32_be(command, msg_buf + 8);
  _pack_u32_be(*msg_size - 16, msg_buf + 12);

  memcpy((char *)msg_buf + 16, (char *)ct_buf, ct_len);
  uint8_t *tail_ptr = msg_buf + 16 + ct_len;
  free(ct_buf);

  _pack_u32_be(_crc32(msg_buf, tail_ptr - msg_buf), tail_ptr);
  _pack_u32_be(SUFFIX_VALUE, tail_ptr + 4);

  return msg_buf;
}

void _tuya_33_protobytes(unsigned char *buf) {
  buf[0] = '3';
  buf[1] = '.';
  buf[2] = '3';
  _pack_u32_be(0, buf + 3);
}

void _tuya_35_protobytes(unsigned char *buf) {
  buf[0] = '3';
  buf[1] = '.';
  buf[2] = '5';
  _pack_u32_be(0, buf + 3);
}

int tuya_cmd_send(tuya_led_t *led, uint32_t command, char *dps) {
  if (led->version == 35)
    return _tuya_35_cmd_send(led, command, dps);

  unsigned char payload[256];
  uint32_t msg_size =
      snprintf((char *)payload, 256,
               "{\"gwId\":\"%s\",\"devId\":\"%s\",\"uid\":\"%s\",\"t\":\"%d\"",
               led->id, led->id, led->id, (int)time(NULL));
  if (dps) {
    msg_size += snprintf((char *)payload + msg_size, 256 - msg_size,
                         ",\"dps\":%s}", dps ? dps : "{}");
  } else {
    msg_size += snprintf((char *)payload + msg_size, 256 - msg_size, "}");
  }

  unsigned char header_buf[VERSION_HEADER_SIZE] = {0};
  unsigned char *header = header_buf;
  if (led->version == 33) {
    if (!((command == COMMAND_QUERY))) {
      _tuya_33_protobytes(header);
    } else {
      header = NULL;
    }
  } else {
    return 1;
  }
  uint8_t *encoded =
      _tuya_payload_encode(led, command, payload, header, &msg_size);
  if (!encoded) {
    return ERR_ENCODE_FAIL;
  }

  int success = send(led->sock, encoded, msg_size, 0);
  int retries = 0;
  while (retries <= MAX_RETRIES && success <= 0) {
    if (errno) {
      int sock_open_status;
      if ((sock_open_status = _tuya_socket_open(led)) && retries == 0) {
        LOG(PRI_ERR, "failed to open socket: %d\n", sock_open_status);
      };
    }
    success = send(led->sock, encoded, msg_size, 0);
    retries++;
  }
  if (success < 0) {
    led->failures++;
    return ERR_SOCK_FAIL;
  } else {
    led->failures = 0;
  }

  free(encoded);
  return 0;
}

_tuya_header_t _tuya_header_parse(unsigned char *header_buf,
                                  size_t header_size) {
  _tuya_header_t header;
  const uint32_t header_len = 16;
  uint32_t prefix;
  _unpack_u32_be(header_buf, &prefix);
  if (prefix != PREFIX_55AA_VALUE) {
    LOG(PRI_WRN, "unknown prefix %d\n", prefix);
  }

  _unpack_u32_be(header_buf + 4, &header.seqno);
  _unpack_u32_be(header_buf + 8, &header.command);
  _unpack_u32_be(header_buf + 12, &header.payload_len);
  header.total_len = header_len + header.payload_len;

  return header;
}

void _tuya_payload_decode(tuya_led_t *led, uint32_t expected_command,
                          unsigned char *encoded, tuya_msg_t *msg) {
  const uint32_t end_len = 8;
  const uint32_t header_len = 16;
  const uint32_t retcode_len = ((msg->command == expected_command)) ? 4 : 0;
  const uint32_t version_header_len =
      ((msg->command == COMMAND_STATUS)) ? VERSION_HEADER_SIZE : 0;

  size_t ct_offset = header_len + retcode_len + version_header_len;
  int ct_len = msg->payload_len - retcode_len - end_len - version_header_len;
  if (ct_len <= 0) {
    msg->payload_len = 0;
    return;
  }
  unsigned char ct[msg->payload_len];
  memset(ct, 0, msg->payload_len);
  msg->payload = (unsigned char *)malloc(ct_len + 1);
  memset(msg->payload, 0, ct_len + 1);

  if (retcode_len) {
    _unpack_u32_be(encoded + header_len, &(msg->retcode));
  } else {
    msg->retcode = -1;
  }

  unsigned char padded[ct_len];
  memset(padded, 0, ct_len);
  memcpy(ct, encoded + ct_offset, ct_len);
  if ((msg->payload_len = _decrypt(ct, ct_len, led->key, padded)) <= 0) {
    msg->payload_len = 0;
    return;
  };

  msg->payload_len = _unpad(padded, msg->payload, ct_len);
}

int tuya_msg_recv(tuya_led_t *led, uint32_t expected_command, tuya_msg_t *msg) {
  if (led->version == 35)
    return _tuya_35_msg_recv(led, expected_command, msg);

  const uint32_t BUFSIZE = 1024;
  unsigned char output_buf[BUFSIZE];
  memset(output_buf, 0, BUFSIZE);
  const uint32_t min_len = 16 + 4;

  int header_len = recv(led->sock, output_buf, min_len, 0);
  int retries = 0;
  while (retries <= MAX_RETRIES && header_len <= 0) {
    if (errno) {
      int sock_open_status;
      if ((sock_open_status = _tuya_socket_open(led)) && retries == 0) {
        LOG(PRI_ERR, "failed to open socket: %d\n", sock_open_status);
      };
    }
    header_len = recv(led->sock, output_buf, min_len, 0);
    retries++;
  }
  if (header_len < 0) {
    led->failures++;
    return ERR_SOCK_FAIL;
  } else if (header_len == 0) {
    led->failures++;
    return ERR_SOCK_CLOSE;
  }

  _tuya_header_t header = _tuya_header_parse(output_buf, (size_t)header_len);
  uint32_t remaining = header.total_len - header_len;
  if (remaining <= 0) {
    return 0;
  }
  if (msg) {
    msg->seqno = header.seqno;
    msg->command = header.command;
    msg->payload_len = remaining;
  }

  size_t body_rx = recv(led->sock, output_buf + header_len, remaining, 0);
  if (body_rx < 0) {
    LOG(PRI_WRN, "\nfailed!\n");
    led->failures++;
    return ERR_SOCK_FAIL;
  } else if (body_rx == 0) {
    LOG(PRI_WRN, "\nclosed!\n");
    led->failures++;
    return ERR_SOCK_CLOSE;
  }

  if (msg) {
    msg->payload_len = header.payload_len;
    _tuya_payload_decode(led, expected_command, output_buf, msg);
  }
  return led->failures = 0;
}

}  // namespace ui
