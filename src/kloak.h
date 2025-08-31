/*
 * Copyright (c) 2025 - 2025 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
 * See the file COPYING for copying conditions.
 */

#include <stdint.h>
#include <wayland-client.h>
#include <libinput.h>

#define MAX_DRAWABLE_LAYERS 128
#define CURSOR_RADIUS 15
#define POLL_TIMEOUT_MS 1
#define DEFAULT_MAX_DELAY_MS 100
#define DEFAULT_STARTUP_TIMEOUT_MS 500

#ifndef min
#define min(a, b) ( ((a) < (b)) ? (a) : (b) )
#endif

#ifndef max
#define max(a, b) ( ((a) > (b)) ? (a) : (b) )
#endif

/*******************/
/* core structures */
/*******************/

/*
 * Defines a screen-local layer that can be drawn on. Each screen has one
 * drawable layer, the virtual cursor is drawn on this. The layer will be
 * created using layer_shell and will appear on top of all other surfaces if
 * at all possible.
 */
struct drawable_layer {
  struct wl_output *output;
  struct wl_buffer *buffer;
  size_t width;
  size_t height;
  size_t stride;
  size_t size;
  uint32_t *pixbuf;
  struct wl_surface *surface;
  struct wl_shm_pool *shm_pool;
  /* Layer shell stuff */
  struct zwlr_layer_surface_v1 *layer_surface;
  bool layer_surface_configured;
  /* Sync state */
  bool frame_released;
  bool frame_pending;
  int32_t last_drawn_cursor_x;
  int32_t last_drawn_cursor_y;
};

/*
 * Defines the location and size of a display in compositor-global space.
 */
struct output_geometry {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

/*
 * Defines a point in screen-local space, along with which screen the point is
 * located on.
 */
struct screen_local_coord {
  int32_t x;
  int32_t y;
  int32_t output_idx;
  bool valid;
};

/*
 * Defines a point in no particular space.
 */
struct coord {
  int32_t x;
  int32_t y;
};

/*
 * Defines a buffered input event. Two types of events are supported, mouse
 * movement events and libinput events. libinput events can be any arbitrary
 * event supported by libinput. Mouse movement events are defined as a cursor
 * position in compositor global space. Both kinds of events have a scheduled
 * release time. An `entries` field is included to allow buffered events to be
 * stored in a tail queue.
 */
struct input_packet {
  bool is_libinput;

  /* libinput-specific bits */
  struct libinput_event * li_event;
  enum libinput_event_type li_event_type;

  /* mouse movement bits */
  uint32_t cursor_x;
  uint32_t cursor_y;

