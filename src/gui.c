#include "gui.h"
#include "util.h"
#include "filesystem.h"
#include "paging.h"
#include "font.h"

// Physical video memory:
color *videomem;

// In-RAM framebuffer we draw to:
color *framebuffer;

// Background image:
#define BG_BUF_SIZE ((BG_BUF_WIDTH * BG_BUF_HEIGHT))
color bgbuf[BG_BUF_SIZE];

// Color that VGA methods use when calling into GUI methods:
color gui_vga_api_color;

// Kernel for Gaussian Blurs:
static int8_t blur_kern[GAUSSIAN_BLUR_SIZE] = {1, 6, 15, 20, 15, 6, 1};

void gui_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    int i;
    memsetl(&videomem[((y+0) * SCREEN_WIDTH) + x], color, w);
    memsetl(&videomem[((y+h) * SCREEN_WIDTH) + x], color, w);

    // Vertical lines are a bit slower
    for (i = 0; i < h; i++) {
        videomem[((y+i) * SCREEN_WIDTH) + (x+0-0)].value = color;
        videomem[((y+i) * SCREEN_WIDTH) + (x+w-1)].value = color;
    }
}

void gui_draw_bmp(uint32_t x, uint32_t y, char *filename) {
    fd_t gui_fd;
    int i = 0;

    // Line buffer:
    color line_buffer[SCREEN_WIDTH];
    bmp_header header;

    // Attempt to open the file:
    if (!fs_open(&gui_fd, filename)) {
        return;
    }

    // Read header:
    fs_read(&gui_fd, (char *)&header, sizeof(header));

    // Seek:
    // @TODO: Use a real seek method
    gui_fd.fs_offset = header.pixel_data_offset;

    // Line by line draw the image to the screen:
    for (i = 0; i < 169; i++) {
        fs_read(&gui_fd, (char *)line_buffer, 300*sizeof(color));
        int x_offset = 0;
        for (x_offset = 0; x_offset < 300; x_offset++) {
            videomem[((y+i) * SCREEN_WIDTH) + (x + x_offset)] = line_buffer[x_offset];
        }
        //memcpy(&videomem[((y+i) * SCREEN_WIDTH) + x], ((char *)line_buffer), 400);
    }

    // Cleanup
    fs_close(&gui_fd);
}

// Helper method for gui_draw_char- by default gui_draw_char writes into videomem
// This lets you specify where the character will be drawn (videomem or framebuffer)
// NOTE: dest MUST be the same size as videomem! (Size of the screen)
static inline void _gui_draw_char(color *dest, uint32_t x, uint32_t y, char char_draw, uint32_t scalar, uint32_t color_value) {
    // scalar: scale down factor (1 = full size, 2 = half size, etc.)
    color col;
    uint32_t i, j, k;
    col.value = color_value;

    uint8_t font_table_idx = fontmap[(uint8_t)char_draw];

    if (char_draw == '\0') {
        // Clear this region!
        for (i = 0; i < FONT_HEIGHT; i+=scalar) {
            uint32_t local_y = ((i / scalar) + y);
            memcpy(&dest[local_y * SCREEN_WIDTH + x], &framebuffer[local_y * SCREEN_WIDTH + x], FONT_WIDTH);
        }
        return;
    }

    for (i = 0; i < FONT_WIDTH; i+=scalar) {
        for (j = 0; j < FONT_HEIGHT; j+=scalar) {
            uint32_t brightness = 0;
            uint32_t local_x = ((i / scalar) + x);
            uint32_t local_y = ((j / scalar) + y);

            // Subsampling:
            for (k = 0; k < scalar; k++) {
                uint16_t this_brightness = font[font_table_idx][j * (FONT_WIDTH >> 2) + ((i + k) >> 2)];

                this_brightness = (this_brightness >> ((3-i) % 4)) & 3;
                if (this_brightness == 1) this_brightness = (255/4);
                if (this_brightness == 2) this_brightness = 2 * (255/4);
                if (this_brightness == 3) this_brightness = 255;

                brightness += this_brightness;
            }
            brightness /= scalar;
            if (brightness > 255) { brightness = 255; }
            if (brightness < 0) { brightness = 0; }

            // This is really inefficient
            // @TODO: Double buffering here

            if (brightness != 0) {
                int32_t tmp_r = dest[(local_y * SCREEN_WIDTH) + local_x].r;
                int32_t tmp_g = dest[(local_y * SCREEN_WIDTH) + local_x].g;
                int32_t tmp_b = dest[(local_y * SCREEN_WIDTH) + local_x].b;

                if (brightness < 250) {
                    tmp_r = floor_div((tmp_r * (int32_t)(255 - brightness)), 255) + floor_div(col.r * brightness, 255);
                    tmp_g = floor_div((tmp_g * (int32_t)(255 - brightness)), 255) + floor_div(col.g * brightness, 255);
                    tmp_b = floor_div((tmp_b * (int32_t)(255 - brightness)), 255) + floor_div(col.b * brightness, 255);
                }
                else {
                    tmp_r = col.r;
                    tmp_g = col.g;
                    tmp_b = col.b;
                }

                if (tmp_r > 255) { tmp_r = 255; }
                if (tmp_r < 0) { tmp_r = 0; }
                if (tmp_g > 255) { tmp_g = 255; }
                if (tmp_g < 0) { tmp_g = 0; }
                if (tmp_b > 255) { tmp_b = 255; }
                if (tmp_b < 0) { tmp_b = 0; }

                dest[(local_y * SCREEN_WIDTH) + local_x].r = tmp_r;
                dest[(local_y * SCREEN_WIDTH) + local_x].g = tmp_g;
                dest[(local_y * SCREEN_WIDTH) + local_x].b = tmp_b;
            }
        }
    }
}

