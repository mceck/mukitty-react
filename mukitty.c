// Node.js plugin that integrates MicroUI rendering with Kitty terminal graphics.
#include <ctype.h>
#include <node_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "microui.h"
// Bitmap font for rendering text.
#include "c64_font.h"

#define FONT_SIZE 8
#define RESW 4 // Character width in pixels.
#define RESH 8 // Character height in pixels.

#define MAX_STR_LEN 256
#define MAX_INPUT_IDS 32
#define TARGET_FPS 60.0
#define FRAME_TIME (1.0 / TARGET_FPS)

#define TRACE_LOGS 1
#define LOG(fmt, ...) TRACE_LOGS ? printf("\33[2K\r" fmt, ##__VA_ARGS__) : 0

#define node_parse_args()                                 \
    size_t argc;                                          \
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL); \
    napi_value args[argc];                                \
    napi_get_cb_info(env, info, &argc, args, NULL, NULL)

#define node_get_string(index, buf) \
    napi_get_value_string_utf8(env, args[index], buf, sizeof(buf), NULL);

#define node_bool_to_napi_val(value)       \
    ({                                     \
        napi_value _r;                     \
        napi_get_boolean(env, value, &_r); \
        _r;                                \
    })

#define node_float_to_napi_val(value)                \
    ({                                               \
        napi_value _r;                               \
        napi_create_double(env, (double)value, &_r); \
        _r;                                          \
    })

#define node_export_fn(name, func)                            \
    do {                                                      \
        napi_value _fn;                                       \
        napi_create_function(env, NULL, 0, func, NULL, &_fn); \
        napi_set_named_property(env, exports, name, _fn);     \
    } while (0)

/* Global configuration (mostly from command line options). */
struct {
    bool ghostty_mode;       // Use non standard Kitty protocol that works with
                             // Ghostty, and allows animation, but is incompatible
                             // with Kitty.
    bool kitty_mode;         // Use graphics protocol with animation codes, this
                             // is needed for the Kitty terminal.
    int width_chars;         // display width in characters.
    int height_chars;        // display height in characters.
    int width;               // display width in pixels.
    int height;              // display height in pixels.
    unsigned long render_id; // Unique ID for the current render session.
} Config;
static struct {
    int x, y, w, h;
    bool enabled;
} clip_rect = {0};

struct termios orig_termios;
static uint8_t *fb = NULL; // Framebuffer pointer
static int frame_number = 0;
static mu_Context ctx;

