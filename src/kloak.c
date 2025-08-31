/*
 * Copyright (c) 2025 - 2025 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
 * See the file COPYING for copying conditions.
 */

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

#include <wayland-client.h>
#include "xdg-output-protocol.h"
#include "wlr-layer-shell.h"
#include "wlr-virtual-pointer.h"
#include "virtual-keyboard.h"

#include <libudev.h>
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
static struct libinput *li;
static struct udev *udev_ctx;

static struct pollfd *ev_fds;

static int64_t prev_release_time = 0;
static int32_t max_delay = DEFAULT_MAX_DELAY_MS;
static int32_t startup_delay = DEFAULT_STARTUP_TIMEOUT_MS;
static TAILQ_HEAD(tailhead, input_packet) head;

int randfd;

/*********************/
/* utility functions */
/*********************/

static void read_random(char *buf, size_t len) {
  if (read(randfd, buf, len) < len) {
    fprintf(stderr,
      "FATAL ERROR: Could not read %d byte(s) from /dev/urandom!\n", len);
    exit(1);
  }
}

static void randname(char *buf, size_t len) {
  char randchar = 0;
  for (size_t i = 0; i < len; ++i) {
    do {
      read_random(&randchar, 1);
      if (randchar & 0x80)
        randchar ^= 0x80;
    } while (randchar >= (127 - 127 % 52));

    randchar %= 52;
    if (randchar < 26) {
      randchar += 65;
    } else {
      randchar += 71;
    }
    buf[i] = randchar;
  }
}

static int create_shm_file(size_t size) {
  int32_t retries = 100;
  int fd = -1;
  do {
    char name[] = "/kloak-XXXXXXXXXX";
    randname(name + sizeof(name) - 11, 10);
    --retries;
    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) {
      shm_unlink(name);
      break;
    }
  } while (retries > 0 && errno == EEXIST);

  if (fd == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not create shared memory fd: Resource temporarily unavailable\n");
    exit(1);
  }

  int32_t ret;
  do {
    ret = ftruncate(fd, size);
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
  return (spec.tv_sec) * 1000 + (spec.tv_nsec) / 1000000;
}

static int64_t random_between(int64_t lower, int64_t upper) {
  uint64_t maxval;
  union rand_uint64 randval;
  /* default to max if the interval is not valid */
  if (lower >= upper) {
    return upper;
  }
  if (lower < 0 || upper < 0) {
    return 0;
  }

  maxval = upper - lower + 1;
  do {
    read_random(randval.raw, sizeof(uint64_t));
  } while (randval.val >= (UINT64_MAX - UINT64_MAX % maxval));

  randval.val %= maxval;
  uint64_t out_val = lower + randval.val;
  if (out_val > INT64_MAX) {
    return 0;
  }
  return (int64_t) lower + randval.val;
}

