/* c64-kitty.c
 * C64 running in a terminal using Kitty Graphics Protocol.  */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
// Bitmap font for rendering text.
#include "font.h"
#include "microui.h"

/* Global configuration (mostly from command line options). */
struct {
  int ghostty_mode; // Use non standard Kitty protocol that works with
                    // Ghostty, and allows animation, but is incompatible
                    // with Kitty (default).
  int kitty_mode;   // Use graphics protocol with animation codes, this
                    // is needed for the Kitty terminal.
  float zoom;       // display zoom level.
  int width_chars;  // display width in characters.
  int height_chars; // display height in characters.
  int width;        // display width in pixels.
  int height;       // display height in pixels.
} Config;

#define MIN_ZOOM 0.25           // Minimum zoom level.
#define MAX_ZOOM 10             // Maximum zoom level.
#define DEFAULT_WIDTH_CHARS 32  // Width in characters.
#define DEFAULT_HEIGHT_CHARS 16 // Height in characters.
#define DEFAULT_ZOOM 2          // Zoom level.

#define SCREEN_W 400
#define SCREEN_H 300
#define FONT_SIZE 8

// Function to encode data to base64
size_t base64_encode(const unsigned char *data, size_t input_length,
                     char *encoded_data) {
  const char base64_table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i, j;

  for (i = 0, j = 0; i < input_length; i += 3) {
    uint32_t octet_a = data[i];
    uint32_t octet_b = (i + 1 < input_length) ? data[i + 1] : 0;
    uint32_t octet_c = (i + 2 < input_length) ? data[i + 2] : 0;
    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
    encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
    encoded_data[j++] =
        (i + 1 < input_length) ? base64_table[(triple >> 6) & 0x3F] : '=';
    encoded_data[j++] =
        (i + 2 < input_length) ? base64_table[triple & 0x3F] : '=';
  }
  return j;
}

// Terminal keyboard input handling
struct termios orig_termios;

void disable_raw_mode() {
  /* Disable mouse reporting */
  printf("\033[?1006l\033[?1003l");
  fflush(stdout);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
// In enable_raw_mode(), add these lines before tcsetattr
void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  /* Enable mouse reporting. 1003h = Any Event, 1006h = SGR-Pixel reporting */
  printf("\033[?1003h\033[?1006h");
  fflush(stdout);
}

int kbhit() {
  int bytesWaiting;
  ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
  return bytesWaiting;
}

// Initialize Kitty graphics protocol
uint8_t *kitty_init(int width, int height, long *kitty_id) {
  // Initialize random seed for image ID
  srand(time(NULL));
  *kitty_id = rand();

  // Allocate framebuffer memory
  uint8_t *fb = malloc(width * height * 3);
  memset(fb, 0, width * height * 3);
  return fb;
}