// Function to encode data to base64
size_t base64_encode(size_t input_length, char *encoded_data) {
    const unsigned char *data = fb;
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

// Update display using Kitty graphics protocol
void kitty_update_display() {
    // Calculate base64 encoded size
    size_t bitmap_size = Config.width * Config.height * 3;
    size_t encoded_size = 4 * ((bitmap_size + 2) / 3);
    char *encoded_data = malloc(encoded_size + 1);

    if (!encoded_data) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    // Encode the bitmap data to base64
    base64_encode(bitmap_size, encoded_data);
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
                       frame_number == 0 ? 'T' : 't', Config.render_id, Config.width,
                       Config.height, Config.width_chars, Config.height_chars,
                       more_chunks);
            } else {
                if (frame_number == 0) {
                    printf(
                        "\033_Ga=T,i=%lu,f=24,s=%d,v=%d,q=2,"
                        "c=%d,r=%d,m=%d;",
                        Config.render_id, Config.width, Config.height,
                        Config.width_chars, Config.height_chars, more_chunks);
                } else {
                    printf("\033_Ga=f,r=1,i=%lu,f=24,x=0,y=0,s=%d,v=%d,m=%d;",
                           Config.render_id, Config.width, Config.height, more_chunks);
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
        printf("\033_Ga=a,c=1,i=%lu;", Config.render_id);
        printf("\033\\");
    }

    /* When the image is created, add a newline so that the cursor
     * is more naturally placed under the image, not at the right/bottom
     * corner. */
    if (frame_number == 0) {
        if (TRACE_LOGS && Config.ghostty_mode)
            printf("\r\n");
        frame_number++;
    }

    fflush(stdout);
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
            LOG("Escape key pressed, quitting...");
            return 1; // Request quit
        }
        if (buf[0] == 127) {
            LOG("Backspace key pressed");
            mu_input_keydown(ctx, MU_KEY_BACKSPACE);
            return 0;
        }
        if (isprint(buf[0])) {
            LOG("Key pressed: %c", buf[0]);
            mu_input_text(ctx, buf);
            return 0;
        }
        if (buf[0] == 13) {
            LOG("Enter key pressed");
            mu_input_keydown(ctx, MU_KEY_RETURN);
            return 0;
        }
        if (buf[0] == 3) {
            LOG("Ctrl+C pressed, quitting...");
            return 1; // Request quit
        }
        LOG("Non-printable key pressed: %d", buf[0]);
        return 0;
    }

    // Mouse event parsing (SGR format: \033[<BTN;X;Ym or \033[<BTN;X;YM)
    if (nread > 5 && strncmp(buf, "\033[<", 3) == 0) {
        int button, x, y;
        char event_type;

        if (sscanf(buf, "\033[<%d;%d;%d%c", &button, &x, &y, &event_type) == 4) {
            // Terminal coordinates are 1-based, convert to 0-based
            // We also convert from character cell coordinates to pixel coordinates.
            int pixel_x = (x - 1) * Config.width / Config.width_chars;
            int pixel_y = (y - 1) * Config.height / Config.height_chars;

            // Handle motion event
            if ((button & 32)) {
                LOG("Mouse moved to %d,%d [%d,%d]", pixel_x, pixel_y, x, y);
                mu_input_mousemove(ctx, pixel_x, pixel_y);
            }

            // Handle scroll wheel
            if ((button & 64)) {
                if (button == 64) { // wheel up
                    LOG("Mouse wheel up");
                    mu_input_scroll(ctx, 0, -FONT_SIZE);
                } else if (button == 65) { // wheel down
                    LOG("Mouse wheel down");
                    mu_input_scroll(ctx, 0, FONT_SIZE);
                } else if (button == 66) { // wheel left
                    LOG("Mouse wheel left");
                    mu_input_scroll(ctx, -FONT_SIZE, 0);
                } else if (button == 67) { // wheel right
                    LOG("Mouse wheel right");
                    mu_input_scroll(ctx, FONT_SIZE, 0);
                } else {
                    LOG("Unknown scroll event: %d", button);
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
                    LOG("Mouse button %d pressed at %d,%d [%d,%d]", mu_button, pixel_x,
                        pixel_y, x, y);
                    mu_input_mousedown(ctx, pixel_x, pixel_y, mu_button);
                } else if (event_type == 'm') { // Release
                    LOG("Mouse button %d released at %d,%d [%d,%d]", mu_button, pixel_x,
                        pixel_y, x, y);
                    mu_input_mouseup(ctx, pixel_x, pixel_y, mu_button);
                }
            }
        }
        return 0;
    }

    LOG("Unrecognized input: %d - %d", nread, buf[0]);
    return 0;
}

void crt_set_pixel(int x, int y, uint32_t color) {
    if (x >= Config.width || y >= Config.height)
        return;
    if (clip_rect.enabled) {
        if (x < clip_rect.x || y < clip_rect.y ||
            x >= clip_rect.x + clip_rect.w ||
            y >= clip_rect.y + clip_rect.h) {
            return; // Outside clipping region
        }
    }

    uint8_t *dst = fb + (x * 3 + y * Config.width * 3);
    dst[0] = (color >> 16) & 0xff; // R
    dst[1] = (color >> 8) & 0xff;  // G
    dst[2] = color & 0xff;         // B
}

void init_config() {
    srand(getpid());
    const char *term = getenv("TERM");
    Config.render_id = rand();
    if (term && strstr(term, "ghostty") != NULL) {
        Config.ghostty_mode = true;
        Config.kitty_mode = false;
    } else if (term && strstr(term, "kitty") != NULL) {
        Config.ghostty_mode = false;
        Config.kitty_mode = true;
    } else {
        fprintf(stderr, "Error: Unsupported terminal type '%s'.\n", term);
        exit(1);
    }
}

uint32_t toColor(mu_Color color) {
    return (color.r << 16) | (color.g << 8) | color.b;
}

void draw_rectangle(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            crt_set_pixel(x + j, y + i, color);
        }
    }
}