// Convert a coordinate from VGA-space to GUI-space
// Use this anywhere a text-mode coord needs to be mapped to fixed-position on-screen chars
static inline void vga_coords_to_gui (int x_in, int y_in, int *x_out, int *y_out) {
    *x_out = x_in * (FONT_WIDTH / GUI_FONT_SCALAR);
    *y_out = y_in * (FONT_HEIGHT / GUI_FONT_SCALAR);
}

// Draws a character directly to video memory
void gui_draw_char(uint32_t x, uint32_t y, char char_draw, uint32_t scalar, uint32_t color_value) {
    _gui_draw_char(videomem, x, y, char_draw, scalar, color_value);
}

void gui_setc(int x, int y, char c, uint32_t color_value) {
    gui_draw_char(x * (FONT_WIDTH/GUI_FONT_SCALAR),
        y * (FONT_HEIGHT/GUI_FONT_SCALAR),
        c,
        GUI_FONT_SCALAR,
        color_value);
}

static int gui_cursor_x = 0;
static int gui_cursor_y = 0;
static bool gui_cursor_enabled = false;
static uint32_t gui_cursor_color = 0;
static bool gui_cursor_activated = false; // This is enabled once the desktop has been drawn

void gui_cursor_setxy(int x_in, int y_in, uint32_t color_value) {
    static bool setxy_ever_called = false; // Has setxy ever been called?
    int x, y;
    vga_coords_to_gui(x_in, y_in, &x, &y);

    // Don't do anything until the cursor is activated by desktop compositor:
    if (!gui_cursor_activated) return;

    if (gui_cursor_enabled && setxy_ever_called) {
        // Only clear the cursor if it is currently enabled, and if setxy has been called
        // (gui_cursor_x and gui_cursor_y are valid)
        gui_draw_char(gui_cursor_x, gui_cursor_y, '\0', GUI_FONT_SCALAR, color_value);
    }
    
    setxy_ever_called = true;
    // To maintain feature parity with text-mode, need to change cursor pos, but not
    // update unless we are enabled
    gui_cursor_x = x;
    gui_cursor_y = y;
    if (gui_cursor_enabled) {
        gui_draw_char(x, y, '_', GUI_FONT_SCALAR, color_value);
        gui_cursor_color = color_value;
    }
}

void gui_cursor_enable() {
    // Don't do anything until the cursor is activated by desktop compositor:
    if (!gui_cursor_activated) return;

    gui_cursor_enabled = true;
    gui_draw_char(gui_cursor_x, gui_cursor_y, '_', GUI_FONT_SCALAR, gui_cursor_color);
}

void gui_cursor_disable() {
    // Don't do anything until the cursor is activated by desktop compositor:
    if (!gui_cursor_activated) return;

    if (gui_cursor_enabled) {
        gui_draw_char(gui_cursor_x, gui_cursor_y, '\0', GUI_FONT_SCALAR, 0);
    }
    gui_cursor_enabled = false;
}