// Update display using Kitty graphics protocol
void kitty_update_display(long kitty_id, int frame_number, uint8_t *fb) {
  // Calculate base64 encoded size
  size_t bitmap_size = Config.width * Config.height * 3;
  size_t encoded_size = 4 * ((bitmap_size + 2) / 3);
  char *encoded_data = (char *)malloc(encoded_size + 1);

  if (!encoded_data) {
    fprintf(stderr, "Memory allocation failed\n");
    return;
  }

  // Encode the bitmap data to base64
  base64_encode(fb, bitmap_size, encoded_data);
  encoded_data[encoded_size] = '\0'; // Null-terminate the string

  // Send Kitty Graphics Protocol escape sequence with base64 data.
  // Kitty allows a maximum chunk of 4096 bytes each.
  size_t encoded_offset = 0;
  size_t chunk_size = 4096;
  while (encoded_offset < encoded_size) {
    int more_chunks = (encoded_offset + chunk_size) < encoded_size;
    if (encoded_offset == 0) {
      if (Config.ghostty_mode) {
        printf("\033_Ga=%c,i=%lu,f=24,s=%d,v=%d,q=2,c=%d,r=%d,m=%d;",
               frame_number == 0 ? 'T' : 't', kitty_id, Config.width,
               Config.height, Config.width_chars, Config.height_chars,
               more_chunks);
      } else {
        if (frame_number == 0) {
          printf("\033_Ga=T,i=%lu,f=24,s=%d,v=%d,q=2,"
                 "c=%d,r=%d,m=%d;",
                 kitty_id, Config.width, Config.height, Config.width_chars,
                 Config.height_chars, more_chunks);
        } else {
          printf("\033_Ga=f,r=1,i=%lu,f=24,x=0,y=0,s=%d,v=%d,m=%d;", kitty_id,
                 Config.width, Config.height, more_chunks);
        }
      }
    } else {
      if (Config.ghostty_mode) {
        printf("\033_Gm=%d;", more_chunks);
      } else {
        // Chunks after the first just require the raw data and the
        // more flag.
        if (frame_number == 0) {
          printf("\033_Gm=%d;", more_chunks);
        } else {
          printf("\033_Ga=f,r=1,m=%d;", more_chunks);
        }
      }
    }

    // Transfer payload.
    size_t this_size = more_chunks ? 4096 : encoded_size - encoded_offset;
    fwrite(encoded_data + encoded_offset, this_size, 1, stdout);
    printf("\033\\");
    fflush(stdout);
    encoded_offset += this_size;
  }

  if (Config.kitty_mode && frame_number > 0) {
    // In Kitty mode we need to emit the "a" action to update
    // our area with the new frame.
    printf("\033_Ga=a,c=1,i=%lu;", kitty_id);
    printf("\033\\");
  }

  /* When the image is created, add a newline so that the cursor
   * is more naturally placed under the image, not at the right/bottom
   * corner. */
  if (frame_number == 0) {
    printf("\r\n");
    fflush(stdout);
  }

  // Clean up
  free(encoded_data);
}

struct winsize get_terminal_size() {
  struct winsize w;
  ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
  return w;
}

int process_input(mu_Context *ctx) {
  int bytes_waiting = kbhit();
  if (!bytes_waiting)
    return 0;

  // Reading up to 64 bytes is safe for most escape sequences.
  char buf[64];
  int nread = read(STDIN_FILENO, buf, sizeof(buf) - 1);
  if (nread <= 0)
    return 0;
  buf[nread] = '\0';

  // Simple key press handling
  if (nread == 1) {
    if (buf[0] == 27) { // Escape key
      return 1;         // Request quit
    }
    // mu_input_keydown(ctx, buf[0]);
    mu_input_text(ctx, buf);
    return 0;
  }

  // Mouse event parsing (SGR format: \033[<BTN;X;Ym or \033[<BTN;X;YM)
  if (nread > 5 && strncmp(buf, "\033[<", 3) == 0) {
    int button, x, y;
    char event_type;

    if (sscanf(buf, "\033[<%d;%d;%d%c", &button, &x, &y, &event_type) == 4) {
      // Terminal coordinates are 1-based, convert to 0-based
      // We also convert from character cell coordinates to pixel coordinates.
      struct winsize ts = get_terminal_size();
      int offsety = ts.ws_row - Config.height_chars;

      int pixel_x = (x - 1) * SCREEN_W / Config.width_chars;
      int pixel_y = (y - offsety) * SCREEN_H / Config.height_chars;

      // Handle motion event
      if ((button & 32)) {
        printf("\rMouse moved to %d,%d [%d,%d]", pixel_x, pixel_y, x, y);
        mu_input_mousemove(ctx, pixel_x, pixel_y);
      }

      // Handle scroll wheel
      if ((button & 64)) {
        if (button == 64) { // wheel up
          mu_input_scroll(ctx, 0, -1);
        } else if (button == 65) { // wheel down
          mu_input_scroll(ctx, 0, 1);
        }
      } else { // Handle button press/release
        int mu_button = 0;
        switch (button & 3) {
        case 0:
          mu_button = MU_MOUSE_LEFT;
          break;
        case 1:
          mu_button = MU_MOUSE_MIDDLE;
          break;
        case 2:
          mu_button = MU_MOUSE_RIGHT;
          break;
        default:
          return 0; // Unknown button
        }

        if (event_type == 'M') { // Press
          mu_input_mousedown(ctx, pixel_x, pixel_y, mu_button);
        } else if (event_type == 'm') { // Release
          mu_input_mouseup(ctx, pixel_x, pixel_y, mu_button);
        }
      }
    }
    return 0;
  }

  printf("\rUnrecognized input: %.*s", nread, buf);
  return 0;
}

