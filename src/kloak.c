/*
 * Copyright (c) 2025 - 2025 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
 * See the file COPYING for copying conditions.
 */

/*
 * NOTES FOR DEVELOPERS:
 * - Use signed arithmetic wherever possible. Any form of integer
 *   over/underflow is dangerous here, thus kloak has -ftrapv enabled and thus
 *   signed arithmetic over/underflow will simply crash (and thus restart)
 *   kloak rather than resulting in memory corruption. Unsigned over/underflow
 *   however does NOT trap because it is well-defined in C. Thus avoid
 *   unsigned arithmetic wherever possible.
 * - Use an assert to check that a value is within bounds before every cast.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <math.h>
#include <sys/queue.h>
#include <getopt.h>
#include <assert.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <linux/input.h>
#include <sys/inotify.h>

#include <wayland-client.h>
#include "xdg-output-protocol.h"
#include "wlr-layer-shell.h"
#include "wlr-virtual-pointer.h"
#include "virtual-keyboard.h"

#include <libinput.h>
#include <libevdev/libevdev.h>
#include <xkbcommon/xkbcommon.h>

#include "kloak.h"

/********************/
/* global variables */
/********************/

static double cursor_x = 0;
static double cursor_y = 0;
static double prev_cursor_x = 0;
static double prev_cursor_y = 0;

static struct disp_state state = { 0 };
static struct libinput *li = NULL;
static int inotify_fd = 0;

static struct pollfd *ev_fds = { 0 };

static int64_t prev_release_time = 0;
static TAILQ_HEAD(tailhead_evq, input_packet) evq_head;

static int32_t max_delay = DEFAULT_MAX_DELAY_MS;
static int32_t startup_delay = DEFAULT_STARTUP_TIMEOUT_MS;
static uint32_t cursor_color = 0xffff0000;

static uint32_t **esc_key_list = NULL;
static size_t *esc_key_sublist_len = NULL;
static bool *active_esc_key_list = NULL;
static size_t esc_key_list_len = 0;

static LIST_HEAD(listhead_ldi, li_device_info) ldi_head;

const char *default_esc_key_str = "KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_ESC";

int64_t start_time = 0;

int randfd = 0;

static struct key_name_value key_table[] = {
  {"KEY_ESC", KEY_ESC},
  {"KEY_1", KEY_1},
  {"KEY_2", KEY_2},
  {"KEY_3", KEY_3},
  {"KEY_4", KEY_4},
  {"KEY_5", KEY_5},
  {"KEY_6", KEY_6},
  {"KEY_7", KEY_7},
  {"KEY_8", KEY_8},
  {"KEY_9", KEY_9},
  {"KEY_0", KEY_0},
  {"KEY_MINUS", KEY_MINUS},
  {"KEY_EQUAL", KEY_EQUAL},
  {"KEY_BACKSPACE", KEY_BACKSPACE},
  {"KEY_TAB", KEY_TAB},
  {"KEY_Q", KEY_Q},
  {"KEY_W", KEY_W},
  {"KEY_E", KEY_E},
  {"KEY_R", KEY_R},
  {"KEY_T", KEY_T},
  {"KEY_Y", KEY_Y},
  {"KEY_U", KEY_U},
  {"KEY_I", KEY_I},
  {"KEY_O", KEY_O},
  {"KEY_P", KEY_P},
  {"KEY_LEFTBRACE", KEY_LEFTBRACE},
  {"KEY_RIGHTBRACE", KEY_RIGHTBRACE},
  {"KEY_ENTER", KEY_ENTER},
  {"KEY_LEFTCTRL", KEY_LEFTCTRL},
  {"KEY_A", KEY_A},
  {"KEY_S", KEY_S},
  {"KEY_D", KEY_D},
  {"KEY_F", KEY_F},
  {"KEY_G", KEY_G},
  {"KEY_H", KEY_H},
  {"KEY_J", KEY_J},
  {"KEY_K", KEY_K},
  {"KEY_L", KEY_L},
  {"KEY_SEMICOLON", KEY_SEMICOLON},
  {"KEY_APOSTROPHE", KEY_APOSTROPHE},
  {"KEY_GRAVE", KEY_GRAVE},
  {"KEY_LEFTSHIFT", KEY_LEFTSHIFT},
  {"KEY_BACKSLASH", KEY_BACKSLASH},
  {"KEY_Z", KEY_Z},
  {"KEY_X", KEY_X},
  {"KEY_C", KEY_C},
  {"KEY_V", KEY_V},
  {"KEY_B", KEY_B},
  {"KEY_N", KEY_N},
  {"KEY_M", KEY_M},
  {"KEY_COMMA", KEY_COMMA},
  {"KEY_DOT", KEY_DOT},
  {"KEY_SLASH", KEY_SLASH},
  {"KEY_RIGHTSHIFT", KEY_RIGHTSHIFT},
  {"KEY_KPASTERISK", KEY_KPASTERISK},
  {"KEY_LEFTALT", KEY_LEFTALT},
  {"KEY_SPACE", KEY_SPACE},
  {"KEY_CAPSLOCK", KEY_CAPSLOCK},
  {"KEY_F1", KEY_F1},
  {"KEY_F2", KEY_F2},
  {"KEY_F3", KEY_F3},
  {"KEY_F4", KEY_F4},
  {"KEY_F5", KEY_F5},
  {"KEY_F6", KEY_F6},
  {"KEY_F7", KEY_F7},
  {"KEY_F8", KEY_F8},
  {"KEY_F9", KEY_F9},
  {"KEY_F10", KEY_F10},
  {"KEY_NUMLOCK", KEY_NUMLOCK},
  {"KEY_SCROLLLOCK", KEY_SCROLLLOCK},
  {"KEY_KP7", KEY_KP7},
  {"KEY_KP8", KEY_KP8},
  {"KEY_KP9", KEY_KP9},
  {"KEY_KPMINUS", KEY_KPMINUS},
  {"KEY_KP4", KEY_KP4},
  {"KEY_KP5", KEY_KP5},
  {"KEY_KP6", KEY_KP6},
  {"KEY_KPPLUS", KEY_KPPLUS},
  {"KEY_KP1", KEY_KP1},
  {"KEY_KP2", KEY_KP2},
  {"KEY_KP3", KEY_KP3},
  {"KEY_KP0", KEY_KP0},
  {"KEY_KPDOT", KEY_KPDOT},
  {"KEY_ZENKAKUHANKAKU", KEY_ZENKAKUHANKAKU},
  {"KEY_102ND", KEY_102ND},
  {"KEY_F11", KEY_F11},
  {"KEY_F12", KEY_F12},
  {"KEY_RO", KEY_RO},
  {"KEY_KATAKANA", KEY_KATAKANA},
  {"KEY_HIRAGANA", KEY_HIRAGANA},
  {"KEY_HENKAN", KEY_HENKAN},
  {"KEY_KATAKANAHIRAGANA", KEY_KATAKANAHIRAGANA},
  {"KEY_MUHENKAN", KEY_MUHENKAN},
  {"KEY_KPJPCOMMA", KEY_KPJPCOMMA},
  {"KEY_KPENTER", KEY_KPENTER},
  {"KEY_RIGHTCTRL", KEY_RIGHTCTRL},
  {"KEY_KPSLASH", KEY_KPSLASH},
  {"KEY_SYSRQ", KEY_SYSRQ},
  {"KEY_RIGHTALT", KEY_RIGHTALT},
  {"KEY_LINEFEED", KEY_LINEFEED},
  {"KEY_HOME", KEY_HOME},
  {"KEY_UP", KEY_UP},
  {"KEY_PAGEUP", KEY_PAGEUP},
  {"KEY_LEFT", KEY_LEFT},
  {"KEY_RIGHT", KEY_RIGHT},
  {"KEY_END", KEY_END},
  {"KEY_DOWN", KEY_DOWN},
  {"KEY_PAGEDOWN", KEY_PAGEDOWN},
  {"KEY_INSERT", KEY_INSERT},
  {"KEY_DELETE", KEY_DELETE},
  {"KEY_MACRO", KEY_MACRO},
  {"KEY_MUTE", KEY_MUTE},
  {"KEY_VOLUMEDOWN", KEY_VOLUMEDOWN},
  {"KEY_VOLUMEUP", KEY_VOLUMEUP},
  {"KEY_POWER", KEY_POWER},
  {"KEY_POWER2", KEY_POWER2},
  {"KEY_KPEQUAL", KEY_KPEQUAL},
  {"KEY_KPPLUSMINUS", KEY_KPPLUSMINUS},
  {"KEY_PAUSE", KEY_PAUSE},
  {"KEY_SCALE", KEY_SCALE},
  {"KEY_KPCOMMA", KEY_KPCOMMA},
  {"KEY_HANGEUL", KEY_HANGEUL},
  {"KEY_HANGUEL", KEY_HANGUEL},
  {"KEY_HANJA", KEY_HANJA},
  {"KEY_YEN", KEY_YEN},
  {"KEY_LEFTMETA", KEY_LEFTMETA},
  {"KEY_RIGHTMETA", KEY_RIGHTMETA},
  {"KEY_COMPOSE", KEY_COMPOSE},
  {"KEY_F13", KEY_F13},
  {"KEY_F14", KEY_F14},
  {"KEY_F15", KEY_F15},
  {"KEY_F16", KEY_F16},
  {"KEY_F17", KEY_F17},
  {"KEY_F18", KEY_F18},
  {"KEY_F19", KEY_F19},
  {"KEY_F20", KEY_F20},
  {"KEY_F21", KEY_F21},
  {"KEY_F22", KEY_F22},
  {"KEY_F23", KEY_F23},
  {"KEY_F24", KEY_F24},
  {"KEY_UNKNOWN", KEY_UNKNOWN},
  {NULL, 0}
};

/*********************/
/* utility functions */
/*********************/

static void *safe_calloc(size_t nmemb, size_t size) {
  void *out_ptr = calloc(nmemb, size);
  if (out_ptr == NULL) {
    fprintf(stderr,
      "FATAL ERROR: Could not allocate memory: %s\n", strerror(errno));
    exit(1);
  }
  return out_ptr;
}

static void *safe_reallocarray(void *ptr, size_t nmemb, size_t size) {
  void *out_ptr = reallocarray(ptr, nmemb, size);
  if (out_ptr == NULL) {
    fprintf(stderr,
      "FATAL ERROR: Could not allocate memory: %s\n", strerror(errno));
    exit(1);
  }
  return out_ptr;
}