// Different than enable/ disable- this means we are ready to even think about
// drawing a cursor (framebuffer is in a valid state and ready to go by compositor)
void gui_cursor_activate() {
    gui_cursor_activated = true;
}

// Popup width and height:
static uint32_t gui_popup_w = 0;
static uint32_t gui_popup_h = 0;

static color _gui_popup_cache[(GUI_POPUP_MAX_WIDTH * GUI_POPUP_MAX_HEIGHT)];

// Write videomem -> into gui popup cache
void _gui_popup_cache_save () {
    uint32_t j;
    for (j = 0; j < GUI_POPUP_MAX_HEIGHT; j++) {
        uint32_t vidmem_y = ((SCREEN_HEIGHT/2) - (GUI_POPUP_MAX_HEIGHT/2)) + j;
        uint32_t vidmem_x = (SCREEN_WIDTH/2) - (GUI_POPUP_MAX_WIDTH/2);
        memcpy(
            &_gui_popup_cache[j * GUI_POPUP_MAX_WIDTH],
            &videomem[(vidmem_y * SCREEN_WIDTH) + vidmem_x],
            sizeof(*videomem) * GUI_POPUP_MAX_WIDTH);
    }
}

// Write gui popup cache -> into videomem
void _gui_popup_cache_restore () {
    uint32_t j;
    for (j = 0; j < GUI_POPUP_MAX_HEIGHT; j++) {
        uint32_t vidmem_y = ((SCREEN_HEIGHT/2) - (GUI_POPUP_MAX_HEIGHT/2)) + j;
        uint32_t vidmem_x = (SCREEN_WIDTH/2) - (GUI_POPUP_MAX_WIDTH/2);
        memcpy(
            &videomem[(vidmem_y * SCREEN_WIDTH) + vidmem_x],
            &_gui_popup_cache[j * GUI_POPUP_MAX_WIDTH],
            sizeof(*videomem) * GUI_POPUP_MAX_WIDTH);
    }
}

void gui_popup(int w, int h, char *message) {
    gui_popup_w = w * (FONT_WIDTH/GUI_FONT_SCALAR);
    gui_popup_h = h * (FONT_HEIGHT/GUI_FONT_SCALAR);

    if (gui_popup_w > GUI_POPUP_MAX_WIDTH) {
        gui_popup_w = GUI_POPUP_MAX_WIDTH;
        w = ((gui_popup_w * GUI_FONT_SCALAR) / FONT_WIDTH) - 1;
    }

    if (gui_popup_h > GUI_POPUP_MAX_HEIGHT) {
        gui_popup_h = GUI_POPUP_MAX_HEIGHT;
        h = ((gui_popup_h * GUI_FONT_SCALAR) / FONT_HEIGHT) - 1;
    }

    _gui_popup_cache_save();

    // Technically this blur could go outside the max "allowed" alert zone
    // and leave artifacts when its cleared, but the -15 and + 30 factors
    // look literally perfect & nobody can make an alert big enough to trigger
    // the graphical glitch. I'm gonna just roll with it for now.
    // @TODO: Blur background image directly instead of framebuffer?
    blur_region(videomem, framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
        (SCREEN_WIDTH/2) - (gui_popup_w/2) - 15,
        (SCREEN_HEIGHT/2) - (gui_popup_h/2),
        gui_popup_w + 30,
        gui_popup_h,
        25,
        LIGHT_MODE_LIGHTEN_AMOUNT);

    char strbuf[w];

    // Flag is: uiuctf{aed05ba24417$#4c64411096}
    snprintf_unsafe(strbuf, w, message,33,
        0x75697563,
        0x74667b61,
        0x65643035,
        0x62613234,
        0x34313724,
        0x23346336,
        0x34343131,
        0x3039367d);



    size_t strbuflen = strlen(strbuf);
    //size_t msglen = strlen(message);

    if (strbuflen > w-2) {
        // Had to truncate it!
        strbuf[strbuflen-1] = '.';
        strbuf[strbuflen-2] = '.';
        strbuf[strbuflen-3] = '.';
    }

    // Shift to the right by this many pixels from window left edge to
    // center strbuf:
    size_t centering_factor = (strbuflen * FONT_WIDTH) / GUI_FONT_SCALAR;

    gui_draw_string((SCREEN_WIDTH/2) - (centering_factor/2), (SCREEN_HEIGHT/2) - (FONT_HEIGHT/ (2 * GUI_FONT_SCALAR)), strbuf, GUI_FONT_SCALAR, 0);
}