void crt_set_pixel(void *fbptr, int x, int y, uint32_t color) {
  uint8_t *fb = fbptr;

  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H)
    return;

  uint8_t *dst = fb + (x * 3 + y * SCREEN_W * 3);
  dst[0] = (color >> 16) & 0xff; // R
  dst[1] = (color >> 8) & 0xff;  // G
  dst[2] = color & 0xff;         // B
}

/* Initialize and parse the configuration, storing it into the
 * global Config structure. */
void parse_config(int argc, char **argv) {
  Config.ghostty_mode = 1;
  Config.kitty_mode = 0;
  Config.zoom = DEFAULT_ZOOM;
  Config.width = SCREEN_W;
  Config.height = SCREEN_H;

  for (int j = 1; j < argc; j++) {
    int leftargs = argc - j - 1;
    if (!strcasecmp(argv[j], "--kitty")) {
      Config.kitty_mode = 1;
      Config.ghostty_mode = 0;
    } else if (!strcasecmp(argv[j], "--ghostty")) {
      Config.kitty_mode = 0;
      Config.ghostty_mode = 1;
    } else if (!strcasecmp(argv[j], "--zoom") && leftargs) {
      j++;
      Config.zoom = strtod(argv[j], NULL);
      if (Config.zoom < MIN_ZOOM) {
        Config.zoom = MIN_ZOOM;
      } else if (Config.zoom > MAX_ZOOM) {
        Config.zoom = MAX_ZOOM;
      }
    } else {
      if (argv[j][0] != '-' && Config.prg_filename == NULL) {
        Config.prg_filename = strdup(argv[j]);
      } else {
        fprintf(stderr, "Unrecognized option: %s\n", argv[j]);
        exit(1);
      }
    }
  }

  // Handle configurations that require to be computed.
  Config.width_chars = DEFAULT_WIDTH_CHARS * Config.zoom;
  Config.height_chars = DEFAULT_HEIGHT_CHARS * Config.zoom;
}

void draw_rectangle(void *fbptr, int x, int y, int w, int h, uint32_t color) {
  uint8_t *fb = fbptr;

  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      crt_set_pixel(fb, x + j, y + i, color);
    }
  }
}

/* Draw a single character at position (x, y) with given size and color */
void draw_char(void *fbptr, int x, int y, char c, uint32_t size,
               uint32_t color) {
  uint8_t ch = (uint8_t)c;

  /* Clamp character to valid range */
  if (ch > 126)
    ch = 32; /* Replace invalid chars with space */

  const uint8_t *glyph = font_8x8[ch];

  for (int row = 0; row < 8; row++) {
    uint8_t line = glyph[row];
    for (int col = 0; col < 8; col++) {
      if (line & (0x80 >> col)) {
        /* Draw a size x size square for each font pixel */
        for (uint32_t dy = 0; dy < size; dy++) {
          for (uint32_t dx = 0; dx < size; dx++) {
            crt_set_pixel(fbptr, x + col * size + dx, y + row * size + dy,
                          color);
          }
        }
      }
    }
  }
}