static char *safe_strdup(const char *s) {
  char *out_ptr = strdup(s);
  if (out_ptr == NULL) {
    fprintf(stderr,
      "FATAL ERROR: Could not allocate memory: %s\n", strerror(errno));
    exit(1);
  }
  return out_ptr;
}

static int safe_open(const char *pathname, int flags) {
  int out_int = open(pathname, flags);
  if (out_int == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not open file '%s': %s\n", pathname,
      strerror(errno));
    exit(1);
  }
  return out_int;
}

static void safe_close(int fd) {
  int rslt = close(fd);
  if (rslt == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not close a file: %s\n", strerror(errno));
    exit(1);
  }
}

static DIR *safe_opendir(const char *name, bool allow_enoent) {
  DIR *out_ptr = opendir(name);
  int dir_fd = 0;
  if (out_ptr == NULL) {
    if (!allow_enoent || errno != ENOENT) {
      fprintf(stderr,
        "FATAL ERROR: Could not open directory '%s': %s\n", name,
        strerror(errno));
      exit(1);
    }
  }

  dir_fd = dirfd(out_ptr);
  if (dir_fd == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not get file descriptor for directory '%s': %s\n",
      name, strerror(errno));
    exit(1);
  }
  if (fcntl(dir_fd, F_SETFD, FD_CLOEXEC) == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not set FD_CLOEXEC on file descriptor for directory '%s': %s\n",
      name, strerror(errno));
    exit(1);
  }
  return out_ptr;
}

static void safe_closedir(DIR *dirp) {
  int rslt = closedir(dirp);
  if (rslt == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not close a directory: %s\n", strerror(errno));
    exit(1);
  }
}

/*static off_t safe_lseek(int fd, off_t offset, int whence) {
  off_t rslt = lseek(fd, offset, whence);
  if (rslt == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not seek within a file: %s\n", strerror(errno));
    exit(1);
  }
  return rslt;
}*/

static void read_random(char *buf, ssize_t len) {
  assert(len >= 0);
  assert(randfd > 0);
  assert(buf != NULL);

  if (read(randfd, buf, (size_t)(len)) < len) {
    fprintf(stderr,
      "FATAL ERROR: Could not read %ld byte(s) from /dev/urandom!\n", len);
    exit(1);
  }
}

static void randname(char *buf, ssize_t len) {
  char randchar = 0;

  assert(len >= 0);
  assert(buf != NULL);

  for (ssize_t i = 0; i < len; i++) {
    do {
      read_random(&randchar, 1);
      if (randchar == CHAR_MIN) {
        randchar = 0;
      } else {
        randchar = (char)(abs(randchar));
      }
    } while (randchar >= (CHAR_MAX - (CHAR_MAX % ALPHABET_LEN)));

    randchar %= (ALPHABET_LEN * 2);
    if (randchar < ALPHABET_LEN) {
      randchar += ASCII_UPPERCASE_START;
    } else {
      randchar += ASCII_LOWERCASE_START;
    }
    buf[i] = randchar;
  }
}

static int create_shm_file(ssize_t size) {
  int32_t retries = 100;
  int fd = -1;
  /* 18 = length of string '/kloak-XXXXXXXXXX' + NULL terminator */
  char name[18];
  int snprintf_len = 0;

  assert(size >= 0);
  /*
   * In a perfect world, we'd do the sizeof check here at compile time, but
   * unfortunately that isn't possible.
   */
  if (sizeof(off_t) == 4) {
    assert(size <= INT32_MAX);
  } else if (sizeof(off_t) == 8) {
    assert(size <= INT64_MAX);
  } else {
    fprintf(stderr,
      "FATAL ERROR: off_t is not 32-bit or 64-bit, cannot continue\n");
    exit(1);
  }

  do {
    snprintf_len = snprintf(name, sizeof(name), "%s", "/kloak-XXXXXXXXXX");
    assert(snprintf_len > 0 && (size_t)(snprintf_len) == sizeof(name) - 1);
    /* 10 = length of 'XXXXXXXXXX', 11 = length + NULL terminator */
    randname(name + sizeof(name) - 11, 10);
    --retries;
    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd >= 0) {
      shm_unlink(name);
      break;
    }
  } while (retries > 0 && errno == EEXIST);

  if (fd == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not create shared memory fd: %s\n",
      strerror(errno));
    exit(1);
  }

  int32_t ret;
  do {
    ret = ftruncate(fd, (off_t)(size));
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    close(fd);
    fprintf(stderr,
      "FATAL ERROR: Could not allocate shared memory block: %s\n",
      strerror(errno));
    exit(1);
  }

  return fd;
}

static int64_t current_time_ms(void) {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  int64_t result = (spec.tv_sec * 1000) + (spec.tv_nsec / 1000000);
  assert(result >= 0);
  if (start_time == 0) {
    start_time = result;
    return 0;
  }
  return result - start_time;
}

static int64_t random_between(int64_t lower, int64_t upper) {
  assert(lower >= 0);
  assert(upper >= 0);

  int64_t maxval;
  union rand_int64 randval;
  /* default to max if the interval is not valid */
  if (lower >= upper) {
    return upper;
  }

  maxval = upper - lower + 1;
  assert(maxval > 0);
  do {
    read_random(randval.raw, sizeof(int64_t));
    if (randval.val == INT64_MIN) {
      randval.val = 0;
    } else {
      randval.val = llabs(randval.val);
    }
  } while (randval.val >= (INT64_MAX - (INT64_MAX % maxval)));

  randval.val %= maxval;
  return lower + randval.val;
}

static bool check_point_in_area(int32_t x, int32_t y, int32_t rect_x,
  int32_t rect_y, int32_t rect_width, int32_t rect_height) {
  if (x < 0 || y < 0 || rect_x < 0 || rect_y < 0 || rect_width < 0
    || rect_height < 0) {
    return false;
  }
  if (x >= rect_x && x < rect_x + rect_width
    && y >= rect_y && y < rect_y + rect_height) {
    return true;
  }
  return false;
}

static bool check_screen_touch(struct output_geometry scr1,
  struct output_geometry scr2) {
  /*
   * We check for both touching and overlapping screens. Screens are
   * overlapping if any of one screen's corner points falls inside the area of
   * the other screen. The criteria to establish touching screens is a bit
   * tricky, but a shortcut we can take is to simply grow the size of one of
   * the screens by one pixel in every direction (i.e., subtract one from both
   * the X and Y position coordinates and then add two to the width and
   * height). Then any form of screen touching will be seen as an overlap,
   * including touching at the corners.
   */

  if (scr1.x < 0 || scr1.y < 0 || scr1.width < 0 || scr1.height < 0
    || scr2.x < 0 || scr2.y < 0 || scr2.width < 0 || scr2.height < 0) {
    return false;
  }

  if (scr1.x > 0) {
    scr1.x -= 1;
    scr1.width += 2;
  } else {
    scr1.width += 1;
  }
  if (scr1.y > 0) {
    scr1.y -= 1;
    scr1.height += 2;
  } else {
    scr1.height += 1;
  }

  if (check_point_in_area(scr1.x, scr1.y, scr2.x, scr2.y, scr2.x + scr2.width,
    scr2.y + scr2.height)) {
    return true;
  }
  if (check_point_in_area(scr1.x + scr1.width, scr1.y, scr2.x, scr2.y,
    scr2.x + scr2.width, scr2.y + scr2.height)) {
    return true;
  }
  if (check_point_in_area(scr1.x, scr1.y + scr1.height, scr2.x, scr2.y,
    scr2.x + scr2.width, scr2.y + scr2.height)) {
    return true;
  }
  if (check_point_in_area(scr1.x + scr1.width, scr1.y + scr1.height, scr2.x,
    scr2.y, scr2.x + scr2.width, scr2.y + scr2.height)) {
    return true;
  }
  return false;
}

static void recalc_global_space(struct disp_state * state) {
  int32_t ul_corner_x = INT32_MAX;
  int32_t ul_corner_y = INT32_MAX;
  int32_t br_corner_x = 0;
  int32_t br_corner_y = 0;

  struct output_geometry *screen_list[MAX_DRAWABLE_LAYERS];
  ssize_t screen_list_len = 0;

  for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
    if (!state->output_geometries[i]) {
      continue;
    }
    int32_t cur_geom_x = state->output_geometries[i]->x;
    int32_t cur_geom_y = state->output_geometries[i]->y;
    int32_t cur_geom_width = state->output_geometries[i]->width;
    int32_t cur_geom_height = state->output_geometries[i]->height;
    if (cur_geom_x < 0 || cur_geom_y < 0
      || cur_geom_width < 0 || cur_geom_height < 0) {
      continue;
    }
    screen_list[screen_list_len] = state->output_geometries[i];
    screen_list_len++;
    if (cur_geom_x < ul_corner_x) {
      ul_corner_x = cur_geom_x;
    }
    if (cur_geom_y < ul_corner_y) {
      ul_corner_y = cur_geom_y;
    }
    int32_t temp_br_x = cur_geom_x + cur_geom_width;
    int32_t temp_br_y = cur_geom_y + cur_geom_height;
    if (temp_br_x > br_corner_x) {
      br_corner_x = temp_br_x;
    }
    if (temp_br_y > br_corner_y) {
      br_corner_y = temp_br_y;
    }
  }

  /* Silently fail if we haven't gotten a valid state yet */
  if (screen_list_len <= 0) {
    return;
  }
  if (ul_corner_x > br_corner_x) {
    return;
  }
  if (ul_corner_y > br_corner_y) {
    return;
  }

  struct output_geometry *conn_screen_list[MAX_DRAWABLE_LAYERS];
  conn_screen_list[0] = screen_list[0];
  ssize_t conn_screen_list_len = 1;

  /*
   * Check for gaps between the screens. We don't support running if gaps are
   * present. To do this, we start with an arbitrary screen, then find all
   * screens touching it, then find all screens touching those screens, and so
   * on, until we can't find any more screens touching whatever "zone" we
   * started in. If the number of connected screens is equal to the number of
   * attached screens, then all screens are connected, otherwise there is a
   * gap somewhere.
   */
  for (ssize_t i = 0; i < conn_screen_list_len; i++) {
    for (ssize_t j = 0; j < screen_list_len; j++) {
      bool screen_in_conn_list = false;
      for (ssize_t k = 0; k < conn_screen_list_len; k++) {
        if (screen_list[j] == conn_screen_list[k]) {
          screen_in_conn_list = true;
          break;
        }
      }
      if (screen_in_conn_list) {
        continue;
      }
      struct output_geometry *conn_screen = conn_screen_list[i];
      struct output_geometry *cur_screen = screen_list[j];
      if (check_screen_touch(*conn_screen, *cur_screen)) {
        /* Found a touching screen! */
        conn_screen_list[conn_screen_list_len] = cur_screen;
        conn_screen_list_len++;
      }
    }
  }

  if (conn_screen_list_len != screen_list_len) {
    fprintf(stderr,
      "FATAL ERROR: Multiple screens are attached and gaps are present between them. kloak cannot operate in this configuration.\n");
    exit(1);
  }

  state->global_space_width = br_corner_x;
  state->global_space_height = br_corner_y;
  state->pointer_space_x = ul_corner_x;
  state->pointer_space_y = ul_corner_y;
}