void gui_popup_clear() {
    _gui_popup_cache_restore();
}

// Helper method for gui_draw_string- by default gui_draw_string writes to videomemory directly
// Sometimes we want to write characters into framebuffer (like for menubar), so this method allows you to
// specify where the string will be drawn to
// NOTE: dest must be the same size as videomem (size of the screen!) for this to work!
void _gui_draw_string (color *dest, uint32_t x, uint32_t y, char *str, uint32_t scalar, uint32_t color_value) {
    char *cursor = str;
    uint32_t idx = 0;
    while (*cursor) {
        _gui_draw_char(dest, x + FONT_WIDTH/scalar * idx, y, *cursor, scalar, color_value);
        idx++;
        cursor++;
    }
}

void gui_draw_string(uint32_t x, uint32_t y, char *str, uint32_t scalar, uint32_t color_value) {
    _gui_draw_string(videomem, x, y, str, scalar, color_value);
}

/*
 * gui_draw_string
 *
 * Draws a string at position (x,y) with scalar value scalar and color color_value.
 * (x,y) are in screen-space coordinates (SCREEN_WIDTH x SCREEN_HEIGHT).
 *
 * This draws into the framebuffer instead of videomem (so it cannot be erased without a repaint!)
 */
void gui_draw_string_framebuffer(uint32_t x, uint32_t y, char *str, uint32_t scalar, uint32_t color_value) {
    _gui_draw_string(framebuffer, x, y, str, scalar, color_value);
}