/* Draw text string at position (x, y) with given size and color */
void draw_text(void *fbptr, int x, int y, char *str, uint32_t size,
               uint32_t color) {
  if (!str)
    return;

  int current_x = x;
  int current_y = y;
  int char_width = 8 * size;
  int char_height = 8 * size;

  while (*str) {
    if (*str == '\n') {
      /* Handle newline */
      current_x = x;
      current_y += char_height;
    } else if (*str == '\r') {
      /* Handle carriage return */
      current_x = x;
    } else {
      /* Draw the character */
      draw_char(fbptr, current_x, current_y, *str, size, color);
      current_x += char_width;

      /* Wrap to next line if we exceed screen width */
      if (current_x + char_width > SCREEN_W) {
        current_x = x;
        current_y += char_height;
      }
    }
    str++;

    /* Stop if we exceed screen height */
    if (current_y + char_height > SCREEN_H) {
      break;
    }
  }
}

static int text_width(mu_Font font, const char *text, int len) {
  (void)font;
  if (len == -1) {
    len = strlen(text);
  }
  return FONT_SIZE * len;
}

static int text_height(mu_Font font) {
  (void)font;
  return FONT_SIZE;
}

uint32_t toColor(mu_Color color) {
  return (color.r << 16) | (color.g << 8) | color.b;
}

void test_window(mu_Context *ctx) {
  if (mu_begin_window(ctx, "Log Window", mu_rect(0, 0, SCREEN_W, SCREEN_H))) {
    /* output text panel */
    mu_layout_row(ctx, 1, (int[]){-1}, -25);
    mu_begin_panel(ctx, "Log Output");
    mu_layout_row(ctx, 1, (int[]){-1}, -1);
    mu_text(ctx, "Hello");
    mu_end_panel(ctx);

    /* input textbox + submit button */
    static char buf[128];
    int submitted = 0;
    mu_layout_row(ctx, 2, (int[]){-70, -1}, 0);
    if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
      mu_set_focus(ctx, ctx->last_id);
      submitted = 1;
    }
    if (mu_button(ctx, "Submit")) {
      submitted = 1;
    }
    if (submitted) {
      printf("\rSubmitted: %s", buf);
      buf[0] = '\0';
    }

    mu_end_window(ctx);
  }
}

int main(int argc, char **argv) {
  mu_Context ctx;
  mu_init(&ctx);
  ctx.text_width = text_width;
  ctx.text_height = text_height;

  parse_config(argc, argv);

  /* Initialize Kitty graphics */
  long kitty_id;
  uint8_t *fb = kitty_init(Config.width, Config.height, &kitty_id);

  // Enable raw mode for keyboard input
  enable_raw_mode();

  // run the emulation/input/render loop
  int frame = 0;
  int quit_requested = 0;

  while (!quit_requested) {
    // Handle input
    quit_requested = process_input(&ctx);

    // ui
    mu_begin(&ctx);
    test_window(&ctx);
    mu_end(&ctx);

    // bind renderer
    mu_Command *cmd = NULL;
    while (mu_next_command(&ctx, &cmd)) {
      switch (cmd->type) {
      case MU_COMMAND_TEXT:
        draw_text(fb, cmd->text.pos.x, cmd->text.pos.y, cmd->text.str, 1,
                  toColor(cmd->text.color));
        break;
      case MU_COMMAND_RECT:
        draw_rectangle(fb, cmd->rect.rect.x, cmd->rect.rect.y, cmd->rect.rect.w,
                       cmd->rect.rect.h, toColor(cmd->rect.color));
        break;
      case MU_COMMAND_ICON:
        // TODO: handle icons
        break;
      case MU_COMMAND_CLIP:
        // TODO: handle clipping
        break;
      }
    }

    // Update display using Kitty protocol
    kitty_update_display(kitty_id, frame++, fb);
  }

  // Cleanup
  free(fb);
  disable_raw_mode();
  printf("\nEmulator terminated.\n");

  return 0;
}