static bool check_point_in_area(uint32_t x, uint32_t y, uint32_t rect_x,
  uint32_t rect_y, uint32_t rect_width, uint32_t rect_height) {
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
   *
   * TODO: Do we actually *want* to allow touching at the corners? kloak's
   * current algorithm seems to deal with that particular edge case
   * acceptably well...
   */

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
  uint32_t ul_corner_x = UINT32_MAX;
  uint32_t ul_corner_y = UINT32_MAX;
  uint32_t br_corner_x = 0;
  uint32_t br_corner_y = 0;

  struct output_geometry *screen_list[MAX_DRAWABLE_LAYERS];
  size_t screen_list_len = 0;

  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
    if (!state->output_geometries[i])
      continue;
    screen_list[screen_list_len] = state->output_geometries[i];
    ++screen_list_len;
    if (state->output_geometries[i]->x < ul_corner_x) {
      ul_corner_x = state->output_geometries[i]->x;
    }
    if (state->output_geometries[i]->y < ul_corner_y) {
      ul_corner_y = state->output_geometries[i]->y;
    }
    uint32_t temp_br_x
      = state->output_geometries[i]->x + state->output_geometries[i]->width;
    uint32_t temp_br_y
      = state->output_geometries[i]->y + state->output_geometries[i]->height;
    if (temp_br_x > br_corner_x) {
      br_corner_x = temp_br_x;
    }
    if (temp_br_y > br_corner_y) {
      br_corner_y = temp_br_y;
    }
  }

  if (ul_corner_x > br_corner_x) {
    /* Maybe we just haven't gotten a valid screen state yet, silently fail */
    return;
  }
  if (ul_corner_y > br_corner_y) {
    /* same as above */
    return;
  }

  struct output_geometry *conn_screen_list[MAX_DRAWABLE_LAYERS];
  conn_screen_list[0] = screen_list[0];
  size_t conn_screen_list_len = 1;

  /*
   * Check for gaps between the screens. We don't support running if gaps are
   * present.
   *
   * If a screen's left edge touches another screen, current_screen->x ==
   * left_screen->x + left_screen->width.
   * If a screen's right edge touches another screen, current_screen->x +
   * current_screen->width == right_screen->x
   * If a screen's top edge touches another screen, current_screen->y ==
   * up_screen->y + up_screen->height
   * If a screen's bottom edge touches another screen, current_screen->y +
   * current_screen->height == bottom_screen->y
   */
  for (size_t i = 0; i < conn_screen_list_len; ++i) {
    for (size_t j = 0; j < screen_list_len; ++j) {
      bool screen_in_conn_list = false;
      for (size_t k = 0; k < conn_screen_list_len; ++k) {
        if (screen_list[j] == conn_screen_list[k]) {
          screen_in_conn_list = true;
          break;
        }
      }
      if (screen_in_conn_list)
        continue;
      struct output_geometry *conn_screen = conn_screen_list[i];
      struct output_geometry *cur_screen = screen_list[j];
      if (check_screen_touch(*conn_screen, *cur_screen)) {
        /* Found a touching screen! */
        conn_screen_list[conn_screen_list_len] = cur_screen;
        ++conn_screen_list_len;
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

  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
    if (!state.output_geometries[i]) {
      continue;
    }
    if (!(x >= state.output_geometries[i]->x)) {
      continue;
    }
    if (!(y >= state.output_geometries[i]->y)) {
      continue;
    }
    if (!(x < state.output_geometries[i]->x
      + state.output_geometries[i]->width)) {
      continue;
    }
    if (!(y < state.output_geometries[i]->y
      + state.output_geometries[i]->height)) {
      continue;
    }
    out_data.output_idx = i;
    out_data.x = x - state.output_geometries[i]->x;
    out_data.y = y - state.output_geometries[i]->y;
    out_data.valid = true;
    break;
  }

  /* There is a possibility that out_data will contain all zeros; if this
   * happens, it means that no output covered the requested coordinates in the
   * compositor's global space. That's a valid thing to tell a caller, so just
   * return it anyway. */
  return out_data;
}

static struct coord screen_local_coord_to_abs_coord(uint32_t x, uint32_t y,
  int32_t output_idx) {
  struct coord out_val = {
    .x = -1,
    .y = -1,
  };

  if (!state.layers[output_idx]
    || x >= state.output_geometries[output_idx]->width
    || y >= state.output_geometries[output_idx]->height) {
    return out_val;
  }
  out_val.x = state.output_geometries[output_idx]->x + x;
  out_val.y = state.output_geometries[output_idx]->y + y;
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
  double steep;
  if (slope < 0)
    steep = -slope;
  else
    steep = slope;

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

static void draw_block(uint32_t *pixbuf, int32_t x, int32_t y,
  int32_t layer_width, int32_t layer_height, int32_t rad, bool crosshair) {
  int32_t start_x = x - rad;
  if (start_x < 0) start_x = 0;
  int32_t start_y = y - rad;
  if (start_y < 0) start_y = 0;
  int32_t end_x = x + rad;
  if (end_x >= layer_width) end_x = layer_width - 1;
  int32_t end_y = y + rad;
  if (end_y >= layer_height) end_y = layer_height - 1;

  for (int32_t work_y = start_y; work_y <= end_y; ++work_y) {
    for (int32_t work_x = start_x; work_x <= end_x; ++work_x) {
      if (crosshair && work_x == x) {
        pixbuf[work_y * layer_width + work_x] = 0xffff0000;
      } else if (crosshair && work_y == y) {
        pixbuf[work_y * layer_width + work_x] = 0xffff0000;
      } else {
        pixbuf[work_y * layer_width + work_x] = 0x00000000;
      }
    }
  }
}

static int32_t parse_uintarg(const char *arg_name, const char *val) {
  char *val_endchar;
  uint64_t val_int;

  val_int = strtoul(val, &val_endchar, 10);
  if (*val_endchar != '\0') {
    goto parse_uintarg_error;
  }
  if (val_int > UINT32_MAX) {
    goto parse_uintarg_error;
  }
  return val_int;

parse_uintarg_error:
  fprintf(stderr,
    "FATAL ERROR: Invalid value '%s' passed to parameter '%s'!", val, arg_name);
  exit(1);
  return -1;
}

static void sleep_ms(long ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  int nanosleep_ret;
  do {
    nanosleep_ret = nanosleep(&ts, &ts);
  } while (nanosleep_ret == -1 && errno == EINTR);

  if (nanosleep_ret == -1) {
    fprintf(stderr,
      "FATAL ERROR: Could not sleep for requested start duration!\n");
    exit(1);
  }
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
    for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
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
          state->pending_output_geometries[i] = calloc(1,
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
    for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
      if (state->outputs[i]) {
        if (!state->xdg_outputs[i]) {
          /* This is where we make xdg_outputs for any wl_outputs that were
           * sent too early. */
          state->xdg_outputs[i] = zxdg_output_manager_v1_get_xdg_output(
            state->xdg_output_manager, state->outputs[i]);
          zxdg_output_v1_add_listener(state->xdg_outputs[i],
            &xdg_output_listener, state);
          wl_output_add_listener(state->outputs[i], &output_listener, state);
          state->pending_output_geometries[i] = calloc(1,
            sizeof(struct output_geometry));
        }
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
  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
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
          munmap(layer->pixbuf, layer->size);
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
      close(fd);
      return;
    } else {
      munmap(state->old_kb_map_shm, state->old_kb_map_shm_size);
    }
  }
  zwp_virtual_keyboard_v1_keymap(state->virt_kb, format, fd, size);
  state->old_kb_map_shm = kb_map_shm;
  state->old_kb_map_shm_size = size;
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
  close(fd);
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
  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
    if (state.layers[i]) {
      if (state.layers[i]->buffer == buffer) {
        state.layers[i]->frame_released = true;
        state.layers[i]->buffer = NULL;
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
  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
    if (state->outputs[i] == output) {
      struct output_geometry *geometry = state->pending_output_geometries[i];
      if (geometry->x == 0 && geometry->y == 0 && geometry->width == 0
        && geometry->height == 0)
        return;
      state->output_geometries[i] = state->pending_output_geometries[i];
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
  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
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
  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
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
  struct drawable_layer *layer;
  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
    if (state->layers[i]) {
      if (state->layers[i]->layer_surface == layer_surface) {
        layer = state->layers[i];
        break;
      }
    }
  }
  layer->width = width;
  layer->height = height;
  layer->stride = width * 4;
  layer->size = layer->stride * (size_t) height;
  int shm_fd = create_shm_file(layer->size);
  if (shm_fd == -1) {
    fprintf(stderr,
      "FATAL ERROR: Cannot allocate shared memory block for frame: %s\n",
      strerror(errno));
    exit(1);
  }
  layer->pixbuf = mmap(NULL, layer->size, PROT_READ | PROT_WRITE,
    MAP_SHARED, shm_fd, 0);
  if (layer->pixbuf == MAP_FAILED) {
    close(shm_fd);
    fprintf(stderr,
      "FATAL ERROR: Failed to map shared memory block for frame: %s\n",
      strerror(errno));
    exit(1);
  }
  layer->shm_pool = wl_shm_create_pool(state->shm, shm_fd,
    layer->size);

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
  int fd = open(path, flags);
  int one = 1;
  if (ioctl(fd, EVIOCGRAB, &one) < 0) {
    fprintf(stderr, "FATAL ERROR: Could not grab evdev device '%s'!\n", path);
    exit(1);
  }
  return fd < 0 ? -errno : fd;
}

static void li_close_restricted(int fd, void *user_data) {
  close(fd);
}

/************************/
/* high-level functions */
/************************/

static void draw_frame(struct drawable_layer *layer) {
  if (!layer->frame_released)
    return;
  if (!layer->layer_surface_configured)
    return;
  layer->frame_pending = false;

  struct wl_buffer *buffer = wl_shm_pool_create_buffer(layer->shm_pool, 0,
    layer->width, layer->height, layer->stride, WL_SHM_FORMAT_ARGB8888);

  struct screen_local_coord scr_coord = abs_coord_to_screen_local_coord(
    (int32_t) cursor_x, (int32_t) cursor_y);
  struct screen_local_coord prev_scr_coord = abs_coord_to_screen_local_coord(
    (int32_t) prev_cursor_x, (int32_t) prev_cursor_y);

  bool cursor_is_on_layer = false;
  for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
    if (state.layers[i] == layer) {
      if (i == scr_coord.output_idx) {
        cursor_is_on_layer = true;
      }
    }
  }

  if (layer->last_drawn_cursor_x >= 0 && layer->last_drawn_cursor_y >= 0) {
    /* Blank out the previous cursor location */
    draw_block(layer->pixbuf, layer->last_drawn_cursor_x,
      layer->last_drawn_cursor_y, layer->width, layer->height,
      CURSOR_RADIUS, false);
    damage_surface_enh(layer->surface,
      layer->last_drawn_cursor_x - CURSOR_RADIUS,
      layer->last_drawn_cursor_y - CURSOR_RADIUS,
      layer->last_drawn_cursor_x + CURSOR_RADIUS + 1,
      layer->last_drawn_cursor_y + CURSOR_RADIUS + 1);
  }
  if (cursor_is_on_layer) {
    /* Draw red crosshairs at the pointer location */
    draw_block(layer->pixbuf, scr_coord.x, scr_coord.y, layer->width,
      layer->height, CURSOR_RADIUS, true);
    damage_surface_enh(layer->surface, scr_coord.x - CURSOR_RADIUS,
      scr_coord.y - CURSOR_RADIUS, scr_coord.x + CURSOR_RADIUS + 1,
      scr_coord.y + CURSOR_RADIUS + 1);
  }

  wl_buffer_add_listener(buffer, &buffer_listener, NULL);
  layer->buffer = buffer;
  wl_surface_attach(layer->surface, buffer, 0, 0);
  wl_surface_commit(layer->surface);
  if (cursor_is_on_layer) {
    layer->last_drawn_cursor_x = scr_coord.x;
    layer->last_drawn_cursor_y = scr_coord.y;
  } else {
    layer->last_drawn_cursor_x = -1;
    layer->last_drawn_cursor_y = -1;
  }
  layer->frame_released = false;
}

static struct drawable_layer *allocate_drawable_layer(struct disp_state *state,
  struct wl_output *output) {
  struct drawable_layer *layer = calloc(1, sizeof(struct drawable_layer));
  layer->frame_released = true;
  layer->frame_pending = true;
  layer->last_drawn_cursor_x = -1;
  layer->last_drawn_cursor_y = -1;
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
  wl_surface_commit(layer->surface);

  return layer;
}

static void damage_surface_enh(struct wl_surface *surface, int32_t x,
  int32_t y, int32_t width, int32_t height) {
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  wl_surface_damage_buffer(surface, x, y, width, height);
}

static struct input_packet * update_virtual_cursor(uint32_t ts_milliseconds) {
  struct screen_local_coord prev_scr_coord = abs_coord_to_screen_local_coord(
    (int32_t) prev_cursor_x, (int32_t) prev_cursor_y);

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
        prev_scr_coord = abs_coord_to_screen_local_coord(
          (int32_t) prev_cursor_x, (int32_t) prev_cursor_y);
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
   * This sounds like an awful lot of work, but I couldn't find another way to
   * get the mouse to glide smoothly along edges while still respecting them.
   */
  struct coord start = {
    .x = (int32_t) prev_cursor_x,
    .y = (int32_t) prev_cursor_y,
  };
  struct coord end = {
    .x = (int32_t) cursor_x,
    .y = (int32_t) cursor_y,
  };
  struct coord prev_trav_coord = start;
  bool end_x_hit = false;
  bool end_y_hit = false;
  for (int32_t i = 0; ; ++i) {
    struct coord trav_coord = traverse_line(start, end, i);
    if (trav_coord.x == end.x) end_x_hit = true;
    if (trav_coord.y == end.y) end_y_hit = true;
    struct screen_local_coord trav_scr_coord
      = abs_coord_to_screen_local_coord(trav_coord.x, trav_coord.y);
    if (!trav_scr_coord.valid) {
      /* Figure out what direction we moved when we went off screen, and move
       * move backwards in that direction, but in only one dimension. */
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
    }
    if (end_x_hit && end_y_hit) {
      if ((int32_t) cursor_x != end.x) {
        cursor_x = end.x;
      }
      if ((int32_t) cursor_y != end.y) {
        cursor_y = end.y;
      }
      break;
    }
    prev_trav_coord = trav_coord;
  }
  struct screen_local_coord scr_coord = abs_coord_to_screen_local_coord(
    (int32_t) cursor_x, (int32_t) cursor_y);

  state.layers[prev_scr_coord.output_idx]->frame_pending = true;
  state.layers[scr_coord.output_idx]->frame_pending = true;

  struct input_packet *old_ev_packet;
  /* = rather than == is intentional here */
  if ((old_ev_packet = TAILQ_LAST(&head, tailhead))
    && (!old_ev_packet->is_libinput)) {
    old_ev_packet->cursor_x = (uint32_t) cursor_x;
    old_ev_packet->cursor_y = (uint32_t) cursor_y;
    return NULL;
  } else {
    struct input_packet *ev_packet = calloc(1, sizeof(struct input_packet));
    if (ev_packet == NULL) {
      fprintf(stderr,
        "FATAL ERROR: Could not allocate memory for libinput event packet!\n");
      exit(1);
    }
    ev_packet->is_libinput = false;
    ev_packet->cursor_x = (uint32_t) cursor_x;
    ev_packet->cursor_y = (uint32_t) cursor_y;
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
        zwlr_virtual_pointer_v1_axis_stop(
          state.virt_pointer, ts_milliseconds,
          WL_POINTER_AXIS_VERTICAL_SCROLL);
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
      }
    }

  } else if (ev_type == LIBINPUT_EVENT_KEYBOARD_KEY) {
    if (state.virt_kb_keymap_set) {
      struct libinput_event_keyboard *kb_event
        = libinput_event_get_keyboard_event(li_event);
      uint32_t key = libinput_event_keyboard_get_key(kb_event);
      enum libinput_key_state key_state
        = libinput_event_keyboard_get_key_state(kb_event);
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
      if (key_state == LIBINPUT_KEY_STATE_PRESSED) {
        /* XKB keycodes == evdev keycodes + 8. Why this design decision was
         * made, I have no idea. */
        xkb_state_update_key(state.xkb_state, key + 8, XKB_KEY_DOWN);
      } else {
        xkb_state_update_key(state.xkb_state, key + 8, XKB_KEY_UP);
      }
    }
  }

  if (mouse_event_handled) {
    zwlr_virtual_pointer_v1_frame(state.virt_pointer);
  }
  libinput_event_destroy(li_event);
}

static void queue_libinput_event_and_relocate_virtual_cursor(
  enum libinput_event_type li_event_type, struct libinput_event *li_event) {
  int64_t current_time = current_time_ms();
  int64_t lower_bound = min(max(prev_release_time - current_time, 0),
    max_delay);
  int64_t random_delay = random_between(lower_bound, max_delay);
  struct input_packet *ev_packet;

  if (li_event_type == LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE) {
    struct libinput_event_pointer *pointer_event
      = libinput_event_get_pointer_event(li_event);
    double abs_x = libinput_event_pointer_get_absolute_x_transformed(
      pointer_event, state.global_space_width);
    double abs_y = libinput_event_pointer_get_absolute_y_transformed(
      pointer_event, state.global_space_height);
    prev_cursor_x = cursor_x;
    prev_cursor_y = cursor_y;
    cursor_x = abs_x;
    cursor_y = abs_y;
    ev_packet = update_virtual_cursor((uint32_t) current_time);
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
    ev_packet = update_virtual_cursor((uint32_t) current_time);
    libinput_event_destroy(li_event);
    if (!ev_packet) {
      return;
    }

  } else {
    ev_packet = calloc(1, sizeof(struct input_packet));
    if (ev_packet == NULL) {
      fprintf(stderr,
        "FATAL ERROR: Could not allocate memory for libinput event packet!\n");
      exit(1);
    }
    ev_packet->is_libinput = true;
    ev_packet->li_event = li_event;
    ev_packet->li_event_type = li_event_type;
  }

  ev_packet->sched_time = current_time + random_delay;
  TAILQ_INSERT_TAIL(&head, ev_packet, entries);
  prev_release_time = ev_packet->sched_time;
}

static void release_scheduled_input_events(void) {
  int64_t current_time = current_time_ms();
  struct input_packet *packet;
  while ((packet = TAILQ_FIRST(&head))
    && (current_time >= packet->sched_time)) {
    if (packet->is_libinput) {
      handle_libinput_event(packet->li_event_type, packet->li_event,
        (uint32_t) packet->sched_time);
    } else {
      zwlr_virtual_pointer_v1_motion_absolute(
        state.virt_pointer, (uint32_t) packet->sched_time,
        (uint32_t) packet->cursor_x - state.pointer_space_x,
        (uint32_t) packet->cursor_y - state.pointer_space_y,
        state.global_space_width - state.pointer_space_x,
        state.global_space_height - state.pointer_space_y);
      zwlr_virtual_pointer_v1_frame(state.virt_pointer);
    }
    TAILQ_REMOVE(&head, packet, entries);
    free(packet);
  }
}

static void print_usage(void) {
  fprintf(stderr,
    "Usage: kloak [options]\n");
  fprintf(stderr,
    "Anonymizes keyboard and mouse input patterns by injecting jitter into input\n");
  fprintf(stderr,
    "events. Designed specifically for wlroots-based Wayland compositors. Will NOT\n");
  fprintf(stderr,
    "work with X11.\n");
  fprintf(stderr, "\n");
  fprintf(stderr,
    "Options:\n");
  fprintf(stderr,
    "  -d, --delay=milliseconds          maximum delay of released events.\n");
  fprintf(stderr,
    "                                    Default 100.\n");
  fprintf(stderr,
    "  -s, --start-delay=milliseconds    time to wait before startup. Default 500.\n");
  fprintf(stderr,
    "  -h, --help                        print help\n");
}

/****************************/
/* initialization functions */
/****************************/

static void applayer_random_init(void) {
  randfd = open("/dev/urandom", O_RDONLY);
  if (randfd < 0) {
    fprintf(stderr, "FATAL ERROR: Could not open /dev/urandom: %s\n",
      strerror(errno));
    exit(1);
  }
}

static void applayer_wayland_init(void) {
  /* Technically we also initialize xkbcommon in here but it's only involved
   * because it turned out to be important for sending key events to
   * Wayland. */
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

  /* At this point, the shm, compositor, and wm_base objects will be
   * allocated by registry global handler. */

  state.virt_kb = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
    state.virt_kb_manager, state.seat);
  /* The virtual-keyboard-v1 protocol returns 0 when making a new virtual
   * keyboard if kloak is unauthorized to create a virtual keyboard. However,
   * the protocol treats this as an enum value, meaning... we have to compare
   * a pointer to an enum. This is horrible and the protocol really shouldn't
   * require this, but it does, so... */
  if ((uint64_t)state.virt_kb
    == ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_ERROR_UNAUTHORIZED) {
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
}

static void applayer_libinput_init(void) {
  udev_ctx = udev_new();
  li = libinput_udev_create_context(&li_interface, NULL, udev_ctx);
  /* TODO: Allow customizing the seat with a command line arg */
  libinput_udev_assign_seat(li, "seat0");
  TAILQ_INIT(&head);
}

static void applayer_poll_init(void) {
  ev_fds = calloc(2, sizeof(struct pollfd));
  ev_fds[0].fd = state.display_fd;
  ev_fds[0].events = POLLIN;
  ev_fds[1].fd = libinput_get_fd(li);
  ev_fds[1].events = POLLIN;
}

static void parse_cli_args(int argc, char **argv) {
  const char *optstring = "d:s:h";
  static struct option optarr[] = {
    {"delay", required_argument, NULL, 'd'},
    {"start-delay", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}
  };
  int getopt_rslt;

  while(1) {
    getopt_rslt = getopt_long(argc, argv, optstring, optarr, NULL);
    if (getopt_rslt == -1) {
      break;
    } else if (getopt_rslt == '?') {
      exit(1);
    } else if (getopt_rslt == 'd') {
      max_delay = parse_uintarg("delay", optarg);
    } else if (getopt_rslt == 's') {
      startup_delay = parse_uintarg("start-delay", optarg);
    } else if (getopt_rslt == 'h') {
      print_usage();
      exit(0);
    } else {
      print_usage();
      exit(1);
    }
  }
}

/**********/
/**********/
/** MAIN **/
/**********/
/**********/

int main(int argc, char **argv) {
  if (getuid() != 0) {
    fprintf(stderr, "FATAL ERROR: Must be run as root!\n");
    exit(1);
  }
  parse_cli_args(argc, argv);
  sleep_ms(startup_delay);

  applayer_random_init();
  applayer_wayland_init();
  applayer_libinput_init();
  applayer_poll_init();

  for (;;) {
    while (wl_display_prepare_read(state.display) != 0)
      wl_display_dispatch_pending(state.display);
    wl_display_flush(state.display);

    for (;;) {
      enum libinput_event_type next_ev_type = libinput_next_event_type(li);
      if (next_ev_type == LIBINPUT_EVENT_NONE)
        break;
      struct libinput_event *li_event = libinput_get_event(li);
      queue_libinput_event_and_relocate_virtual_cursor(next_ev_type,
        li_event);
    }

    release_scheduled_input_events();

    for (size_t i = 0; i < MAX_DRAWABLE_LAYERS; ++i) {
      if (!state.layers[i])
        continue;
      if (state.layers[i]->frame_pending)
        draw_frame(state.layers[i]);
    }
    wl_display_flush(state.display);

    poll(ev_fds, 2, POLL_TIMEOUT_MS);

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
  }

  wl_display_disconnect(state.display);
  return 0;
}