// Downsample an image
void downsample (color *dest, size_t dest_w, size_t dest_h, color *src, size_t src_w, size_t src_h) {
    // (i,j) defines a point in downsampled space
    uint32_t i, j;

    for (i = 0; i < dest_w; i++) {
        for (j = 0; j < dest_h; j++) {
            uint32_t src_x = i * (src_w / dest_w);
            uint32_t src_y = j * (src_h / dest_h);
            if (src_x >= src_w) { src_x = src_w - 1; }
            if (src_y >= src_h) { src_y = src_h - 1; }
            dest[(j * dest_w) + i] = src[(src_y * src_w) + src_x];
        }
    }
}
// Upscale an image using bilinear interpolation
// Only writes into the region defined by (x, y, dest_w, dest_h) in dest frame space
void _upscale_region (color *dest, size_t dest_w, size_t dest_h, color *src, size_t src_w, size_t src_h, uint32_t x, uint32_t y, uint32_t region_w, uint32_t region_h, uint32_t border_radius, int32_t texture) {
    uint32_t x_small, x_large, y_small, y_large;
    uint32_t i, j;

    // I think this may be the first time in my entire life I've had to use a compount && statement in a for loop clause like this
    for (j = 0; (j < region_h) && (j < dest_h); j++) {
        for (i = 0; (i < region_w) && (i < dest_w); i++) {
            // (local_x, local_y) is a position in destination frame space
            uint32_t local_x = i + x;
            uint32_t local_y = j + y;

            // Check if this is a corner, if so- round it!
            // Corner status: None = not a corner, small = corresponds to smaller (top or left) valued side,
            // large = corresponds to larger (bottom or right) valued side
            enum corner_status_enum{NONE, SMALL, LARGE, EDGE};
            enum corner_status_enum corner_status_x = NONE;
            enum corner_status_enum corner_status_y = NONE;

            if (local_x - x < border_radius) { corner_status_x = SMALL; }
            else if (x + region_w - local_x < border_radius) { corner_status_x = (corner_status_x == SMALL) ? EDGE : LARGE; }
            if (local_y - y < border_radius) { corner_status_y = SMALL; }
            else if (y + region_h - local_y < border_radius) { corner_status_y = (corner_status_y == SMALL) ? EDGE : LARGE; }

            // Create center of corner circle:
            uint32_t corner_center_x = 0;
            uint32_t corner_center_y = 0;
            if (corner_status_x == SMALL) { corner_center_x = x + border_radius; }
            else if (corner_status_x == LARGE) { corner_center_x = (x + region_w) - border_radius; }
            else if (corner_status_x == EDGE) { corner_center_x = x + region_w / 2;}

            if (corner_status_y == SMALL) { corner_center_y = y + border_radius; }
            else if (corner_status_y == LARGE) { corner_center_y = (y + region_h) - border_radius; }
            else if (corner_status_y == EDGE) { corner_center_y = y + region_h / 2; }

            // If we are in a corner, check radius:
            if (corner_status_x != NONE && corner_status_y != NONE) {
                int32_t y_diff = ((int32_t)local_y - corner_center_y) * ((int32_t)local_y - corner_center_y);
                int32_t x_diff = ((int32_t)local_x - corner_center_x) * ((int32_t)local_x - corner_center_x);

                if (x_diff + y_diff > border_radius * border_radius) {
                    continue;
                }
            }

            // Known coords closest to our current point:
            x_small = floor_div((local_x * src_w), dest_w);
            y_small = floor_div((local_y * src_h), dest_h);
            x_large = ceil_div((local_x * src_w), dest_w);
            y_large = ceil_div((local_y * src_h), dest_h);

            if (x_large >= src_w) {
                x_large--;
            }

            if (y_large >= src_h) {
                y_large--;
            }

            // Value of this channel at the 4 nearby points:
            color val_11 = src[(y_small * src_w) + x_small];
            color val_12 = src[(y_large * src_w) + x_small];
            color val_21 = src[(y_small * src_w) + x_large];
            color val_22 = src[(y_large * src_w) + x_large];

            uint32_t scale_factor_x = ceil_div(dest_w, src_w);
            uint32_t scale_factor_y = ceil_div(dest_h, src_h);
            uint32_t x_1 = x_small * scale_factor_x;
            uint32_t x_2 = x_1 + scale_factor_x;
            uint32_t y_1 = y_small * scale_factor_y;
            uint32_t y_2 = y_1 + scale_factor_y;

            if (local_x > dest_w || local_y > dest_h) { continue; }

            uint32_t f_y1_r = (val_11.r * (x_2 - local_x)) / (x_2 - x_1) +
                              (val_21.r * (local_x - x_1)) / (x_2 - x_1);
            uint32_t f_y2_r = (val_12.r * (x_2 - local_x)) / (x_2 - x_1) +
                              (val_22.r * (local_x - x_1)) / (x_2 - x_1);
            uint32_t fval_r = (f_y1_r * (y_2 - local_y)) / (y_2 - y_1) +
                              (f_y2_r * (local_y - y_1)) / (y_2 - y_1);

            uint32_t f_y1_g = (val_11.g * (x_2 - local_x)) / (x_2 - x_1) +
                              (val_21.g * (local_x - x_1)) / (x_2 - x_1);
            uint32_t f_y2_g = (val_12.g * (x_2 - local_x)) / (x_2 - x_1) +
                              (val_22.g * (local_x - x_1)) / (x_2 - x_1);
            uint32_t fval_g = (f_y1_g * (y_2 - local_y)) / (y_2 - y_1) +
                              (f_y2_g * (local_y - y_1)) / (y_2 - y_1);

            uint32_t f_y1_b = (val_11.b * (x_2 - local_x)) / (x_2 - x_1) +
                              (val_21.b * (local_x - x_1)) / (x_2 - x_1);
            uint32_t f_y2_b = (val_12.b * (x_2 - local_x)) / (x_2 - x_1) +
                              (val_22.b * (local_x - x_1)) / (x_2 - x_1);
            uint32_t fval_b = (f_y1_b * (y_2 - local_y)) / (y_2 - y_1) +
                              (f_y2_b * (local_y - y_1)) / (y_2 - y_1);

            dest[(local_y * dest_w) + local_x].r = fval_r;
            dest[(local_y * dest_w) + local_x].g = fval_g;
            dest[(local_y * dest_w) + local_x].b = fval_b;
        }
    }
}

// Upscale an image using bilinear interpolation
void upscale (color *dest, size_t dest_w, size_t dest_h, color *src, size_t src_w, size_t src_h) {
    _upscale_region(dest, dest_w, dest_h, src, src_w, src_h,
        0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0);
}