static struct screen_local_coord abs_coord_to_screen_local_coord(int32_t x,
  int32_t y) {
  struct screen_local_coord out_data = { 0 };
  if (x < 0 || y < 0) {
    return out_data;
  }

  for (int32_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
    if (!state.output_geometries[i]) {
      continue;
    }

    int32_t cur_geom_x = state.output_geometries[i]->x;
    int32_t cur_geom_y = state.output_geometries[i]->y;
    int32_t cur_geom_width = state.output_geometries[i]->width;
    int32_t cur_geom_height = state.output_geometries[i]->height;
    if (cur_geom_x < 0 || cur_geom_y < 0 || cur_geom_width < 0
      || cur_geom_height < 0 ) {
      continue;
    }

    if (!(x >= cur_geom_x)) {
      continue;
    }
    if (!(y >= cur_geom_y)) {
      continue;
    }
    if (!(x < cur_geom_x + cur_geom_width)) {
      continue;
    }
    if (!(y < cur_geom_y + cur_geom_height)) {
      continue;
    }
    out_data.output_idx = i;
    out_data.x = x - cur_geom_x;
    out_data.y = y - cur_geom_y;
    out_data.valid = true;
    break;
  }

  /* There is a possibility that out_data will contain all zeros; if this
   * happens, it means that no output covered the requested coordinates in the
   * compositor's global space. That's a valid thing to tell a caller, so just
   * return it anyway. */
  return out_data;
}

static struct coord screen_local_coord_to_abs_coord(int32_t x, int32_t y,
  int32_t output_idx) {
  struct coord out_val = {
    .x = -1,
    .y = -1,
  };

  assert(output_idx >= 0);
  if (x < 0 || y < 0) {
    return out_val;
  }

  if (!state.layers[output_idx]) {
    return out_val;
  }
  int32_t cur_geom_x = state.output_geometries[output_idx]->x;
  int32_t cur_geom_y = state.output_geometries[output_idx]->y;
  int32_t cur_geom_width = state.output_geometries[output_idx]->width;
  int32_t cur_geom_height = state.output_geometries[output_idx]->height;
  if (cur_geom_x < 0 || cur_geom_y < 0 || cur_geom_width < 0
    || cur_geom_height < 0) {
    return out_val;
  }

  out_val.x = cur_geom_x + x;
  out_val.y = cur_geom_y + y;
  return out_val;
}

struct coord traverse_line(struct coord start, struct coord end,
  int32_t pos) {
  if (pos == 0) return start;
  struct coord out_val = { 0 };

  double num = ((double) end.y) - ((double) start.y);
  double denom = ((double) start.x) - ((double) end.x);
  if (denom == 0) {
    /* vertical line */
    out_val.x = start.x;
    if (start.y < end.y) {
      out_val.y = start.y + pos;
    } else {
      out_val.y = start.y - pos;
    }
    return out_val;
  }

  double slope = num / denom;
  double steep = fabs(slope);

  if (steep < 1) {
    if (start.x < end.x) {
      out_val.x = start.x + pos;
    } else {
      out_val.x = start.x - pos;
    }
    if (start.y < end.y) {
      out_val.y = start.y + (int32_t)((double) pos * steep);
    } else {
      out_val.y = start.y - (int32_t)((double) pos * steep);
    }
  } else {
    if (start.y < end.y) {
      out_val.y = start.y + pos;
    } else {
      out_val.y = start.y - pos;
    }
    if (start.x < end.x) {
      out_val.x = start.x + (int32_t)((double) pos * (1 / steep));
    } else {
      out_val.x = start.x - (int32_t)((double) pos * (1 / steep));
    }
  }

  return out_val;
}

static void draw_block(uint32_t *pixbuf, int32_t offset, int32_t x, int32_t y,
  int32_t layer_width, int32_t layer_height, int32_t rad, bool crosshair) {
  int32_t start_x = x - rad;
  if (start_x < 0) start_x = 0;
  int32_t start_y = y - rad;
  if (start_y < 0) start_y = 0;
  int32_t end_x = x + rad;
  if (end_x >= layer_width) end_x = layer_width - 1;
  int32_t end_y = y + rad;
  if (end_y >= layer_height) end_y = layer_height - 1;

  for (int32_t work_y = start_y; work_y <= end_y; work_y++) {
    for (int32_t work_x = start_x; work_x <= end_x; work_x++) {
      if (crosshair && work_x == x) {
        pixbuf[offset + (work_y * layer_width + work_x)] = cursor_color;
      } else if (crosshair && work_y == y) {
        pixbuf[offset + (work_y * layer_width + work_x)] = cursor_color;
      } else {
        pixbuf[offset + (work_y * layer_width + work_x)] = 0x00000000;
      }
    }
  }
}

static int32_t parse_uint31_arg(const char *arg_name, const char *val,
  int base) {
  char *val_endchar;
  uint64_t val_int;

  val_int = strtoul(val, &val_endchar, base);
  if (*val_endchar != '\0') {
    goto parse_uint31_arg_error;
  }
  if (val_int > INT32_MAX) {
    goto parse_uint31_arg_error;
  }
  return (int32_t)(val_int);

parse_uint31_arg_error:
  fprintf(stderr,
    "FATAL ERROR: Invalid value '%s' passed to parameter '%s'!\n", val, arg_name);
  exit(1);
  return -1;
}

static uint32_t parse_uint32_arg(const char *arg_name, const char *val,
  int base) {
  char *val_endchar;
  uint64_t val_int;

  val_int = strtoul(val, &val_endchar, base);
  if (*val_endchar != '\0') {
    goto parse_uint31_arg_error;
  }
  if (val_int > UINT32_MAX) {
    goto parse_uint31_arg_error;
  }
  return (uint32_t)(val_int);

parse_uint31_arg_error:
  fprintf(stderr,
    "FATAL ERROR: Invalid value '%s' passed to parameter '%s'!\n", val, arg_name);
  exit(1);
  return 0;
}

static int32_t sleep_ms(int64_t ms) {
  assert(ms >= 0);
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000);
  ts.tv_nsec = (ms % 1000) * 1000000;
  int nanosleep_ret;
  do {
    nanosleep_ret = nanosleep(&ts, &ts);
  } while (nanosleep_ret == -1 && errno == EINTR);

  if (nanosleep_ret == -1) {
    return -1;
  }
  return 0;
}

static char *sgenprintf(char *str, ...) {
  char *rslt = NULL;
  int rslt_len = 0;
  int rslt_writelen = 0;

  va_list extra_args;
  va_start(extra_args, str);
  rslt_len = vsnprintf(NULL, 0, str, extra_args) + 1;
  va_end(extra_args);
  if (rslt_len == -1) {
    fprintf(stderr,
      "FATAL ERROR: String processing issue: %s\n", strerror(errno));
    exit(1);
  }
  rslt = safe_calloc(1, (size_t)(rslt_len));
  va_start(extra_args, str);
  rslt_writelen = vsnprintf(rslt, (size_t)(rslt_len), str, extra_args);
  va_end(extra_args);

  assert(rslt_writelen == rslt_len - 1);
  return rslt;
}

static uint32_t lookup_keycode(const char *name) {
  struct key_name_value *p;
  for (p = key_table; p->name != NULL; p++) {
    if (strcmp(p->name, name) == 0) {
      return p->value;
    }
  }
  return 0;
}

static void attach_input_device(const char *dev_name) {
  bool found_device = false;
  struct libinput_device *new_device = NULL;
  char *device_path = NULL;
  struct li_device_info *ldi_node = NULL;

  ldi_node = LIST_FIRST(&ldi_head);
  while (ldi_node != NULL) {
    if (strcmp(ldi_node->device_name, dev_name) == 0) {
      found_device = true;
      break;
    }
    ldi_node = LIST_NEXT(ldi_node, entries);
  }

  if (found_device) {
    /*
     * This may mean a device was quickly detached and then attached again,
     * thus try removing and re-attaching it.
     */
    detach_input_device(dev_name);
  }

  device_path = sgenprintf("/dev/input/%s", dev_name);
  new_device = libinput_path_add_device(li, device_path);
  free(device_path);
  if (new_device == NULL) {
    return;
  }

  ldi_node = safe_calloc(1, sizeof(struct li_device_info));
  ldi_node->device = new_device;
  ldi_node->device_name = safe_strdup(dev_name);
  LIST_INSERT_HEAD(&ldi_head, ldi_node, entries);
}

static void detach_input_device(const char *dev_name) {
  struct li_device_info *ldi_node = NULL;
  bool found_device = false;

  ldi_node = LIST_FIRST(&ldi_head);
  while (ldi_node != NULL) {
    if (strcmp(ldi_node->device_name, dev_name) == 0) {
      found_device = true;
      break;
    }
    ldi_node = LIST_NEXT(ldi_node, entries);
  }

  if (!found_device) {
    return;
  }

  libinput_path_remove_device(ldi_node->device);
  free(ldi_node->device_name);
  LIST_REMOVE(ldi_node, entries);
  free(ldi_node);
}

/********************/
/* wayland handling */
/********************/

