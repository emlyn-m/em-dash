#include "net/findmy.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"

#include <cstdio>
#include <fcntl.h>
#include <functional>
#include <glib.h>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <sys/poll.h>
#include <thread>
#include <unistd.h>

namespace ui {
namespace {

constexpr int WIRETYPE_VARINT = 0;
constexpr int WIRETYPE_LEN = 2;
constexpr int DEVICE_TYPE_SPOT_DEVICE = 2;

struct FindMy {
  std::string adm_token;
  time_t adm_expiry = 0;
  std::string client_id;
  int wake_r = -1, wake_w = -1;
};

FindMy_State fm_state;
FindMy fm;

// --- google api interface -----------
std::string generate_uuid_v4() {
  // Use a random device to seed the generator
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

  // Generate 128 bits of random data (4 x 32-bit integers)
  uint32_t parts[4];
  for (int i = 0; i < 4; ++i) {
    parts[i] = dis(gen);
  }

  // Enforce UUIDv4 constraints
  // 1. Set the 4 most significant bits of the 7th byte to 0100 (Version 4)
  parts[1] = (parts[1] & 0xFFFF0FFF) | 0x00004000;
  // 2. Set the 2 most significant bits of the 9th byte to 10 (Variant 1)
  parts[2] = (parts[2] & 0x3FFFFFFF) | 0x80000000;

  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(8) << parts[0] << "-"
     << std::setw(4) << (parts[1] >> 16) << "-" << std::setw(4)
     << (parts[1] & 0xFFFF) << "-" << std::setw(4) << (parts[2] >> 16) << "-"
     << std::setw(4) << (parts[2] & 0xFFFF) << std::setw(8) << parts[3];

  return ss.str();
}

// --- protobuf packing ----------------
std::string varint_encode(uint64_t number) {
  std::string out;
  if (number == 0) {
    out.push_back(0);
    return out;
  }
  while (number > 0) {
    uint8_t chunk = number & 0x7F;
    number >>= 7;
    if (number > 0)
      chunk |= 0x80; // continuation bit on all but the last chunk
    out.push_back((char)chunk);
  }
  return out;
}

std::string bytes_encode(const std::string &s) {
  return varint_encode(s.size()) + s;
}

std::string pack_len_field(int field_no, const std::string &val) {
  return varint_encode(((uint64_t)field_no << 3) | WIRETYPE_LEN) +
         bytes_encode(val);
}

std::string pack_varint_field(int field_no, uint64_t val) {
  return varint_encode(((uint64_t)field_no << 3) | WIRETYPE_VARINT) +
         varint_encode(val);
}

std::string to_hex(const std::string &bytes) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (unsigned char c : bytes)
    ss << std::setw(2) << (int)c;
  return ss.str();
}

std::string create_sound_req(const std::string &canonic_device_id,
                             const std::string &gcm_registration_id,
                             bool play) {
  std::string req_id = generate_uuid_v4();

  std::string scope =
      pack_varint_field(2, DEVICE_TYPE_SPOT_DEVICE) +
      pack_len_field(3,
                     pack_len_field(1, pack_len_field(1, canonic_device_id)));

  // DeviceComponentUnspecified: default value omitted, so a zero-length field.
  std::string action = pack_len_field(31 + (play ? 0 : 1), std::string());

  std::string metadata =
      pack_varint_field(1, DEVICE_TYPE_SPOT_DEVICE) +
      pack_len_field(2, req_id) + pack_len_field(3, fm.client_id) +
      pack_len_field(4, pack_len_field(1, gcm_registration_id)) +
      pack_varint_field(6, 1);

  std::string req = pack_len_field(1, scope) + pack_len_field(2, action) +
                    pack_len_field(3, metadata);

  return to_hex(req);
}

bool ensure_adm_token() {
  time_t now = time(0);
  if ((now + 300) <
      fm.adm_expiry) { // 5 minute buffer to ensure timing - ideally avoid
                       // refreshing on a STOP_PLAY action
    return 1;
  }

  char adm_req[1024];
  snprintf(adm_req, 1024,
           "curl -s \"https://android.clients.google.com/auth\" -X POST"
           " -H \"Accept-Encoding: identity\""
           " -H \"Content-Type: application/x-www-form-urlencoded\""
           " -H \"User-Agent: GoogleAuth/1.4\""
           " -d \"accountType=HOSTED_OR_GOOGLE\""
           " -d \"Email=%s\""
           " -d \"has_permission=1\""
           " -d \"EncryptedPasswd=%s\""
           " -d "
           "\"service=oauth2:https://www.googleapis.com/auth/"
           "android_device_manager\""
           " -d \"source=android\""
           " -d \"androidId=%s\""
           " -d \"app=com.google.android.apps.adm\""
           " -d \"client_sig=38918a453d07199354f8b19af05ec6562ced5788\""
           " -d \"device_country=us\""
           " -d \"operatorCountry=us\""
           " -d \"lang=en\""
           " -d \"sdk_version=17\""
           " -d \"google_play_services_version=240913000\"",
           get_attr_str("FINDMY_EMAIL"), get_attr_str("FINDMY_AAS"),
           get_attr_str("FINDMY_FCM_ID"));

  FILE *fp = popen(adm_req, "r");
  if (!fp) {
    LOG(PRI_ERR, "findmy: adm req failed\n");
    return false;
  }

  std::string body;
  char chunk[4096];
  size_t got;
  while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0)
    body.append(chunk, got);
  pclose(fp);