  /* generic bits */
  int64_t sched_time;
  TAILQ_ENTRY(input_packet) entries;
};

/*
 * Monolithic Wayland state object.
 */
struct disp_state {
  struct wl_display *display;
  int display_fd;
  struct wl_registry *registry;
  struct wl_shm *shm;
  struct wl_compositor *compositor;
  struct wl_seat *seat;
  const char *seat_name;
  uint32_t seat_caps;
  bool seat_set;
  struct wl_keyboard *kb;
  struct wl_output *outputs[MAX_DRAWABLE_LAYERS];
  int output_names[MAX_DRAWABLE_LAYERS];
  struct zxdg_output_manager_v1 *xdg_output_manager;
  struct zxdg_output_v1 *xdg_outputs[MAX_DRAWABLE_LAYERS];
  struct output_geometry *output_geometries[MAX_DRAWABLE_LAYERS];
  struct output_geometry *pending_output_geometries[MAX_DRAWABLE_LAYERS];
  uint32_t global_space_width;
  uint32_t global_space_height;
  uint32_t pointer_space_x;
  uint32_t pointer_space_y;
  struct zwlr_layer_shell_v1 *layer_shell;
  struct zwlr_virtual_pointer_manager_v1 *virt_pointer_manager;
  struct zwp_virtual_keyboard_manager_v1 *virt_kb_manager;
  struct zwp_virtual_keyboard_v1 *virt_kb;
  struct zwlr_virtual_pointer_v1 *virt_pointer;
  bool virt_kb_keymap_set;
  struct xkb_context *xkb_ctx;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  char *old_kb_map_shm;
  uint32_t old_kb_map_shm_size;
  struct drawable_layer *layers[MAX_DRAWABLE_LAYERS];
};

/***************/
/* core unions */
/***************/

/*
 * A 64-bit unsigned integer with direct access to the constituent bytes. Used
 * to allow getting 64-bit integers from /dev/urandom.
 */
union rand_uint64 {
  uint64_t val;
  char raw[sizeof(uint64_t)];
};

/*********************/
/* utility functions */
/*********************/

/*
 * Reads the specified number of random bytes from /dev/urandom into the
 * specified buffer. applayer_random_init must be called before this function
 * will behave as intended.
 */
static void read_random(char * buf, size_t len);

/*
 * Populates a string with a number of random characters in the set [a-zA-Z].
 */
static void randname(char *buf, size_t len);

/*
 * Creates a mmap-able shared memory file of the specified size.
 */
static int create_shm_file(size_t size);

/*
 * Returns a monotonic 64-bit timestamp.
 */
static int64_t current_time_ms(void);

/*
 * Generates a random 64-bit number between the two specified numbers. Uses
 * /dev/urandom as its source.
 */
static int64_t random_between(int64_t lower, int64_t upper);

/*
 * Determine if a point falls inside an area.
 */
static bool check_point_in_area(uint32_t x, uint32_t y, uint32_t rect_x,
  uint32_t rect_y, uint32_t rect_width, uint32_t rect_height);

/*
 * Determine if two screens are touching or overlapping given their
 * geometries.
 */
static bool check_screen_touch(struct output_geometry scr1,
  struct output_geometry scr2);

/*
 * Calculates the size of the global compositor space and the location of the
 * upper-left corner of the pointer's coordinate space from the geometries of
 * the active displays. The function detects if there are gaps between the
 * displays and aborts the program if so.
 */
static void recalc_global_space(struct disp_state * state);

/*
 * Converts a set of coordinates in global compositor space to a set of
 * coordinates in screen-local space.
 */
static struct screen_local_coord abs_coord_to_screen_local_coord(int32_t x,
  int32_t y);

/*
 * Converts a set of coordinates in screen-local space to a set of coordinates
 * in compositor-global space.
 */
static struct coord screen_local_coord_to_abs_coord(uint32_t x, uint32_t y,
  int32_t output_idx);

/*
 * Takes two points that define a line on a 2d plane, and walks the specified
 * number of pixels from the start point towards the "end" point. Note that
 * you *can* walk past the end point, and that the end point is NOT guaranteed
 * to be one of the values this function outputs.
 */
static struct coord traverse_line(struct coord start, struct coord end,
  int32_t pos);

/*
 * Draws a virtual cursor block on the specified pixel buffer. If crosshair is
 * set to true, crosshairs representing the cursor will be drawn in the block,
 * otherwise the block will simply blank out anything that it is drawing over.
 */
static void draw_block(uint32_t * pixbuf, int32_t x, int32_t y,
  int32_t layer_width, int32_t layer_height, int32_t rad, bool crosshair);

/*
 * Parse an option parameter as an unsigned integer.
 */
static int32_t parse_uintarg(const char *arg_name, const char *val);

/*
 * Sleeps for the specified number of milliseconds.
 */
static void sleep_ms(long ms);

/********************/
/* wayland handling */
/********************/

static void registry_handle_global(void * data, struct wl_registry * registry,
  uint32_t name, const char * interface, uint32_t version);
static void registry_handle_global_remove(void *data,
  struct wl_registry * registry, uint32_t name);
static void seat_handle_name(void *data, struct wl_seat *seat,
  const char *name);
static void seat_handle_capabilities(void *data, struct wl_seat *seat,
  uint32_t capabilities);
static void kb_handle_keymap(void *data, struct wl_keyboard *kb,
  uint32_t format, int32_t fd, uint32_t size);
static void kb_handle_enter(void *data, struct wl_keyboard *kb,
  uint32_t serial, struct wl_surface *surface, struct wl_array *keys);
static void kb_handle_leave(void *data, struct wl_keyboard *kb,
  uint32_t serial, struct wl_surface *surface);
static void kb_handle_key(void *data, struct wl_keyboard *kb, uint32_t serial,
  uint32_t time, uint32_t key, uint32_t state);
static void kb_handle_modifiers(void *data, struct wl_keyboard *kb,
  uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
  uint32_t mods_locked, uint32_t group);
static void kb_handle_repeat_info(void *data, struct wl_keyboard *kb,
  int32_t rate, int32_t delay);
static void wl_buffer_release(void *data, struct wl_buffer * buffer);
static void wl_output_handle_geometry(void *data, struct wl_output *output,
  int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
  int32_t subpixel, const char *make, const char *model, int32_t transform);
static void wl_output_handle_mode(void *data, struct wl_output *output,
  uint32_t flags, int32_t width, int32_t height, int32_t refresh);
static void wl_output_info_done(void *data, struct wl_output *output);
static void wl_output_handle_scale(void *data, struct wl_output *output,
  int32_t factor);
static void wl_output_handle_name(void *data, struct wl_output *output,
  const char *name);
static void wl_output_handle_description(void *data, struct wl_output *output,
  const char *description);
static void xdg_output_handle_logical_position(void *data,
  struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y);
static void xdg_output_handle_logical_size(void *data,
  struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height);
static void xdg_output_info_done(void *data,
  struct zxdg_output_v1 *xdg_output);
static void xdg_output_handle_name(void *data,
  struct zxdg_output_v1 *xdg_output, const char *name);
static void xdg_output_handle_description(void *data,
  struct zxdg_output_v1 *xdg_output, const char *description);
static void layer_surface_configure(void *data,
  struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial, uint32_t width,
  uint32_t height);

/*********************/
/* libinput handling */
/*********************/

/*
 * Opens evdev devices for libinput, grabbing them with EVIOCGRAB so that they
 * can't be used by other applications on the system.
 */
static int li_open_restricted(const char *path, int flags, void * user_data);

/*
 * Closes evdev devices for libinput.
 */
static void li_close_restricted(int fd, void *user_data);

/************************/
/* high-level functions */
/************************/

/*
 * Attempts to update the specified layer to display the virtual cursor at the
 * right location. May do nothing if the compositor has not yet acknowledged a
 * previously sent frame, there is no pending changes that affect the
 * specified layer, or kloak hasn't yes negotiated an appropriate
 * configuration for the layer with the compositor.
 */
static void draw_frame(struct drawable_layer *layer);

/*
 * Allocates a drawable_layer struct for the specified display, and registers
 * it with the compositor.
 */
static struct drawable_layer *allocate_drawable_layer(
  struct disp_state *state, struct wl_output *output);

/*
 * Damages the specified region of the specified surface. Takes the same
 * arguments as wl_surface_damage_buffer, but clamps the x and y parameters to
 * 0 if they're below zero when passed in. This works around undesirable
 * behavior when passing negative x and y values to wl_surface_damage_buffer.
 */
static void damage_surface_enh(struct wl_surface *surface, int32_t x,
  int32_t y, int32_t width, int32_t height);

/*
 * Updates the virtual cursor's position based on the global cursor_x and
 * cursor_y variables. This will create a new mouse movement event if there
 * isn't one already queued, and update a queued mouse movement event to
 * reflect the current virtual cursor position otherwise.
 */
static struct input_packet * update_virtual_cursor(uint32_t ts_milliseconds);

/*
 * Processes a libinput event, sending emulated input to the compositor as
 * appropriate.
 */
static void handle_libinput_event(enum libinput_event_type ev_type,
  struct libinput_event *li_event, uint32_t ts_milliseconds);

/*
 * Schedules a libinput event for future release to the compositor. As a side
 * effect, also redraws the virtual cursor if needed.
 */
static void queue_libinput_event_and_relocate_virtual_cursor(
  enum libinput_event_type li_event_type, struct libinput_event *li_event);

/*
 * Finds all queued input events that are ready to be released, and process
 * them.
 */
static void release_scheduled_input_events(void);

/*
 * Prints usage information.
 */
static void print_usage(void);

/****************************/
/* initialization functions */
/****************************/

/*
 * Opens /dev/urandom so that other parts of the system that need random
 * values can get them.
 */
static void applayer_random_init(void);

/*
 * Connects to the wayland compositor and begins initialization of the Wayland
 * state.
 */
static void applayer_wayland_init(void);

/*
 * Opens all input devices on seat0 with libinput and prepares to process
 * events from them.
 */
static void applayer_libinput_init(void);

/*
 * Initializes the fd poll mechanism.
 */
static void applayer_poll_init(void);

/*
 * Parses command-line arguments.
 */
static void parse_cli_args(int argc, char **argv);

/*********************/
/* wayland callbacks */
/*********************/

static const struct wl_registry_listener registry_listener = {
  .global = registry_handle_global,
  .global_remove = registry_handle_global_remove,
};
static const struct wl_seat_listener seat_listener = {
  .name = seat_handle_name,
  .capabilities = seat_handle_capabilities,
};
static const struct wl_keyboard_listener kb_listener = {
  .keymap = kb_handle_keymap,
  .enter = kb_handle_enter,
  .leave = kb_handle_leave,
  .key = kb_handle_key,
  .modifiers = kb_handle_modifiers,
  .repeat_info = kb_handle_repeat_info,
};
static const struct wl_buffer_listener buffer_listener = {
  .release = wl_buffer_release,
};
static const struct wl_output_listener output_listener = {
  .geometry = wl_output_handle_geometry,
  .mode = wl_output_handle_mode,
  .done = wl_output_info_done,
  .scale = wl_output_handle_scale,
  .name = wl_output_handle_name,
  .description = wl_output_handle_description,
};
static const struct zxdg_output_v1_listener xdg_output_listener = {
  .logical_position = xdg_output_handle_logical_position,
  .logical_size = xdg_output_handle_logical_size,
  .done = xdg_output_info_done,
  .name = xdg_output_handle_name,
  .description = xdg_output_handle_description,
};
static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
  .configure = layer_surface_configure,
};

/**********************/
/* libinput callbacks */
/**********************/

static const struct libinput_interface li_interface = {
  .open_restricted = li_open_restricted,
  .close_restricted = li_close_restricted,
};