static void registry_handle_global(void *data, struct wl_registry *registry,
  uint32_t name, const char *interface, uint32_t version) {
  struct disp_state *state = data;
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state->compositor = wl_registry_bind(registry, name,
      &wl_compositor_interface, 5);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    if (!state->seat_set) {
      state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 9);
      state->seat_set = true;
    } else {
      fprintf(stderr,
        "WARNING: Multiple seats detected, all but first will be ignored.\n");
    }
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 2);
  } else if (strcmp(interface, wl_output_interface.name) == 0) {
    bool new_layer_allocated = false;
    for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
      if (!state->layers[i]) {
        state->outputs[i] = wl_registry_bind(registry, name,
          &wl_output_interface, 4);
        state->output_names[i] = name;
        state->layers[i] = allocate_drawable_layer(state, state->outputs[i]);
        if (state->xdg_output_manager) {
          /*
           * We can only create xdg_outputs for wl_outputs if we've received
           * the zxdg_output_manager_v1 object from the server, thus the 'if'
           * condition here. When we *do* get the zxdg_output_manager_v1
           * object, we go through and make xdg_outputs for any wl_outputs
           * that were sent too early.
           *
           * NOTE: We do not add wl_output listeners until we have the
           * zxdg_output_manager_v1 object to avoid the situation where we get
           * wl_output_done signals before an xdg_output is created for a
           * wl_output.
           */
          state->xdg_outputs[i] = zxdg_output_manager_v1_get_xdg_output(
            state->xdg_output_manager, state->outputs[i]);
          zxdg_output_v1_add_listener(state->xdg_outputs[i],
            &xdg_output_listener, state);
          wl_output_add_listener(state->outputs[i], &output_listener, state);
          state->pending_output_geometries[i] = safe_calloc(1,
            sizeof(struct output_geometry));
        }
        new_layer_allocated = true;
        break;
      }
    }
    if (!new_layer_allocated) {
      fprintf(stderr,
        "FATAL ERROR: Cannot handle more than %d displays attached at once!\n",
        MAX_DRAWABLE_LAYERS);
      exit(1);
    }
  } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
    state->xdg_output_manager = wl_registry_bind(registry, name,
      &zxdg_output_manager_v1_interface, 3);
    for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
      if ((state->outputs[i]) && (!state->xdg_outputs[i])) {
        /*
         * This is where we make xdg_outputs for any wl_outputs that were
         * sent too early.
         */
        state->xdg_outputs[i] = zxdg_output_manager_v1_get_xdg_output(
          state->xdg_output_manager, state->outputs[i]);
        zxdg_output_v1_add_listener(state->xdg_outputs[i],
          &xdg_output_listener, state);
        wl_output_add_listener(state->outputs[i], &output_listener, state);
        state->pending_output_geometries[i] = safe_calloc(1,
          sizeof(struct output_geometry));
      }
    }
  } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    state->layer_shell = wl_registry_bind(registry, name,
      &zwlr_layer_shell_v1_interface, 4);
  } else if (
    strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
    state->virt_pointer_manager = wl_registry_bind(registry, name,
      &zwlr_virtual_pointer_manager_v1_interface, 2);
    state->virt_pointer
      = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
      state->virt_pointer_manager, NULL);
  } else if (
    strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
    state->virt_kb_manager = wl_registry_bind(registry, name,
      &zwp_virtual_keyboard_manager_v1_interface, 1);
  }
}

static void registry_handle_global_remove(void *data,
  struct wl_registry *registry, uint32_t name) {
  struct disp_state *state = data;
  for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
    if (state->layers[i]) {
      if (state->output_names[i] == name) {
        struct drawable_layer *layer = state->layers[i];
        zwlr_layer_surface_v1_destroy(layer->layer_surface);
        wl_output_release(state->outputs[i]);
        state->outputs[i] = NULL;
        state->output_names[i] = 0;
        zxdg_output_v1_destroy(state->xdg_outputs[i]);
        state->xdg_outputs[i] = NULL;
        free(state->pending_output_geometries[i]);
        state->pending_output_geometries[i] = NULL;
        state->output_geometries[i] = NULL;
        wl_surface_destroy(layer->surface);
        if (layer->pixbuf != NULL) {
          assert(layer->size >= 0);
          munmap(layer->pixbuf, (size_t)(layer->size));
        }
        if (layer->shm_pool != NULL) {
          wl_shm_pool_destroy(layer->shm_pool);
        }
        free(layer);
        state->layers[i] = NULL;
        recalc_global_space(state);
        break;
      }
    }
  }
}

static void seat_handle_name(void *data, struct wl_seat *seat,
  const char *name) {
  struct disp_state *state = data;
  state->seat_name = name;
}

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
  uint32_t capabilities) {
  struct disp_state *state = data;
  state->seat_caps = capabilities;
  if (capabilities | WL_SEAT_CAPABILITY_KEYBOARD) {
    state->kb = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(state->kb, &kb_listener, state);
  } else {
    fprintf(stderr,
      "FATAL ERROR: No keyboard capability for seat, cannot continue.\n");
    exit(1);
  }
}

static void kb_handle_keymap(void *data, struct wl_keyboard *kb,
  uint32_t format, int32_t fd, uint32_t size) {
  assert(size <= INT32_MAX);
  struct disp_state *state = data;
  char *kb_map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (kb_map_shm == MAP_FAILED) {
    fprintf(stderr, "FATAL ERROR: Could not mmap xkb layout!\n");
    exit(1);
  }
  if (state->old_kb_map_shm) {
    if (strcmp(state->old_kb_map_shm, kb_map_shm) == 0) {
      /* New and old maps are the same, cleanup and return. */
      munmap(kb_map_shm, size);
      safe_close(fd);
      return;
    } else {
      assert(state->old_kb_map_shm_size >= 0);
      munmap(state->old_kb_map_shm, (size_t)(state->old_kb_map_shm_size));
    }
  }
  zwp_virtual_keyboard_v1_keymap(state->virt_kb, format, fd, size);
  state->old_kb_map_shm = kb_map_shm;
  state->old_kb_map_shm_size = (int32_t)(size);
  if (state->xkb_keymap) {
    xkb_keymap_unref(state->xkb_keymap);
  }
  state->xkb_keymap = xkb_keymap_new_from_string(
    state->xkb_ctx, kb_map_shm, XKB_KEYMAP_FORMAT_TEXT_V1,
    XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!state->xkb_keymap) {
    fprintf(stderr, "FATAL ERROR: Could not compile xkb layout!\n");
    exit(1);
  }
  safe_close(fd);
  if (state->xkb_state) {
    xkb_state_unref(state->xkb_state);
  }
  state->xkb_state = xkb_state_new(state->xkb_keymap);
  if (!state->xkb_state) {
    fprintf(stderr, "FATAL ERROR: Could not create xkb state!\n");
    exit(1);
  }
  state->virt_kb_keymap_set = true;
}

static void kb_handle_enter(void *data, struct wl_keyboard *kb,
  uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
  wl_array_release(keys);
}

static void kb_handle_leave(void *data, struct wl_keyboard *kb,
  uint32_t serial, struct wl_surface *surface) {
  ;
}

static void kb_handle_key(void *data, struct wl_keyboard *kb,
  uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
  ;
}

static void kb_handle_modifiers(void *data, struct wl_keyboard *kb,
  uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
  uint32_t mods_locked, uint32_t group) {
  ;
}

static void kb_handle_repeat_info(void *data, struct wl_keyboard *kb,
  int32_t rate, int32_t delay) {
  ;
}

static void wl_buffer_release(void *data, struct wl_buffer *buffer) {
  for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
    if (!state.layers[i]) {
      continue;
    }
    for (int32_t j = 0; j < MAX_UNRELEASED_FRAMES; j++) {
      if (state.layers[i]->buffer_list[j] == buffer) {
        state.layers[i]->frame_in_use[j] = false;
        state.layers[i]->buffer_list[j] = NULL;
        /* Blank out the location where the cursor was drawn previously */
        draw_block(
          state.layers[i]->pixbuf,
          (state.layers[i]->size * j) / 4,
          state.layers[i]->cursor_x_pos_list[j],
          state.layers[i]->cursor_y_pos_list[j],
          state.layers[i]->width,
          state.layers[i]->height,
          CURSOR_RADIUS,
          false
        );
        break;
      }
    }
  }
  wl_buffer_destroy(buffer);
}

static void wl_output_handle_geometry(void *data, struct wl_output *output,
  int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
  int32_t subpixel, const char *make, const char *model, int32_t transform) {
  ;
}

static void wl_output_handle_mode(void *data, struct wl_output *output,
  uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
  ;
}

static void wl_output_info_done(void *data, struct wl_output *output) {
  struct disp_state *state = data;
  for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
    if (state->outputs[i] == output) {
      struct output_geometry *geometry = state->pending_output_geometries[i];
      if (geometry->x == 0 && geometry->y == 0 && geometry->width == 0
        && geometry->height == 0)
        return;
      state->output_geometries[i] = geometry;
      recalc_global_space(state);
      break;
    }
  }
}

static void wl_output_handle_scale(void *data, struct wl_output *output,
  int32_t factor) {
  ;
}

static void wl_output_handle_name(void *data, struct wl_output *output,
  const char *name) {
  ;
}

static void wl_output_handle_description(void *data, struct wl_output *output,
  const char *description) {
  ;
}

static void xdg_output_handle_logical_position(void *data,
  struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y) {
  struct disp_state *state = data;
  for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
    if (state->xdg_outputs[i] == xdg_output) {
      state->pending_output_geometries[i]->x = x;
      state->pending_output_geometries[i]->y = y;
      break;
    }
  }
}

static void xdg_output_handle_logical_size(void *data,
  struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height) {
  struct disp_state *state = data;
  for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
    if (state->xdg_outputs[i] == xdg_output) {
      state->pending_output_geometries[i]->width = width;
      state->pending_output_geometries[i]->height = height;
      break;
    }
  }
}

static void xdg_output_info_done(void *data,
  struct zxdg_output_v1 *xdg_output) {
  ;
}

static void xdg_output_handle_name(void *data,
  struct zxdg_output_v1 *xdg_output, const char *name) {
  ;
}

static void xdg_output_handle_description(void *data,
  struct zxdg_output_v1 *xdg_output, const char *description) {
  ;
}