  if (!body.size()) {
    LOG(PRI_ERR, "findmy: no adm response\n");
    return false;
  }

  size_t expiry_pos = body.find("Expiry=") + 7;
  size_t auth_pos = body.find("Auth=") + 5;

  if (expiry_pos == body.size() || auth_pos == body.size()) {
    LOG(PRI_ERR, "findmy: adm response did not include tags\nraw: %s\n",
        body.c_str());
    return false;
  }

  fm.adm_expiry =
      atol(body.substr(expiry_pos, body.find('\n', expiry_pos) - expiry_pos)
               .c_str());
  fm.adm_token = body.substr(auth_pos, body.find('\n', auth_pos) - auth_pos);

  LOG(PRI_DBG, "findmy: rx adm token expiring at %d\n", fm.adm_expiry);

  return true;
}

int set_sound(bool play) {
  if (ensure_adm_token()) {
    std::string req = create_sound_req(get_attr_str("FINDMY_DEVICE_ID"),
                                       get_attr_str("FINDMY_FCM_TOKEN"), play);
    const char *req_c = req.c_str();

    char setsound_cmd[4096] = {0};
    int setsound_offset = snprintf(setsound_cmd, 11, "echo -en \"");

    uint req_offset = 0;
    int step;
    while (req_offset < req.size()) {
      step = snprintf(setsound_cmd + setsound_offset, 17, "%s",
                      req_c + req_offset);
      if (step > 16) {
        snprintf(setsound_cmd + setsound_offset + 16, 3, "\\n");
        setsound_offset += 2;
      }
      req_offset += MIN(step, 16);
      setsound_offset += MIN(step, 16);
    }

    snprintf(
        setsound_cmd + setsound_offset, 4096 - setsound_offset,
        "\" | xxd -r -p | curl -s -X POST "
        "\"https://android.googleapis.com/nova/nbe_execute_action\""
        " -H \"Content-Type: application/x-www-form-urlencoded; charset=UTF-8\""
        " -H \"Authorization: Bearer %s\""
        " -H \"Accept-Language: en-US\""
        " -H \"User-Agent: fmd/20006320; gzip\""
        " --data-binary @-",
        fm.adm_token.c_str());

    FILE *fp = popen(setsound_cmd, "r");
    if (!fp) {
      LOG(PRI_ERR, "findmy: setsound exec failed\n");
      return fm_state.playing;
    }
    int status = pclose(fp);
    if (status == -1 || WEXITSTATUS(status)) {
      LOG(PRI_ERR, "findmy: setsound cURL returned %d\n", WEXITSTATUS(status));
      return fm_state.playing;
    }

    return play;
  }
  return fm_state.playing;
}

// --- work thread ---------------------

int poll_wait(int timeout) {
  struct pollfd fds[1];
  fds[0].fd = fm.wake_r;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  int nfds = 1;
  if (poll(fds, nfds, timeout) <= 0) // timeout=-1 => wait until woken
    return 0;
  if (fds[0].revents & POLLIN) {
    char buf[1];
    while (read(fm.wake_r, buf, sizeof buf) > 0) {
    } // drain (non-blocking)
    return *buf;
  }
  return 0;
}

void findmy_worker(std::function<void()> on_update) {
  FindMy_InitState initstate = FINDMY_INIT_SUCCESS;
  if (get_attr_str("FINDMY_EMAIL") == NULL) {
    initstate = FINDMY_INIT_FAILED;
  }
  if (get_attr_str("FINDMY_AAS") == NULL) {
    initstate = FINDMY_INIT_FAILED;
  }
  if (get_attr_str("FINDMY_FCM_ID") == NULL) {
    initstate = FINDMY_INIT_FAILED;
  }
  fm.client_id = generate_uuid_v4();
  if (!ensure_adm_token()) {
    initstate = FINDMY_INIT_FAILED;
  }

  post_to_main([initstate, on_update]() mutable {
    fm_state.initialized = initstate;
    if (on_update) {
      on_update();
    }
  });

  if (initstate != FINDMY_INIT_SUCCESS) {
    LOG(PRI_ERR, "FindMy failed to init, thread exiting\n");
    return;
  }

  for (;;) {
    int action = poll_wait(-1);

    int init_state = fm_state.playing;
    post_to_main([on_update]() mutable {
      fm_state.loading = 1;
      if (on_update)
        on_update();
    });

    int target_state = action ? !init_state : init_state;
    int new_state = set_sound(target_state);

    post_to_main([new_state, on_update]() mutable {
      fm_state.playing = new_state;
      fm_state.loading = 0;
      if (on_update) {
        on_update();
      }
    });
  }
}

} // namespace

void findmy_start(std::function<void()> on_update) {
  fm_state.initialized = FINDMY_INITIALIZING;
  fm_state.playing = 0;
  fm_state.loading = 0;

  int fds[2];
  if (pipe(fds) != 0) {
    LOG(PRI_ERR, "findmy: pipe failed\n");
    return;
  }
  fcntl(fds[0], F_SETFL, O_NONBLOCK);
  fcntl(fds[1], F_SETFL, O_NONBLOCK);

  fm.wake_r = fds[0];
  fm.wake_w = fds[1];

  std::thread(findmy_worker, std::move(on_update)).detach();
}

const FindMy_State &findmy_state() { return fm_state; }

void findmy_ping() {
  int b = 1;
  ssize_t n = write(fm.wake_w, &b, 1);
  (void)n;
}

} // namespace ui