// Blur downsample factor needs to be reduced for smaller resolutions:
// @TODO: Find a way to increase downsample factor for 800x600 mode
#if (SCREEN_WIDTH == 800)
#define BLUR_DOWNSAMPLE_FACTOR ((8))
#elif (SCREEN_WIDTH == 1024)
#define BLUR_DOWNSAMPLE_FACTOR ((32))
#elif (SCREEN_WIDTH == 1600)
#define BLUR_DOWNSAMPLE_FACTOR ((16))
#else
#define BLUR_DOWNSAMPLE_FACTOR ((8))
#endif

// Buffers used for blur operations:
color blurbuf[(SCREEN_WIDTH / BLUR_DOWNSAMPLE_FACTOR) * (SCREEN_HEIGHT / BLUR_DOWNSAMPLE_FACTOR)];
color blurbuf2[(SCREEN_WIDTH / BLUR_DOWNSAMPLE_FACTOR) * (SCREEN_HEIGHT / BLUR_DOWNSAMPLE_FACTOR)];

// Blur a region of an image
// Only modifies the blurred pixels in the dest buffer
void blur_region (color *dest, color *src, size_t w, size_t h, uint32_t region_x, uint32_t region_y, uint32_t region_w, uint32_t region_h, uint32_t border_radius, int32_t texture) {
    int i,j;
    // @TODO: FFT version of this

    // Convolve in 1D down to image 1/32 in both dimensions the size of the buffer:
    // We will then upscale back up to dest
    uint32_t downsample_w = (w / BLUR_DOWNSAMPLE_FACTOR);
    uint32_t downsample_h = (h / BLUR_DOWNSAMPLE_FACTOR);

    memsetl(blurbuf, 0, downsample_w * downsample_h);
    memsetl(blurbuf2, 0, downsample_w * downsample_h);

    // We downsample the image and then blur for performance, and upsample after
    // Downsample:
    downsample(blurbuf, downsample_w, downsample_h, src, w, h);

    // Blur:
    // Convolve in X:
    for (j = 0; j < downsample_h; j++) {
        for (i = 0; i < downsample_w; i++) {
            int32_t tmp_r = 0;
            int32_t tmp_g = 0;
            int32_t tmp_b = 0;

            // Accumulates the sum of all scalars applied
            // At edges, this will be less than the sum of all elements in the blur kernel:
            uint32_t kern_divisor = 0;

            uint32_t blur_idx = 0;
            for (blur_idx = 0; blur_idx < GAUSSIAN_BLUR_SIZE; blur_idx++) {
                int32_t i_relative = (i - (GAUSSIAN_BLUR_SIZE / 2) + blur_idx);

                if (i_relative > 0 && i_relative < w) {
                    tmp_r += blur_kern[blur_idx] * blurbuf[(j * downsample_w) + i_relative].r;
                    tmp_g += blur_kern[blur_idx] * blurbuf[(j * downsample_w) + i_relative].g;
                    tmp_b += blur_kern[blur_idx] * blurbuf[(j * downsample_w) + i_relative].b;

                    kern_divisor += blur_kern[blur_idx];
                }
            }

            if (kern_divisor != 0) {
                tmp_r /= kern_divisor;
                tmp_g /= kern_divisor;
                tmp_b /= kern_divisor;
            }

            blurbuf2[(j * downsample_w) + i].r = tmp_r;
            blurbuf2[(j * downsample_w) + i].g = tmp_g;
            blurbuf2[(j * downsample_w) + i].b = tmp_b;
        }
    }

    // Convolve again but in Y:
    for (j = 0; j < downsample_h; j++) {
        for (i = 0; i < downsample_w; i++) {
            int32_t tmp_r = 0;
            int32_t tmp_g = 0;
            int32_t tmp_b = 0;

            // Accumulates the sum of all scalars applied
            // At edges, this will be less than the sum of all elements in the blur kernel:
            int32_t kern_divisor = 0;

            uint32_t blur_idx = 0;
            for (blur_idx = 0; blur_idx < GAUSSIAN_BLUR_SIZE; blur_idx++) {
                int32_t j_relative = (j - (GAUSSIAN_BLUR_SIZE / 2) + blur_idx);

                if (j_relative > 0 && j_relative < h) {
                    tmp_r += blur_kern[blur_idx] * blurbuf2[(j_relative * downsample_w) + i].r;
                    tmp_g += blur_kern[blur_idx] * blurbuf2[(j_relative * downsample_w) + i].g;
                    tmp_b += blur_kern[blur_idx] * blurbuf2[(j_relative * downsample_w) + i].b;

                    kern_divisor += blur_kern[blur_idx];
                }
            }

            if (kern_divisor != 0) {
                tmp_r /= kern_divisor;
                tmp_g /= kern_divisor;
                tmp_b /= kern_divisor;
            }

            // Brighten/ darken material to give it a different feeling:
            // @TODO: Dark mode!!! :D
            tmp_r += texture;
            tmp_g += texture;
            tmp_b += texture;
            
            if (tmp_r < 0) { tmp_r = 0; }
            if (tmp_g < 0) { tmp_g = 0; }
            if (tmp_b < 0) { tmp_b = 0; }
            if (tmp_r > 255) { tmp_r = 255; }
            if (tmp_g > 255) { tmp_g = 255; }
            if (tmp_b > 255) { tmp_b = 255; }

            blurbuf[(j * downsample_w) + i].r = tmp_r;
            blurbuf[(j * downsample_w) + i].g = tmp_g;
            blurbuf[(j * downsample_w) + i].b = tmp_b;
        }
    }

    // Upsample:
    _upscale_region(dest, w, h, blurbuf, downsample_w, downsample_h, region_x, region_y, region_w, region_h, border_radius, texture);
}