static void layer_surface_configure(void *data,
  struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial, uint32_t width,
  uint32_t height) {
  struct disp_state *state = data;
  struct drawable_layer *layer = NULL;
  for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
    if (state->layers[i]) {
      if (state->layers[i]->layer_surface == layer_surface) {
        layer = state->layers[i];
        break;
      }
    }
  }
  assert(layer != NULL);
  assert(width * 4 <= INT32_MAX);
  assert(height <= INT32_MAX);
  layer->width = (int32_t)(width);
  layer->height = (int32_t)(height);
  layer->stride = (int32_t)(width * 4);
  layer->size = layer->stride * layer->height;
  assert(layer->size >= 0);
  int shm_fd = create_shm_file(layer->size * MAX_UNRELEASED_FRAMES);
  if (shm_fd == -1) {
    fprintf(stderr,
      "FATAL ERROR: Cannot allocate shared memory block for frame: %s\n",
      strerror(errno));
    exit(1);
  }
  layer->pixbuf = mmap(NULL, (size_t)(layer->size * MAX_UNRELEASED_FRAMES), PROT_READ | PROT_WRITE,
    MAP_SHARED, shm_fd, 0);
  if (layer->pixbuf == MAP_FAILED) {
    safe_close(shm_fd);
    fprintf(stderr,
      "FATAL ERROR: Failed to map shared memory block for frame: %s\n",
      strerror(errno));
    exit(1);
  }
  layer->shm_pool = wl_shm_create_pool(state->shm, shm_fd,
    layer->size * MAX_UNRELEASED_FRAMES);

  struct wl_region *zeroed_region = wl_compositor_create_region(
    state->compositor);
  wl_region_add(zeroed_region, 0, 0, 0, 0);
  wl_surface_set_input_region(layer->surface, zeroed_region);

  zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
  layer->layer_surface_configured = true;
  wl_region_destroy(zeroed_region);
  draw_frame(layer);
}

/*********************/
/* libinput handling */
/*********************/

static int li_open_restricted(const char *path, int flags, void *user_data) {
  int fd = safe_open(path, flags | O_CLOEXEC);
  int one = 1;
  if (ioctl(fd, EVIOCGRAB, &one) < 0) {
    fprintf(stderr, "FATAL ERROR: Could not grab evdev device '%s'!\n", path);
    exit(1);
  }
  return fd < 0 ? -errno : fd;
}

static void li_close_restricted(int fd, void *user_data) {
  safe_close(fd);
}

/************************/
/* high-level functions */
/************************/

static void draw_frame(struct drawable_layer *layer) {
  int32_t chosen_frame_idx = -1;

  if (!layer->layer_surface_configured)
    return;

  for (int32_t i = 0; i < MAX_UNRELEASED_FRAMES; i++) {
    if (!layer->frame_in_use[i]) {
      chosen_frame_idx = i;
      break;
    }
  }
  if (chosen_frame_idx == -1) {
    return;
  }

  layer->frame_pending = false;

  assert(layer->size >= 0);
  assert(chosen_frame_idx >= 0);
  assert((layer->size * chosen_frame_idx)
    < (layer->size * MAX_UNRELEASED_FRAMES));
  struct wl_buffer *buffer = wl_shm_pool_create_buffer(layer->shm_pool,
    layer->size * chosen_frame_idx, layer->width, layer->height,
    layer->stride, WL_SHM_FORMAT_ARGB8888);

  assert(cursor_x < INT32_MAX && cursor_x >= 0);
  assert(cursor_y < INT32_MAX && cursor_y >= 0);
  struct screen_local_coord scr_coord = abs_coord_to_screen_local_coord(
    (int32_t)(cursor_x), (int32_t)(cursor_y));

  bool cursor_is_on_layer = false;
  if (scr_coord.valid) {
    for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
      if ((state.layers[i] == layer) && (i == scr_coord.output_idx)) {
        cursor_is_on_layer = true;
      }
    }
  }

  if (layer->last_drawn_cursor_x >= 0 && layer->last_drawn_cursor_y >= 0) {
    /* Damage the previous cursor location */
    damage_surface_enh(layer->surface,
      layer->last_drawn_cursor_x - CURSOR_RADIUS,
      layer->last_drawn_cursor_y - CURSOR_RADIUS,
      layer->last_drawn_cursor_x + CURSOR_RADIUS + 1,
      layer->last_drawn_cursor_y + CURSOR_RADIUS + 1);
  }
  if (cursor_is_on_layer) {
    /* Draw red crosshairs at the pointer location */
    draw_block(layer->pixbuf, (layer->size * chosen_frame_idx) / 4,
      scr_coord.x, scr_coord.y, layer->width, layer->height, CURSOR_RADIUS,
      true);
    damage_surface_enh(layer->surface, scr_coord.x - CURSOR_RADIUS,
      scr_coord.y - CURSOR_RADIUS, scr_coord.x + CURSOR_RADIUS + 1,
      scr_coord.y + CURSOR_RADIUS + 1);
  }

  wl_buffer_add_listener(buffer, &buffer_listener, NULL);
  layer->buffer_list[chosen_frame_idx] = buffer;
  wl_surface_attach(layer->surface, buffer, 0, 0);
  wl_surface_commit(layer->surface);
  if (cursor_is_on_layer) {
    layer->last_drawn_cursor_x = scr_coord.x;
    layer->last_drawn_cursor_y = scr_coord.y;
  } else {
    layer->last_drawn_cursor_x = -1;
    layer->last_drawn_cursor_y = -1;
  }
  layer->cursor_x_pos_list[chosen_frame_idx] = layer->last_drawn_cursor_x;
  layer->cursor_y_pos_list[chosen_frame_idx] = layer->last_drawn_cursor_y;
  layer->frame_in_use[chosen_frame_idx] = true;
}

