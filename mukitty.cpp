// Node.js plugin that integrates together Raylib and MicroUI
#include <node.h>
#include <print>
extern "C" {
#include "microui.h"
}
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
// Bitmap font for rendering text.
#include "c64_font.h"

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
#define DEFAULT_ZOOM 2       // Zoom level.
#define DEFAULT_SCREEN_W 450 // Default screen width in pixels.
#define DEFAULT_SCREEN_H 300 // Default screen height in pixels.

#define FONT_SIZE 8
#define RESW 10 // Character width in pixels.
#define RESH 16 // Character height in pixels.

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

uint8_t *fb = NULL; // Framebuffer pointer
long kitty_id = 0;  // Kitty graphics ID

// Initialize Kitty graphics protocol
uint8_t *kitty_init(int width, int height) {
  // Initialize random seed for image ID
  srand(time(NULL));
  kitty_id = rand();

  // Allocate framebuffer memory
  fb = (uint8_t *)malloc(width * height * 3);
  memset(fb, 0, width * height * 3);
  return fb;
}

// Update display using Kitty graphics protocol
void kitty_update_display() {
  static int frame_number = 0;
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
    frame_number++;
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

      int pixel_x = (x - 1) * Config.width / Config.width_chars;
      int pixel_y = (y - offsety) * Config.height / Config.height_chars;

      // Handle motion event
      if ((button & 32)) {
        printf("\rMouse moved to %d,%d [%d,%d]", pixel_x, pixel_y, x, y);
        mu_input_mousemove(ctx, pixel_x, pixel_y);
      }

      // Handle scroll wheel
      if ((button & 64)) {
        if (button == 64) { // wheel up
          mu_input_scroll(ctx, 0, -5);
        } else if (button == 65) { // wheel down
          mu_input_scroll(ctx, 0, 5);
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
  uint8_t *fb = (uint8_t *)fbptr;

  if (x < 0 || x >= Config.width || y < 0 || y >= Config.height)
    return;

  uint8_t *dst = fb + (x * 3 + y * Config.width * 3);
  dst[0] = (color >> 16) & 0xff; // R
  dst[1] = (color >> 8) & 0xff;  // G
  dst[2] = color & 0xff;         // B
}

/* Initialize and parse the configuration, storing it into the
 * global Config structure. */
void init_config(int kitty_mode, int zoom, int width, int height) {
  Config.zoom = zoom;
  Config.width = width;
  Config.height = height;
  Config.ghostty_mode = !kitty_mode;
  Config.kitty_mode = kitty_mode;
  Config.width_chars = (width / RESW) * Config.zoom;
  Config.height_chars = (height / RESH) * Config.zoom;
}

uint32_t toColor(mu_Color color) {
  return (color.r << 16) | (color.g << 8) | color.b;
}

void draw_rectangle(void *fbptr, int x, int y, int w, int h, uint32_t color) {
  uint8_t *fb = (uint8_t *)fbptr;

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
      if (current_x + char_width > Config.width) {
        current_x = x;
        current_y += char_height;
      }
    }
    str++;

    /* Stop if we exceed screen height */
    if (current_y + char_height > Config.height) {
      break;
    }
  }
}

void draw_icon(int id, mu_Rect rect, mu_Color color) {
  int x = rect.x + (rect.w - 16) / 2;
  int y = rect.y + (rect.h - 16) / 2;
  draw_char(fb, x, y, id, 2, toColor(color));
}

int text_width(mu_Font font, const char *text, int len) {
  (void)font;
  if (len == -1) {
    len = strlen(text);
  }
  return FONT_SIZE * len;
}

int text_height(mu_Font font) {
  (void)font;
  return FONT_SIZE;
}

static mu_Context ctx = {0};

void muButton(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  bool result = mu_button(
      &ctx, *v8::String::Utf8Value(
                isolate, args[0]->ToString(context).ToLocalChecked()));
  args.GetReturnValue().Set(v8::Boolean::New(isolate, result));
}

void muLabel(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  mu_label(&ctx, *v8::String::Utf8Value(
                     isolate, args[0]->ToString(context).ToLocalChecked()));
}

void muSlider(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  int min = args[0]->Int32Value(context).FromJust();
  int max = args[1]->Int32Value(context).FromJust();
  float value = args[2]->NumberValue(context).FromJust();
  mu_slider(&ctx, &value, min, max);
  args.GetReturnValue().Set(v8::Number::New(isolate, value));
}

void muCheckbox(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  int checked = args[0]->BooleanValue(isolate);
  mu_checkbox(&ctx,
              *v8::String::Utf8Value(
                  isolate, args[1]->ToString(context).ToLocalChecked()),
              &checked);
  args.GetReturnValue().Set(v8::Boolean::New(isolate, checked));
}

void muTextbox(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  char buf[128];
  strcpy(buf, *v8::String::Utf8Value(
                  isolate, args[0]->ToString(context).ToLocalChecked()));
  mu_textbox(&ctx, buf, sizeof(buf));
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(isolate, buf).ToLocalChecked());
}

void muText(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  mu_text(&ctx, *v8::String::Utf8Value(
                    isolate, args[0]->ToString(context).ToLocalChecked()));
}