// Blur an image
void blur (color *dest, color *src, size_t w, size_t h, int32_t texture) {
    blur_region(dest, src, w, h, 0, 0, w, h, 0, texture);
}

void gui_draw_background (char *filename) {
    fd_t gui_fd;

    // Line buffer:
    bmp_header header;

    // Attempt to open the file:
    if (!fs_open(&gui_fd, filename)) {
        return;
    }

    // Read header:
    fs_read(&gui_fd, (char *)&header, sizeof(header));

    // Seek:
    // @TODO: Use a real seek method
    gui_fd.fs_offset = header.pixel_data_offset;

    fs_read(&gui_fd, (char *)bgbuf, BG_BUF_SIZE *sizeof(color));

    // Bilinear interpolation:
    upscale(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, bgbuf, BG_BUF_WIDTH, BG_BUF_HEIGHT);

    //memcpy(videomem, framebuffer, SCREEN_SIZE * sizeof(color));

    // Cleanup
    fs_close(&gui_fd);
}

// Draws a menubar with name in top-left, and mode in top-right
void gui_menubar(char *name, char *mode) {
    gui_set_region_background(0, 0, SCREEN_WIDTH, MENUBAR_HEIGHT);
    blur_region(framebuffer, framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, SCREEN_WIDTH, MENUBAR_HEIGHT, 0, LIGHT_MODE_LIGHTEN_AMOUNT);
    _gui_draw_string(framebuffer, (SCREEN_WIDTH/2)-(FONT_WIDTH * 6)/12, 0, "pwnyOS", 6, MENUBAR_FONT_COLOR);

    _gui_draw_string(framebuffer, FONT_WIDTH/6, 0, name, 6, MENUBAR_FONT_COLOR);

    size_t mode_len = strlen(mode);
    _gui_draw_string(framebuffer, (SCREEN_WIDTH) - ((mode_len + 1) * FONT_WIDTH)/6, 0, mode, 6, MENUBAR_FONT_COLOR);
}

// Repaint the entire screen into framebuffer
void gui_respring () {
    // Draw desktop to framebuffer:
    gui_vga_api_color.value = 0xFFFFFF;
    memsetl(framebuffer, 0, SCREEN_SIZE);
    gui_draw_background("/prot/images/background"BG_FILE_APPEND);
    gui_menubar("","");

    //gui_redraw();
    //memcpy(videomem, framebuffer, SCREEN_SIZE * sizeof(*videomem));

    // Animate the dock
    // int i = 0;
    // while (i < 65) {
    //     blur_region(videomem, framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, (SCREEN_WIDTH/2) - 375, (SCREEN_HEIGHT-i), 750, 50, 30, 35);
    //     i+=2;
    // }
}

// Draw whatever is in framebuffer onto the screen
void gui_redraw () {
    memcpy(videomem, framebuffer, SCREEN_SIZE * sizeof(*videomem));
}