static struct drawable_layer *allocate_drawable_layer(struct disp_state *state,
  struct wl_output *output) {
  struct drawable_layer *layer = safe_calloc(1, sizeof(struct drawable_layer));
  layer->frame_pending = true;
  layer->last_drawn_cursor_x = -1;
  layer->last_drawn_cursor_y = -1;
  for (ssize_t i = 0; i < MAX_UNRELEASED_FRAMES; i++) {
    layer->cursor_x_pos_list[i] = -1;
    layer->cursor_y_pos_list[i] = -1;
  }
  layer->output = output;
  layer->surface = wl_compositor_create_surface(state->compositor);
  if (!layer->surface) {
    fprintf(stderr, "FATAL ERROR: Could not create Wayland surface!\n");
    exit(1);
  }
  layer->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
    state->layer_shell, layer->surface, layer->output,
    ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "com.kicksecure.kloak");
  zwlr_layer_surface_v1_add_listener(layer->layer_surface,
    &layer_surface_listener, state);

  zwlr_layer_surface_v1_set_anchor(layer->layer_surface,
    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP);
  zwlr_layer_surface_v1_set_anchor(layer->layer_surface,
    ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
  zwlr_layer_surface_v1_set_anchor(layer->layer_surface,
    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
  zwlr_layer_surface_v1_set_anchor(layer->layer_surface,
    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
  zwlr_layer_surface_v1_set_exclusive_zone(layer->layer_surface, -1);
  wl_surface_commit(layer->surface);

  return layer;
}

static void damage_surface_enh(struct wl_surface *surface, int32_t x,
  int32_t y, int32_t width, int32_t height) {
  assert(width >= 0);
  assert(height >= 0);
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  wl_surface_damage_buffer(surface, x, y, width, height);
}

static struct input_packet * update_virtual_cursor() {
  assert(prev_cursor_x < INT32_MAX && prev_cursor_x >= 0);
  assert(prev_cursor_y < INT32_MAX && prev_cursor_y >= 0);
  struct screen_local_coord prev_scr_coord = abs_coord_to_screen_local_coord(
    (int32_t)(prev_cursor_x), (int32_t)(prev_cursor_y));

  if (!prev_scr_coord.valid || !state.layers[prev_scr_coord.output_idx]) {
    /* We've somehow gotten into a spot where the previous coordinate data
     * either is invalid or points at an area where there is no screen. Reset
     * everything in the hopes of recovering sanity. */
    printf("Resetting!\n");
    for (int32_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
      if (state.layers[i]) {
        struct coord sane_location = screen_local_coord_to_abs_coord(0, 0, i);
        prev_cursor_x = sane_location.x;
        prev_cursor_y = sane_location.y;
        cursor_x = sane_location.x;
        cursor_y = sane_location.y;
        assert(prev_cursor_x < INT32_MAX && prev_cursor_x >= 0);
        assert(prev_cursor_y < INT32_MAX && prev_cursor_y >= 0);
        prev_scr_coord = abs_coord_to_screen_local_coord(
          (int32_t)(prev_cursor_x), (int32_t)(prev_cursor_y));
        break;
      }
    }
  }

  /*
   * Ensure the cursor doesn't move off-screen, and recalculate its end
   * position if it would end up off-screen.
   *
   * This is a bit tricky to do since we can't just look at the intended final
   * location of the mouse and move it there if that location is valid, since
   * that would allow jumping over "voids" in the compositor global space
   * (places whether global space has a pixel but no screen covers that
   * pixel). Instead, we use the following algorithm:
   *
   * - Take the previous cursor position and treat it as a "start location".
   *   Treat the current cursor position as an "end location".
   * - Start walking in a straight line from the start location to the end
   *   location, one pixel at a time.
   * - Once we hit the end location, move the real mouse cursor there.
   * - Each time we walk forward a pixel, calculate the screen-local
   *   coordinates of that pixel, and ensure it actually is on a screen.
   * - If a pixel is NOT on any screen, determine which direction we can move
   *   to get back onto a screen. Move one pixel in that direction, then
   *   change the start location to this new position and adjust the end
   *   location so that we can walk to it moving in a straight line without
   *   moving any further in the dimension we just moved to get back to a
   *   screen. I.e., if we moved horizontally to the left one pixel to get
   *   back on a screen, move the end location so that we can walk to it in a
   *   vertical line. This allows us to "glide" along the wall.
   *
   * There may be a more efficient algorithm for this. Contributions welcome.
   */

  assert(cursor_x < INT32_MAX && cursor_x >= 0);
  assert(cursor_y < INT32_MAX && cursor_y >= 0);
  struct coord start = {
    .x = (int32_t)(prev_cursor_x),
    .y = (int32_t)(prev_cursor_y),
  };
  struct coord end = {
    .x = (int32_t)(cursor_x),
    .y = (int32_t)(cursor_y),
  };
  struct coord prev_trav_coord = start;
  bool end_x_hit = false;
  bool end_y_hit = false;
  for (int32_t i = 0; ; i++) {
    struct coord trav_coord = traverse_line(start, end, i);
    if (trav_coord.x == end.x) end_x_hit = true;
    if (trav_coord.y == end.y) end_y_hit = true;
    struct screen_local_coord trav_scr_coord
      = abs_coord_to_screen_local_coord(trav_coord.x, trav_coord.y);
    if (!trav_scr_coord.valid) {
      /*
       * Figure out what direction we moved when we went off screen, and move
       * move backwards in that direction, but in only one dimension.
       */
      if (prev_trav_coord.x < trav_coord.x) {
        trav_scr_coord = abs_coord_to_screen_local_coord(trav_coord.x - 1,
          trav_coord.y);
        if (trav_scr_coord.valid) {
          start.x = trav_coord.x - 1;
          start.y = trav_coord.y;
          end.x = trav_coord.x - 1;
          i = -1;
          continue;
        }
      }
      if (prev_trav_coord.x > trav_coord.x) {
        trav_scr_coord = abs_coord_to_screen_local_coord(trav_coord.x + 1,
          trav_coord.y);
        if (trav_scr_coord.valid) {
          start.x = trav_coord.x + 1;
          start.y = trav_coord.y;
          end.x = trav_coord.x + 1;
          i = -1;
          continue;
        }
      }
      if (prev_trav_coord.y < trav_coord.y) {
        trav_scr_coord = abs_coord_to_screen_local_coord(trav_coord.x,
          trav_coord.y - 1);
        if (trav_scr_coord.valid) {
          start.y = trav_coord.y - 1;
          start.x = trav_coord.x;
          end.y = trav_coord.y -1;
          i = -1;
          continue;
        }
      }
      if (prev_trav_coord.y > trav_coord.y) {
        trav_scr_coord = abs_coord_to_screen_local_coord(trav_coord.x,
          trav_coord.y + 1);
        if (trav_scr_coord.valid) {
          start.y = trav_coord.y + 1;
          start.x = trav_coord.x;
          end.y = trav_coord.y + 1;
          i = -1;
          continue;
        }
      }
      /* This should never be reached, but just in case... */
      abort();
    }
    if (end_x_hit && end_y_hit) {
      if ((int32_t)(cursor_x) != end.x) {
        cursor_x = end.x;
      }
      if ((int32_t)(cursor_y) != end.y) {
        cursor_y = end.y;
      }
      break;
    }
    prev_trav_coord = trav_coord;
  }
  assert(cursor_x < INT32_MAX && cursor_x >= 0);
  assert(cursor_y < INT32_MAX && cursor_y >= 0);
  struct screen_local_coord scr_coord = abs_coord_to_screen_local_coord(
    (int32_t)(cursor_x), (int32_t)(cursor_y));

  state.layers[prev_scr_coord.output_idx]->frame_pending = true;
  state.layers[scr_coord.output_idx]->frame_pending = true;

  struct input_packet *old_ev_packet;
  /* = rather than == is intentional here */
  if ((old_ev_packet = TAILQ_LAST(&evq_head, tailhead_evq))
    && (!old_ev_packet->is_libinput)) {
    old_ev_packet->cursor_x = (int32_t)(cursor_x);
    old_ev_packet->cursor_y = (int32_t)(cursor_y);
    return NULL;
  } else {
    struct input_packet *ev_packet = safe_calloc(1, sizeof(struct input_packet));
    if (ev_packet == NULL) {
      fprintf(stderr,
        "FATAL ERROR: Could not allocate memory for libinput event packet!\n");
      exit(1);
    }
    ev_packet->is_libinput = false;
    ev_packet->cursor_x = (int32_t)(cursor_x);
    ev_packet->cursor_y = (int32_t)(cursor_y);
    return ev_packet;
  }
}

static void handle_libinput_event(enum libinput_event_type ev_type,
  struct libinput_event *li_event, uint32_t ts_milliseconds) {
  bool mouse_event_handled = false;

  if (ev_type == LIBINPUT_EVENT_DEVICE_ADDED) {
    struct libinput_device *new_dev = libinput_event_get_device(li_event);
    int can_tap = libinput_device_config_tap_get_finger_count(new_dev);
    if (can_tap) {
      libinput_device_config_tap_set_enabled(new_dev,
        LIBINPUT_CONFIG_TAP_ENABLED);
    }

  } else if (ev_type == LIBINPUT_EVENT_POINTER_BUTTON) {
    mouse_event_handled = true;
    struct libinput_event_pointer *pointer_event
      = libinput_event_get_pointer_event(li_event);
    uint32_t button_code = libinput_event_pointer_get_button(pointer_event);
    enum libinput_button_state button_state
      = libinput_event_pointer_get_button_state(pointer_event);
    if (button_state == LIBINPUT_BUTTON_STATE_PRESSED) {
      /* Both libinput and zwlr_virtual_pointer_v1 use evdev event codes to
       * identify the button pressed, so we can just pass the data from
       * libinput straight through */
      zwlr_virtual_pointer_v1_button(
        state.virt_pointer, ts_milliseconds, button_code,
        WL_POINTER_BUTTON_STATE_PRESSED);
    } else {
      zwlr_virtual_pointer_v1_button(
        state.virt_pointer, ts_milliseconds, button_code,
        WL_POINTER_BUTTON_STATE_RELEASED);
    }

  } else if (ev_type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL
    || ev_type == LIBINPUT_EVENT_POINTER_SCROLL_FINGER
    || ev_type == LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS) {
    mouse_event_handled = true;
    struct libinput_event_pointer *pointer_event
      = libinput_event_get_pointer_event(li_event);
    int vert_scroll_present = libinput_event_pointer_has_axis(pointer_event,
      LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
    int horiz_scroll_present = libinput_event_pointer_has_axis(pointer_event,
      LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);

    if (vert_scroll_present) {
      double vert_scroll_value = libinput_event_pointer_get_scroll_value(
        pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
      if (vert_scroll_value == 0) {
        zwlr_virtual_pointer_v1_axis_stop(state.virt_pointer,
          ts_milliseconds, WL_POINTER_AXIS_VERTICAL_SCROLL);
      } else {
        zwlr_virtual_pointer_v1_axis(state.virt_pointer, ts_milliseconds,
          WL_POINTER_AXIS_VERTICAL_SCROLL,
          wl_fixed_from_double(vert_scroll_value));
      }
      switch (ev_type) {
        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
          zwlr_virtual_pointer_v1_axis_source(state.virt_pointer,
            WL_POINTER_AXIS_SOURCE_WHEEL);
          break;
        case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
          zwlr_virtual_pointer_v1_axis_source(state.virt_pointer,
            WL_POINTER_AXIS_SOURCE_FINGER);
          break;
        case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
          zwlr_virtual_pointer_v1_axis_source(state.virt_pointer,
            WL_POINTER_AXIS_SOURCE_CONTINUOUS);
          break;
        default:
          abort();
      }
    }

    if (horiz_scroll_present) {
      double horiz_scroll_value = libinput_event_pointer_get_scroll_value(
        pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
      if (horiz_scroll_value == 0) {
        zwlr_virtual_pointer_v1_axis_stop(state.virt_pointer,
          ts_milliseconds, WL_POINTER_AXIS_HORIZONTAL_SCROLL);
      } else {
        zwlr_virtual_pointer_v1_axis(state.virt_pointer,
          ts_milliseconds, WL_POINTER_AXIS_HORIZONTAL_SCROLL,
          wl_fixed_from_double(horiz_scroll_value));
      }
      switch (ev_type) {
        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
          zwlr_virtual_pointer_v1_axis_source(state.virt_pointer,
            WL_POINTER_AXIS_SOURCE_WHEEL);
          break;
        case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
          zwlr_virtual_pointer_v1_axis_source(state.virt_pointer,
            WL_POINTER_AXIS_SOURCE_FINGER);
          break;
        case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
          zwlr_virtual_pointer_v1_axis_source(state.virt_pointer,
            WL_POINTER_AXIS_SOURCE_CONTINUOUS);
          break;
        default:
          abort();
      }
    }

  } else if (ev_type == LIBINPUT_EVENT_KEYBOARD_KEY) {
    if (state.virt_kb_keymap_set) {
      struct libinput_event_keyboard *kb_event
        = libinput_event_get_keyboard_event(li_event);
      uint32_t key = libinput_event_keyboard_get_key(kb_event);
      enum libinput_key_state key_state
        = libinput_event_keyboard_get_key_state(kb_event);
      if (key_state == LIBINPUT_KEY_STATE_PRESSED) {
        /* XKB keycodes == evdev keycodes + 8. Why this design decision was
         * made, I have no idea. */
        xkb_state_update_key(state.xkb_state, key + 8, XKB_KEY_DOWN);
      } else {
        xkb_state_update_key(state.xkb_state, key + 8, XKB_KEY_UP);
      }
      xkb_mod_mask_t depressed_mods = xkb_state_serialize_mods(
        state.xkb_state, XKB_STATE_MODS_DEPRESSED);
      xkb_mod_mask_t latched_mods = xkb_state_serialize_mods(
        state.xkb_state, XKB_STATE_MODS_LATCHED);
      xkb_mod_mask_t locked_mods = xkb_state_serialize_mods(
        state.xkb_state, XKB_STATE_MODS_LOCKED);
      xkb_layout_index_t effective_group = xkb_state_serialize_layout(
        state.xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
      zwp_virtual_keyboard_v1_modifiers(state.virt_kb, depressed_mods,
        latched_mods, locked_mods, effective_group);
      zwp_virtual_keyboard_v1_key(state.virt_kb, ts_milliseconds, key,
        key_state);
    }
  }

  if (mouse_event_handled) {
    zwlr_virtual_pointer_v1_frame(state.virt_pointer);
  }
  libinput_event_destroy(li_event);
}

static void register_esc_combo_event(
  enum libinput_event_type li_event_type, struct libinput_event *li_event) {
  struct libinput_event_keyboard *kb_event = NULL;
  uint32_t key = 0;
  enum libinput_key_state key_state = LIBINPUT_KEY_STATE_PRESSED;
  size_t i = 0;
  size_t j = 0;
  bool hit_exit = true;

  if (li_event_type != LIBINPUT_EVENT_KEYBOARD_KEY) {
    return;
  }

  kb_event = libinput_event_get_keyboard_event(li_event);
  key = libinput_event_keyboard_get_key(kb_event);
  key_state = libinput_event_keyboard_get_key_state(kb_event);

  for (i = 0; i < esc_key_list_len; i++) {
    for (j = 0; j < esc_key_sublist_len[i]; j++) {
      if (esc_key_list[i][j] != key) {
        continue;
      }

      if (key_state == LIBINPUT_KEY_STATE_PRESSED) {
        active_esc_key_list[i] = true;
      } else {
        active_esc_key_list[i] = false;
      }
      break;
    }
  }

  for (i = 0; i < esc_key_list_len; i++) {
    if (!active_esc_key_list[i]) {
      hit_exit = false;
      break;
    }
  }

  if (hit_exit) {
    exit(0);
  }
}

static void queue_libinput_event_and_relocate_virtual_cursor(
  enum libinput_event_type li_event_type, struct libinput_event *li_event) {
  struct input_packet *ev_packet;

  if (li_event_type == LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE) {
    struct libinput_event_pointer *pointer_event
      = libinput_event_get_pointer_event(li_event);
    assert(state.global_space_width >= 0);
    assert(state.global_space_height >= 0);
    double abs_x = libinput_event_pointer_get_absolute_x_transformed(
      pointer_event, (uint32_t)(state.global_space_width));
    double abs_y = libinput_event_pointer_get_absolute_y_transformed(
      pointer_event, (uint32_t)(state.global_space_height));
    prev_cursor_x = cursor_x;
    prev_cursor_y = cursor_y;
    cursor_x = abs_x;
    cursor_y = abs_y;
    ev_packet = update_virtual_cursor();
    libinput_event_destroy(li_event);
    if (!ev_packet) {
      return;
    }

  } else if (li_event_type == LIBINPUT_EVENT_POINTER_MOTION) {
    struct libinput_event_pointer *pointer_event
      = libinput_event_get_pointer_event(li_event);
    double rel_x = libinput_event_pointer_get_dx(pointer_event);
    double rel_y = libinput_event_pointer_get_dy(pointer_event);
    prev_cursor_x = cursor_x;
    prev_cursor_y = cursor_y;
    cursor_x += rel_x;
    cursor_y += rel_y;
    if (cursor_x < state.pointer_space_x) cursor_x = state.pointer_space_x;
    if (cursor_y < state.pointer_space_y) cursor_y = state.pointer_space_y;
    if (cursor_x > state.global_space_width - 1)
      cursor_x = state.global_space_width - 1;
    if (cursor_y > state.global_space_height - 1)
      cursor_y = state.global_space_height - 1;
    ev_packet = update_virtual_cursor();
    libinput_event_destroy(li_event);
    if (!ev_packet) {
      return;
    }

  } else {
    ev_packet = safe_calloc(1, sizeof(struct input_packet));
    if (ev_packet == NULL) {
      fprintf(stderr,
        "FATAL ERROR: Could not allocate memory for libinput event packet!\n");
      exit(1);
    }
    ev_packet->is_libinput = true;
    ev_packet->li_event = li_event;
    ev_packet->li_event_type = li_event_type;
  }

  int64_t current_time = current_time_ms();
  int64_t lower_bound = min(max(prev_release_time - current_time, 0),
    max_delay);
  int64_t random_delay = random_between(lower_bound, max_delay);
  ev_packet->sched_time = current_time + random_delay;
  TAILQ_INSERT_TAIL(&evq_head, ev_packet, entries);
  prev_release_time = ev_packet->sched_time;
}

static void release_scheduled_input_events(void) {
  int64_t current_time = current_time_ms();
  struct input_packet *packet = NULL;
  while ((packet = TAILQ_FIRST(&evq_head))
    && (current_time >= packet->sched_time)) {
    if (packet->is_libinput) {
      assert(packet->sched_time >= 0);
      if (packet->sched_time > UINT32_MAX) {
        fprintf(stderr,
          "packet->sched_time overflowed maximum value. This is not an error, but kloak must be restarted. Exiting.");
        exit(0);
      }
      handle_libinput_event(packet->li_event_type, packet->li_event,
        (uint32_t)(packet->sched_time));
    } else {
      assert(packet->sched_time >= 0);
      if (packet->sched_time > UINT32_MAX) {
        fprintf(stderr,
          "packet->sched_time overflowed maximum value. This is not an error, but kloak must be restarted. Exiting.");
        exit(0);
      }
      assert(state.pointer_space_x >= 0);
      assert(state.pointer_space_y >= 0);
      assert(packet->cursor_x >= state.pointer_space_x);
      assert(packet->cursor_y >= state.pointer_space_y);
      assert(state.global_space_width >= state.pointer_space_x);
      assert(state.global_space_height >= state.pointer_space_y);
      zwlr_virtual_pointer_v1_motion_absolute(
        state.virt_pointer, (uint32_t)(packet->sched_time),
        (uint32_t)(packet->cursor_x - state.pointer_space_x),
        (uint32_t)(packet->cursor_y - state.pointer_space_y),
        (uint32_t)(state.global_space_width - state.pointer_space_x),
        (uint32_t)(state.global_space_height - state.pointer_space_y));
      zwlr_virtual_pointer_v1_frame(state.virt_pointer);
    }
    TAILQ_REMOVE(&evq_head, packet, entries);
    free(packet);
  }
}

static void handle_inotify_events(void) {
  static char *read_buf = NULL;
  ssize_t read_len = 0;
  ssize_t rem_len = 0;
  ssize_t struct_len = 0;
  struct inotify_event *ie;

  if (read_buf == NULL) {
    read_buf = safe_calloc(INOTIFY_READ_BUF_LEN, sizeof(char));
  }

  while (true) {
    read_len = read(inotify_fd, read_buf, INOTIFY_READ_BUF_LEN);
    if (read_len == -1) {
      if (errno == EINTR) {
        continue;
      }
      fprintf(stderr,
        "FATAL ERROR: Could not read from inotify file descriptor: %s\n",
        strerror(errno));
      exit(1);
    }
    break;
  }

  ie = (void *)(read_buf);
  rem_len = read_len;
  while (true) {
    struct_len = ((ssize_t)(sizeof(struct inotify_event)) + (ssize_t)(ie->len));
    assert(struct_len <= rem_len);
    rem_len -= struct_len;
    assert(rem_len >= 0);

    if (strncmp(ie->name, "event", strlen("event")) == 0) {
      if (ie->mask & IN_CREATE) {
        attach_input_device(ie->name);
      } else {
        detach_input_device(ie->name);
      }
    }

    if (rem_len <= 0) {
      break;
    }

    ie = (struct inotify_event *)((char *)(ie) + struct_len);
  }
}

static void parse_esc_key_str(const char *esc_key_str) {
  char *esc_key_str_copy = safe_strdup(esc_key_str);
  char *orig_key_str_copy = esc_key_str_copy;
  char *root_token = NULL;
  char *sub_token = NULL;
  size_t i = 0;
  size_t j = 0;

  for (i = 0; ((root_token = strsep(&esc_key_str_copy, ",")) != NULL); i++) {
    if (root_token[0] == '\0' ) {
      fprintf(stderr,
        "FATAL ERROR: Empty key name specified in escape key list!\n");
      exit(1);
    }

    esc_key_list_len++;
    esc_key_list = safe_reallocarray(esc_key_list, esc_key_list_len,
      sizeof(uint32_t *));
    esc_key_sublist_len = safe_reallocarray(esc_key_sublist_len,
      esc_key_list_len, sizeof(size_t));
    active_esc_key_list = safe_reallocarray(active_esc_key_list,
      esc_key_list_len, sizeof(bool));
    esc_key_list[i] = NULL;
    esc_key_sublist_len[i] = 0;
    active_esc_key_list[i] = false;

    for (j = 0; ((sub_token = strsep(&root_token, "|")) != NULL); j++)  {
      if (sub_token[0] == '\0') {
        fprintf(stderr,
          "FATAL ERROR: Empty key name specified in escape key list!\n");
        exit(1);
      }

      esc_key_sublist_len[i]++;
      esc_key_list[i] = safe_reallocarray(esc_key_list[i],
        esc_key_sublist_len[i], sizeof(uint32_t));
      esc_key_list[i][j] = lookup_keycode(sub_token);
      if (esc_key_list[i][j] == 0) {
        fprintf(stderr, "FATAL ERROR: Unrecognized Key name '%s'!\n",
          sub_token);
        exit(1);
      }
    }
  }

  free(orig_key_str_copy);
}

static int calc_poll_timeout(void) {
  struct input_packet *packet = NULL;
  int64_t timeout_duration = 0;

  packet = TAILQ_FIRST(&evq_head);
  if (packet == NULL) {
    return -1;
  }

  timeout_duration = packet->sched_time - current_time_ms();
  if (timeout_duration < 0) {
    return 0;
  }
  if (timeout_duration > INT_MAX) {
    return INT_MAX;
  }
  return (int)(timeout_duration);
}

static void print_usage(void) {
  fprintf(stderr, "Usage: kloak [options]\n");
  fprintf(stderr, "Anonymizes keyboard and mouse input patterns by injecting jitter into input\n");
  fprintf(stderr, "events. Designed specifically for wlroots-based Wayland compositors. Will NOT\n");
  fprintf(stderr, "work with X11.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -h, --help\n");
  fprintf(stderr, "    Print help.\n");
  fprintf(stderr, "  -d, --delay=milliseconds\n");
  fprintf(stderr, "    Configure the maximum delay of released events. Default is 100.\n");
  fprintf(stderr, "  -s, --start-delay=milliseconds\n");
  fprintf(stderr, "    Configure the time to wait before startup. Default is 500.\n");
  fprintf(stderr, "  -c, --color=AARRGGBB\n");
  fprintf(stderr, "    Configure the color to use for the virtual mouse cursor. Default is\n");
  fprintf(stderr, "    ffff0000 (solid red).\n");
  fprintf(stderr, "  -k, --esc-key-combo=KEY_![,KEY_2|KEY_3...]\n");
  fprintf(stderr, "    Specify the key combination that will terminate kloak. Keys are separated\n");
  fprintf(stderr, "    by commas. Keys can be aliased to each other by separating them with a\n");
  fprintf(stderr, "    pipe (|) character. Default is KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_ESC.\n");
}

/****************************/
/* initialization functions */
/****************************/

static void applayer_random_init(void) {
  randfd = safe_open("/dev/urandom", O_RDONLY | O_CLOEXEC);
}

static void applayer_wayland_init(void) {
  /*
   * Technically we also initialize xkbcommon here, but it's only involved
   * because it's important for sending key events to Wayland.
   */
  state.display = wl_display_connect(NULL);
  if (!state.display) {
    fprintf(stderr, "FATAL ERROR: Could not get Wayland display!\n");
    exit(1);
  }
  state.display_fd = wl_display_get_fd(state.display);

  state.registry = wl_display_get_registry(state.display);
  if (!state.registry) {
    fprintf(stderr, "FATAL ERROR: Could not get Wayland registry!\n");
    exit(1);
  }
  wl_registry_add_listener(state.registry, &registry_listener, &state);
  wl_display_roundtrip(state.display);

  /*
   * At this point, most of the Wayland state should be initialized and set
   * up. Double-check it though, we don't know what Wayland compositor we're
   * running on and we don't want to crash later because the compositor
   * doesn't support a feature we need.
   */
  if (state.shm == NULL) {
    fprintf(stderr, "FATAL ERROR: No wl_shm object from compositor!\n");
    exit(1);
  }
  if (state.compositor == NULL) {
    fprintf(stderr,
      "FATAL ERROR: No wl_compositor object from compositor!\n");
    exit(1);
  }
  if (state.seat == NULL) {
    fprintf(stderr, "FATAL ERROR: No wl_seat object from compositor!\n");
    exit(1);
  }
  if (state.outputs[0] == NULL) {
    fprintf(stderr, "FATAL ERROR: No wl_output objects from compositor!\n");
    exit(1);
  }
  if (state.xdg_output_manager == NULL) {
    fprintf(stderr,
      "FATAL ERROR: No zxdg_output_manager_v1 object from compositor!\n");
    exit(1);
  }
  if (state.xdg_outputs[0] == NULL) {
    fprintf(stderr,
      "FATAL ERROR: No zxdg_output_v1 objects from compositor!\n");
    exit(1);
  }
  if (state.layer_shell == NULL) {
    fprintf(stderr,
      "FATAL ERROR: No zwlr_layer_shell_v1 object from compositor!\n");
    exit(1);
  }
  if (state.virt_pointer_manager == NULL) {
    fprintf(stderr,
      "FATAL ERROR: No zwlr_virtual_pointer_manager_v1 object from compositor!\n");
    exit(1);
  }
  if (state.virt_kb_manager == NULL) {
    fprintf(stderr,
      "FATAL ERROR: No zwp_virtual_keyboard_manager_v1 object from compositor!\n");
    exit(1);
  }
  if (state.virt_pointer == NULL) {
    fprintf(stderr,
      "FATAL ERROR: No zwlr_virtual_pointer_v1 object from compositor!\n");
    exit(1);
  }

  state.virt_kb = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
    state.virt_kb_manager, state.seat);
  /*
   * The virtual-keyboard-v1 protocol returns 0 when making a new virtual
   * keyboard if kloak is unauthorized to create a virtual keyboard. However,
   * the protocol treats this as an enum value, meaning... we have to compare
   * a pointer to an enum. This is horrible and the protocol really shouldn't
   * require this, but it does, see
   * https://wayland.app/protocols/virtual-keyboard-unstable-v1#zwp_virtual_keyboard_manager_v1:request:create_virtual_keyboard
   */
  if ((uintptr_t)(state.virt_kb)
    == (uintptr_t)(ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_ERROR_UNAUTHORIZED)) {
    fprintf(stderr,
      "Not authorized to create a virtual keyboard! Bailing out.\n");
    exit(1);
  }
  wl_seat_add_listener(state.seat, &seat_listener, &state);

  state.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!state.xkb_ctx) {
    fprintf(stderr, "FATAL ERROR: Could not create XKB context!\n");
    exit(1);
  }

  /* Make sure any remaining allocations and similar have finished */
  wl_display_roundtrip(state.display);
  if (state.kb == NULL) {
    fprintf(stderr, "FATAL ERROR: No wl_keyboard object from compositor!\n");
    exit(1);
  }
}

static void applayer_libinput_init(void) {
  DIR *input_dir = NULL;
  struct dirent *input_dir_entry = NULL;

  LIST_INIT(&ldi_head);

  li = libinput_path_create_context(&li_interface, NULL);

  input_dir = safe_opendir("/dev/input", false);
  for (input_dir_entry = readdir(input_dir); input_dir_entry != NULL;
    input_dir_entry = readdir(input_dir)) {
    if (input_dir_entry->d_type != DT_CHR) {
      continue;
    }

    if (strncmp(input_dir_entry->d_name, "event", strlen("event")) != 0) {
      continue;
    }

    attach_input_device(input_dir_entry->d_name);
  }
  safe_closedir(input_dir);

  TAILQ_INIT(&evq_head);
}

static void applayer_inotify_init(void) {
  inotify_fd = inotify_init1(IN_CLOEXEC);
  if (inotify_fd == -1) {
    fprintf(stderr, "FATAL ERROR: Could not initialize inotify: %s\n",
      strerror(errno));
    exit(1);
  }

  if (inotify_add_watch(inotify_fd, "/dev/input", IN_CREATE | IN_DELETE)
    == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not add inotify watch on /dev/input: %s\n",
      strerror(errno));
    exit(1);
  }
}

static void applayer_poll_init(void) {
  /*
   * ev_fds[0] = Wayland server file descriptor
   * ev_fds[1] = libinput file descriptor
   * ev_fds[2] = inotify file descriptor for libinput hotplug
   */
  ev_fds = safe_calloc(POLL_FD_COUNT, sizeof(struct pollfd));
  ev_fds[0].fd = state.display_fd;
  ev_fds[0].events = POLLIN;
  ev_fds[1].fd = libinput_get_fd(li);
  ev_fds[1].events = POLLIN;
  ev_fds[2].fd = inotify_fd;
  ev_fds[2].events = POLLIN;
}

static void parse_cli_args(int argc, char **argv) {
  const char *optstring = "d:s:h";
  static struct option optarr[] = {
    {"delay", required_argument, NULL, 'd'},
    {"start-delay", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {"color", required_argument, NULL, 'c'},
    {"esc-key-combo", required_argument, NULL, 'k'},
    {0, 0, 0, 0}
  };
  int getopt_rslt = 0;

  while(true) {
    getopt_rslt = getopt_long(argc, argv, optstring, optarr, NULL);
    if (getopt_rslt == -1) {
      break;
    } else if (getopt_rslt == '?') {
      print_usage();
      exit(1);
    } else if (getopt_rslt == 'd') {
      max_delay = parse_uint31_arg("delay", optarg, 10);
    } else if (getopt_rslt == 's') {
      startup_delay = parse_uint31_arg("start-delay", optarg, 10);
    } else if (getopt_rslt == 'c') {
      cursor_color = parse_uint32_arg("color", optarg, 16);
    } else if (getopt_rslt == 'k') {
      parse_esc_key_str(optarg);
    } else if (getopt_rslt == 'h') {
      print_usage();
      exit(0);
    } else {
      print_usage();
      exit(1);
    }
  }

  if (esc_key_list == NULL) {
    parse_esc_key_str(default_esc_key_str);
  }
}

/**********/
/**********/
/** MAIN **/
/**********/
/**********/

int main(int argc, char **argv) {
  /*
   * BIG FAT WARNING: Do not attempt to build kloak with NDEBUG defined. Many
   * of the assertions in this code are essential for security, and building
   * kloak with NDEBUG defined will turn all of them off. Systems running a
   * build of kloak with NDEBUG defined should be treated as compromised if
   * they process any form of untrusted data.
   *
   * To lower the risk of this situation occurring, the following check will
   * render kloak non-functional if NDEBUG is defined. Think very carefully
   * about what you are doing if you are considering patching this check out.
   */
#ifdef NDEBUG
  fprintf(stderr,
    "FATAL ERROR: Built with NDEBUG set. kloak does not support this, please rebuild with NDEBUG unset.\n");
  exit(1);
#endif

  if (getuid() != 0) {
    fprintf(stderr, "FATAL ERROR: Must be run as root!\n");
    exit(1);
  }

  /* Make sure that locales don't try to ruin our day */
  if (setenv("LC_ALL", "C", 1) == -1) {
    fprintf(stderr, "FATAL ERROR: Could not set LC_ALL=C!\n");
    exit(1);
  }

  parse_cli_args(argc, argv);
  if (sleep_ms(startup_delay) != 0) {
    fprintf(stderr,
      "FATAL ERROR: Could not sleep for requested start duration!\n");
    exit(1);
  }

  applayer_random_init();
  applayer_wayland_init();
  applayer_libinput_init();
  applayer_inotify_init();
  applayer_poll_init();

  while (true) {
    while (wl_display_prepare_read(state.display) != 0) {
      wl_display_dispatch_pending(state.display);
    }
    wl_display_flush(state.display);

    while (true) {
      enum libinput_event_type next_ev_type = libinput_next_event_type(li);
      if (next_ev_type == LIBINPUT_EVENT_NONE)
        break;
      struct libinput_event *li_event = libinput_get_event(li);
      register_esc_combo_event(next_ev_type, li_event);
      queue_libinput_event_and_relocate_virtual_cursor(next_ev_type,
        li_event);
    }

    release_scheduled_input_events();

    for (ssize_t i = 0; i < MAX_DRAWABLE_LAYERS; i++) {
      if (!state.layers[i]) {
        continue;
      }
      if (state.layers[i]->frame_pending) {
        draw_frame(state.layers[i]);
      }
    }
    wl_display_flush(state.display);

    poll(ev_fds, POLL_FD_COUNT, calc_poll_timeout());

    if (ev_fds[0].revents & POLLIN) {
      wl_display_read_events(state.display);
      wl_display_dispatch_pending(state.display);
    } else {
      wl_display_cancel_read(state.display);
    }
    ev_fds[0].revents = 0;

    if (ev_fds[1].revents & POLLIN) {
      libinput_dispatch(li);
    }
    ev_fds[1].revents = 0;

    if (ev_fds[2].revents & POLLIN) {
      handle_inotify_events();
    }
    ev_fds[2].revents = 0;
  }

  wl_display_disconnect(state.display);
  return 0;
}