void muRect(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  int color = args[0]->Int32Value(context).FromJust();
  mu_Rect r = mu_layout_next(&ctx);
  mu_Color mu_color = {(unsigned char)((color >> 16) & 0xFF),
                       (unsigned char)((color >> 8) & 0xFF),
                       (unsigned char)(color & 0xFF), 255};
  mu_draw_rect(&ctx, r, mu_color);
}

void muBeginWindow(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  mu_begin_window_ex(&ctx,
                     *v8::String::Utf8Value(
                         isolate, args[0]->ToString(context).ToLocalChecked()),
                     mu_rect(0, 0, Config.width, Config.height),
                     MU_OPT_NOCLOSE | MU_OPT_NOTITLE);
}

void muLayoutRow(const v8::FunctionCallbackInfo<v8::Value> &args) {
  int items = 0;
  int height = 0;
  int *widths = NULL;
  if (args.Length() > 2) {
    items =
        args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromJust();
    height =
        args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromJust();
    widths = new int[items];
    for (int i = 0; i < items; i++) {
      widths[i] = args[2 + i]
                      ->Int32Value(args.GetIsolate()->GetCurrentContext())
                      .FromJust();
    }
  }

  mu_layout_row(&ctx, items, widths, height);
  delete[] widths; // Clean up allocated memory
}

void muBeginColumn(const v8::FunctionCallbackInfo<v8::Value> &args) {
  (void)args;
  mu_layout_begin_column(&ctx);
}

void muEndColumn(const v8::FunctionCallbackInfo<v8::Value> &args) {
  (void)args;
  mu_layout_end_column(&ctx);
}

void muEndWindow(const v8::FunctionCallbackInfo<v8::Value> &args) {
  mu_end_window(&ctx);
}

void updateInput(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  int exit = process_input(&ctx);
  args.GetReturnValue().Set(v8::Boolean::New(isolate, exit));
}

void clearBackground(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  uint32_t color = 0x000000; // Default black background
  if (args.Length() > 0 && args[0]->IsUint32()) {
    color = args[0]->Uint32Value(isolate->GetCurrentContext()).FromJust();
  }

  draw_rectangle(fb, 0, 0, Config.width, Config.height, color);
}

void muBegin(const v8::FunctionCallbackInfo<v8::Value> &args) {
  (void)args;
  mu_begin(&ctx);
}

void muEnd(const v8::FunctionCallbackInfo<v8::Value> &args) {
  (void)args;
  mu_end(&ctx);

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
      draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
      break;
    case MU_COMMAND_CLIP:
      // TODO: handle clipping
      break;
    }
  }
  kitty_update_display();
}

int getTextWidth(mu_Font font, const char *str, int len) {
  int x = 10;
  return x;
}

int getTextHeight(mu_Font font) { return FONT_SIZE; }

void initWindow(const v8::FunctionCallbackInfo<v8::Value> &args) {
  auto isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();

  int width = 0;
  if (args.Length() > 0)
    width = args[0]->Int32Value(context).FromJust();
  if (width <= 0)
    width = DEFAULT_SCREEN_W;

  int height = 0;
  if (args.Length() > 1)
    height = args[1]->Int32Value(context).FromJust();
  if (height <= 0)
    height = DEFAULT_SCREEN_H;

  int kitty_mode = 0;
  if (args.Length() > 2) {
    kitty_mode =
        strcmp(*v8::String::Utf8Value(
                   isolate, args[2]->ToString(context).ToLocalChecked()),
               "kitty") == 0
            ? 1
            : 0;
  }
  int zoom = DEFAULT_ZOOM;
  if (args.Length() > 3) {
    zoom = args[3]->Int32Value(context).FromJust();
  }

  mu_init(&ctx);
  ctx.text_width = getTextWidth;
  ctx.text_height = getTextHeight;
  init_config(kitty_mode, zoom, width, height);
  kitty_init(Config.width, Config.height);

  // Enable raw mode for keyboard input
  enable_raw_mode();
}

void close(const v8::FunctionCallbackInfo<v8::Value> &args) {
  (void)args;
  // Close the window and clean up resources
  free(fb);
  disable_raw_mode();
  fb = NULL;
  ctx = {0};
  kitty_id = 0; // Reset Kitty ID
}

void Initialize(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "init", initWindow);
  NODE_SET_METHOD(exports, "close", close);
  NODE_SET_METHOD(exports, "updateInput", updateInput);
  NODE_SET_METHOD(exports, "clearBackground", clearBackground);
  NODE_SET_METHOD(exports, "begin", muBegin);
  NODE_SET_METHOD(exports, "end", muEnd);
  NODE_SET_METHOD(exports, "beginWindow", muBeginWindow);
  NODE_SET_METHOD(exports, "endWindow", muEndWindow);
  NODE_SET_METHOD(exports, "button", muButton);
  NODE_SET_METHOD(exports, "label", muLabel);
  NODE_SET_METHOD(exports, "slider", muSlider);
  NODE_SET_METHOD(exports, "checkbox", muCheckbox);
  NODE_SET_METHOD(exports, "textbox", muTextbox);
  NODE_SET_METHOD(exports, "text", muText);
  NODE_SET_METHOD(exports, "rect", muRect);
  NODE_SET_METHOD(exports, "layoutRow", muLayoutRow);
  NODE_SET_METHOD(exports, "beginColumn", muBeginColumn);
  NODE_SET_METHOD(exports, "endColumn", muEndColumn);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)