/*
 * gui_redraw_region
 *
 * Commit everything in framebuffer to the screen.
 *
 * Framebuffer should be used for windows, background, and static elements.
 * Dynamic elements should be drawn directly to the screen (so that framebuffer can be used)
 * to 'erase' them should they be erased.
 *
 * Region is in screen-space coordinates (SCREEN_WIDTH, SCREEN_HEIGHT) space.
 */
void gui_redraw_region (uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t j;
    for (j = y; j < y+h; j++) {
        memcpy(&videomem[(j * SCREEN_WIDTH) + x],
            &framebuffer[(j * SCREEN_WIDTH) + x],
            sizeof(*videomem) * w);
    }
}

// Just redraw the menubar region
// This exists so apps can change it without having to redraw the whole screen
void gui_redraw_menubar () {
    gui_redraw_region(0, 0, SCREEN_WIDTH, MENUBAR_HEIGHT);
}

/*
 * gui_set_region_background
 *
 * Clears a window, some text, or whatever off the screen and draws the background image at
 * the specified region.
 */
void gui_set_region_background(uint32_t x, uint32_t y, size_t w, size_t h) {
    _upscale_region(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, bgbuf, BG_BUF_WIDTH, BG_BUF_HEIGHT,
        x, y, w, h,
        0, 0);
}

/*
 * gui_erase_region
 *
 * Replace a region of video memory with the framebuffer
 */
void gui_erase_region(uint32_t x, uint32_t y, size_t w, size_t h) {
    // @TODO: Test this method
    _upscale_region(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
        x, y, w, h,
        0, 0);
    gui_redraw_region(x, y, w, h);
}

/*
 * gui_erase_text_region
 *
 * Replace a region of video memory with the framebuffer
 * Very similar to gui_erase_region, except the frame here is in VGA text coords, not
 * absolute screen coords. (IE, x of 1 really means FONT_WIDTH / GUI_FONT_SCALAR).
 */
void gui_erase_text_region(uint32_t x, uint32_t y, size_t w, size_t h) {
    // Try 1:
    // _upscale_region(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
    //     x * GUI_FONT_SCALAR / FONT_WIDTH,
    //     y * GUI_FONT_SCALAR / FONT_HEIGHT,
    //     w * GUI_FONT_SCALAR / FONT_WIDTH,
    //     h * GUI_FONT_SCALAR / FONT_HEIGHT,
    //     0, 0);

    // Try 2:
    // Way too slow
    //gui_erase_region(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Try 3:
    // Too slow, but does work
    //gui_redraw();

    // Try 4:
    uint32_t i;
    uint32_t base_y = (y * GUI_FONT_SCALAR) / FONT_HEIGHT;
    uint32_t base_x = (x * GUI_FONT_SCALAR) / FONT_WIDTH;
    for (i = 0; i < (h * GUI_FONT_SCALAR) / FONT_HEIGHT; i++) {
        uint32_t local_y = base_y + i;
        memcpy(&videomem[(local_y * SCREEN_WIDTH) + base_x],
            &framebuffer[(local_y * SCREEN_WIDTH) + base_x],
            (w * GUI_FONT_SCALAR) / FONT_WIDTH);
    }
}

/*
 * gui_init
 *
 * Framebuffer- the physical address of the VBE VESA framebuffer
 *
 * Returns true on successful initialization of GUI, false otherwise
 * If this returns false, we could fallback to VGA.
 */
bool gui_init(color *videomem) {
    // Allocate a framebuffer using a huge page:
    uint32_t *framebuffer_phys = alloc_huge_page();

    // Check for errors allocating the pages
    if (!framebuffer_phys) {
        return false;
    }

    uint32_t *framebuffer_phys_2 = alloc_huge_page();

    if (!framebuffer_phys_2) {
        free_huge_page(framebuffer_phys);
        return false;
    }

    // Map framebuffer pages:
    framebuffer = (color *)GUI_FRAMEBUFFER_VIRT;
    map_huge_page_kern((uint32_t)framebuffer, (uint32_t)framebuffer_phys);
    map_huge_page_kern((uint32_t)framebuffer + HUGE_PAGE_SIZE, (uint32_t)framebuffer_phys_2);

    gui_vga_api_color.value = 0xFFFFFF;

    return true;
}