void draw_char(int x, int y, char c, uint32_t color) {
    uint8_t ch = (uint8_t)c;

    if (ch > 126)
        ch = 32; // Replace invalid chars with space

    const uint8_t *glyph = font_8x8[ch];

    for (int row = 0; row < 8; row++) {
        uint8_t line = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                crt_set_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_text(int x, int y, char *str, uint32_t color) {
    if (!str)
        return;
    int current_x = x, current_y = y;
    int char_width = 8, char_height = 8;

    while (*str) {
        if (*str == '\n') {
            current_x = x;
            current_y += char_height;
        } else {
            draw_char(current_x, current_y, *str, color);
            current_x += char_width;
        }
        str++;
    }
}

void draw_icon(int id, mu_Rect rect, mu_Color color) {
    int x = rect.x + (rect.w - 8) / 2;
    int y = rect.y + (rect.h - 8) / 2;
    draw_char(x, y, id, toColor(color));
}

void set_clip_rect(int x, int y, int w, int h) {
    if (w == 0x1000000 && h == 0x1000000) {
        clip_rect.enabled = false;
    } else {
        clip_rect.x = x;
        clip_rect.y = y;
        clip_rect.w = w;
        clip_rect.h = h;
        clip_rect.enabled = true;
    }
}

void updateWindowSize(int width, int height) {
    if (fb && width == Config.width_chars && height == Config.height_chars) {
        return;
    }
    Config.width_chars = width;
    Config.height_chars = height;
    Config.width = (width * RESW);
    Config.height = (height * RESH);
    size_t fb_size = Config.width * Config.height * 3;
    if (fb) {
        fb = realloc(fb, fb_size);
        // reset root container size
        mu_Container *root = mu_get_container(&ctx, "root");
        root->rect.w = 0;
        if (Config.kitty_mode) {
            // Clear the screen in Kitty mode
            printf("\033[3J\033[H");
            fflush(stdout);
        }
    } else {
        fb = malloc(fb_size);
    }
    memset(fb, 0, fb_size); // Clear framebuffer
    frame_number = 0;
}

int getTextWidth(mu_Font font, const char *str, int len) {
    if (len < 0)
        len = strlen(str);
    return len * FONT_SIZE;
}

int getTextHeight(mu_Font font) { return FONT_SIZE; }

napi_value muButton(napi_env env, napi_callback_info info) {
    node_parse_args();
    char text[MAX_STR_LEN];
    node_get_string(0, text);

    bool result = mu_button(&ctx, text);
    return node_bool_to_napi_val(result);
}

napi_value muLabel(napi_env env, napi_callback_info info) {
    node_parse_args();
    char text[MAX_STR_LEN];
    node_get_string(0, text);

    mu_label(&ctx, text);
    return NULL;
}

napi_value muSlider(napi_env env, napi_callback_info info) {
    node_parse_args();
    int min, max;
    double value;
    napi_get_value_int32(env, args[0], &min);
    napi_get_value_int32(env, args[1], &max);
    napi_get_value_double(env, args[2], &value);

    float float_value = (float)value;
    mu_slider(&ctx, &float_value, min, max);
    return node_float_to_napi_val(float_value);
}

napi_value muCheckbox(napi_env env, napi_callback_info info) {
    node_parse_args();
    bool checked;
    char text[MAX_STR_LEN];
    napi_get_value_bool(env, args[0], &checked);
    node_get_string(1, text);

    int int_checked = checked;
    mu_checkbox(&ctx, text, &int_checked);
    return node_bool_to_napi_val(int_checked);
}

napi_value muTextbox(napi_env env, napi_callback_info info) {
    node_parse_args();
    int id;
    napi_get_value_int32(env, args[0], &id);
    if (id < 0) id = 0;
    if (id >= MAX_INPUT_IDS) id = id % MAX_INPUT_IDS;
    static char text[MAX_INPUT_IDS][MAX_STR_LEN];
    node_get_string(1, text[id]);

    int submit = mu_textbox(&ctx, text[id], sizeof(text[id])) & MU_RES_SUBMIT;
    napi_value result, text_val, submit_val;
    napi_create_object(env, &result);
    napi_create_string_utf8(env, text[id], NAPI_AUTO_LENGTH, &text_val);
    napi_get_boolean(env, submit != 0, &submit_val);
    napi_set_named_property(env, result, "text", text_val);
    napi_set_named_property(env, result, "submit", submit_val);
    return result;
}

napi_value muText(napi_env env, napi_callback_info info) {
    node_parse_args();
    char text[MAX_STR_LEN];
    node_get_string(0, text);

    mu_text(&ctx, text);
    return NULL;
}

napi_value muRect(napi_env env, napi_callback_info info) {
    node_parse_args();
    uint32_t color;
    napi_get_value_uint32(env, args[0], &color);

    mu_Rect r = mu_layout_next(&ctx);
    mu_Color mu_color = {(unsigned char)((color >> 16) & 0xFF),
                         (unsigned char)((color >> 8) & 0xFF),
                         (unsigned char)(color & 0xFF), 255};
    mu_draw_rect(&ctx, r, mu_color);
    return NULL;
}

napi_value muBeginWindow(napi_env env, napi_callback_info info) {
    node_parse_args();
    char name[MAX_STR_LEN];
    node_get_string(0, name);

    int top = 0, left = 0, width = Config.width, height = Config.height;
    int opt = MU_OPT_NOCLOSE | MU_OPT_NOTITLE | MU_OPT_NORESIZE;
    bool isModal = strcmp(name, "root");
    mu_Container *modalCnt = NULL;
    // modal
    if (isModal) {
        napi_get_value_int32(env, args[1], &top);
        napi_get_value_int32(env, args[2], &left);
        napi_get_value_int32(env, args[3], &width);
        napi_get_value_int32(env, args[4], &height);
        opt = MU_OPT_HOLDFOCUS;
        modalCnt = mu_get_container(&ctx, name);
        mu_bring_to_front(&ctx, modalCnt);
    }

    int ret =
        mu_begin_window_ex(&ctx, name, mu_rect(left, top, width, height), opt);
    if (!ret && modalCnt) {
        // "reopen" the modal after close because the show/hide is managed
        // externally
        modalCnt->open = 1;
    }
    return node_bool_to_napi_val(ret != 0);
}

napi_value muEndWindow(napi_env env, napi_callback_info info) {
    mu_end_window(&ctx);
    return NULL;
}

napi_value muLayoutRow(napi_env env, napi_callback_info info) {
    node_parse_args();
    int height = 0, items = 0;
    int *widths = NULL;
    if (argc)
        napi_get_value_int32(env, args[0], &height);
    if (argc > 1) {
        items = argc - 1;
        widths = malloc(items * sizeof(int));
        for (int i = 0; i < items; i++) {
            napi_get_value_int32(env, args[1 + i], &widths[i]);
        }
    }
    mu_layout_row(&ctx, items, widths, height);
    if (widths) {
        free(widths);
    }
    return NULL;
}

napi_value muBeginColumn(napi_env env, napi_callback_info info) {
    mu_layout_begin_column(&ctx);
    return NULL;
}

napi_value muEndColumn(napi_env env, napi_callback_info info) {
    mu_layout_end_column(&ctx);
    return NULL;
}

double get_time_sec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + (tv.tv_usec / 1000000.0);
}

void limit_fps() {
    static double last_frame_ts = 0.0;
    static double last_fps_ts = 0.0;
    static uint32_t frame_count = 0;

    double current_time = get_time_sec();
    double elapsed_frame = current_time - last_frame_ts;

    // cap fps
    if (last_frame_ts && elapsed_frame < FRAME_TIME) {
        useconds_t u = (useconds_t)((FRAME_TIME - elapsed_frame) * 1000000.0);
        usleep(u);
        current_time = get_time_sec();
    }

    // log fps
    frame_count++;
    if (current_time - last_fps_ts >= 1.0) {
        double fps = frame_count / (current_time - last_fps_ts);
        LOG("FPS: %.2f", fps);
        frame_count = 1;
        last_fps_ts = current_time;
    }

    last_frame_ts = current_time;
}

napi_value handleInputs(napi_env env, napi_callback_info info) {
    int exit = process_input(&ctx);

    napi_value return_value;
    napi_get_boolean(env, exit != 0, &return_value);
    return return_value;
}

napi_value muBegin(napi_env env, napi_callback_info info) {
    struct winsize ts = get_terminal_size();
    updateWindowSize(ts.ws_col, ts.ws_row - 1);
    mu_begin(&ctx);
    return NULL;
}

napi_value muEnd(napi_env env, napi_callback_info info) {
    mu_end(&ctx);

    mu_Command *cmd = NULL;
    while (mu_next_command(&ctx, &cmd)) {
        switch (cmd->type) {
        case MU_COMMAND_TEXT:
            draw_text(cmd->text.pos.x, cmd->text.pos.y, cmd->text.str,
                      toColor(cmd->text.color));
            break;
        case MU_COMMAND_RECT:
            draw_rectangle(cmd->rect.rect.x, cmd->rect.rect.y, cmd->rect.rect.w,
                           cmd->rect.rect.h, toColor(cmd->rect.color));
            break;
        case MU_COMMAND_ICON:
            draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
            break;
        case MU_COMMAND_CLIP:
            set_clip_rect(cmd->clip.rect.x, cmd->clip.rect.y,
                          cmd->clip.rect.w, cmd->clip.rect.h);
            break;
        }
    }
    kitty_update_display();
    limit_fps();
    return NULL;
}

napi_value muBeginTreeNode(napi_env env, napi_callback_info info) {
    node_parse_args();
    char text[MAX_STR_LEN];
    node_get_string(0, text);
    int opt = 0;
    bool expanded;
    napi_get_value_bool(env, args[1], &expanded);
    if (expanded) {
        opt = MU_OPT_EXPANDED;
    }

    int open = mu_begin_treenode_ex(&ctx, text, opt);
    return node_bool_to_napi_val(open != 0);
}

napi_value muEndTreeNode(napi_env env, napi_callback_info info) {
    mu_end_treenode(&ctx);
    return NULL;
}

napi_value muHeader(napi_env env, napi_callback_info info) {
    node_parse_args();
    char text[MAX_STR_LEN];
    node_get_string(0, text);
    int opt = 0;
    bool expanded;
    napi_get_value_bool(env, args[1], &expanded);
    if (expanded) {
        opt = MU_OPT_EXPANDED;
    }

    int open = mu_header_ex(&ctx, text, opt);
    return node_bool_to_napi_val(open != 0);
}

napi_value muBeginPanel(napi_env env, napi_callback_info info) {
    node_parse_args();
    char text[MAX_STR_LEN];
    node_get_string(0, text);

    mu_begin_panel(&ctx, text);
    return NULL;
}

napi_value muEndPanel(napi_env env, napi_callback_info info) {
    mu_end_panel(&ctx);
    return NULL;
}

napi_value initWindow(napi_env env, napi_callback_info info) {
    node_parse_args();
    mu_init(&ctx);
    ctx.text_width = getTextWidth;
    ctx.text_height = getTextHeight;
    init_config();
    enable_raw_mode();
    return NULL;
}

napi_value closeWindow(napi_env env, napi_callback_info info) {
    if (fb) {
        free(fb);
        fb = NULL;
    }
    disable_raw_mode();
    return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
    node_export_fn("init", initWindow);
    node_export_fn("close", closeWindow);
    node_export_fn("handleInputs", handleInputs);
    node_export_fn("begin", muBegin);
    node_export_fn("end", muEnd);
    node_export_fn("beginWindow", muBeginWindow);
    node_export_fn("endWindow", muEndWindow);
    node_export_fn("button", muButton);
    node_export_fn("label", muLabel);
    node_export_fn("slider", muSlider);
    node_export_fn("checkbox", muCheckbox);
    node_export_fn("textbox", muTextbox);
    node_export_fn("text", muText);
    node_export_fn("rect", muRect);
    node_export_fn("layoutRow", muLayoutRow);
    node_export_fn("beginColumn", muBeginColumn);
    node_export_fn("endColumn", muEndColumn);
    node_export_fn("beginTreeNode", muBeginTreeNode);
    node_export_fn("endTreeNode", muEndTreeNode);
    node_export_fn("header", muHeader);
    node_export_fn("beginPanel", muBeginPanel);
    node_export_fn("endPanel", muEndPanel);

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
