#ifndef GUI_H
#define GUI_H
#include "defines.h" // For SCREEN_WIDTH and SCREEN_HEIGHT
#include "types.h"
#include "font.h"

// Some random high virtual address
// This is where our in-RAM buffer lives:
#define GUI_FRAMEBUFFER_VIRT ((0x18400000))

// ((65)) was a nice value for this:
#define LIGHT_MODE_LIGHTEN_AMOUNT ((65))
#define DARK_MODE_LIGHTEN_AMOUNT ((-65))

// ((45)) looks good here too:
#define BORDER_RADIUS ((37))

// Scalar amount that text from the vga_* methods is drawn in:
#define GUI_FONT_SCALAR ((7))

#define MENUBAR_FONT_COLOR ((0x1a1a1a))
#define MENUBAR_HEIGHT ((FONT_HEIGHT/6 + 3))


// Popup box dimensions:
#define GUI_POPUP_MAX_WIDTH ((320 * 2))
// * 2))
#define GUI_POPUP_MAX_HEIGHT ((105 * 3))
// * 3))

// Width and height for vga_* methods
// Divides the screen into individual characters- these are how many can fit
// per row and per column, respectively
// Drop these in anywhere you see VGA_WIDTH or VGA_HEIGHT and want to use GUI with vga_* methods, basically
#define GUI_FONT_SCREEN_WIDTH (((SCREEN_WIDTH * GUI_FONT_SCALAR)/ FONT_WIDTH))
#define GUI_FONT_SCREEN_HEIGHT (((SCREEN_HEIGHT * GUI_FONT_SCALAR)/ FONT_HEIGHT))

typedef union __attribute__((packed)) color {
    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };
    uint32_t value;
} color;

typedef struct __attribute__((packed)) bmp_header {
    uint16_t magic;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_data_offset;
} bmp_header;

typedef struct __attribute__((packed)) bmp_image_info {
    uint32_t todo;
} bmp_image_info;

/*******************************
 *
 * VGA Feature Parity methods
 *
 * "They're like the stuff from 
 * vga.h, but in HD! (TM)"
 *
 * The VGA methods are programmed to
 * use these methods if vga.c's
 * use_highres_gui is set
 *******************************/
void gui_setc(int x, int y, char c, uint32_t color_value);
void gui_popup(int w, int h, char *message);
void gui_popup_clear();
void gui_cursor_setxy(int x, int y, uint32_t color_value);
void gui_cursor_enable();
void gui_cursor_disable();

// Different than enable/ disable- this means we are ready to even think about
// drawing a cursor (framebuffer is in a valid state and ready to go by compositor)
void gui_cursor_activate();

/* The color used by VGA methods that wrap to GUI methods */
extern color gui_vga_api_color;

/*
 * gui_init
 *
 * Framebuffer- the physical address of the VBE VESA framebuffer
 *
 * Returns true on successful initialization of GUI, false otherwise
 * If this returns false, we could fallback to VGA.
 */
bool gui_init(color *framebuffer);

/*
 * gui_respring
 *
 * Redraw the entire desktop, including animation of the dock
 * This only writes into framebuffer, it doesn't actually update the display!
 * (Use gui_redraw to commit changes to the screen all at once).
 */
void gui_respring();

/*
 * gui_redraw
 *
 * Commit everything in framebuffer to the screen.
 *
 * Framebuffer should be used for windows, background, and static elements.
 * Dynamic elements should be drawn directly to the screen (so that framebuffer can be used)
 * to 'erase' them should they be erased.
 */
void gui_redraw ();

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
void gui_redraw_region (uint32_t x, uint32_t y, uint32_t w, uint32_t h);

/*
 * blur_region
 *
 * Blurs a region of a video buffer
 * This method is deprecated! It will be replaced when I update graphics API to
 * support composited layers and windows.
 *
 * For now this is used externally only in the login window method.
 */
void blur_region (color *dest, color *src, size_t w, size_t h, uint32_t region_x, uint32_t region_y, uint32_t region_w, uint32_t region_h, uint32_t border_radius, int32_t texture);

/*
 * blur
 *
 * Blurs an entire framebuffer of dimensions (w,h) with texture texture.
 */
void blur (color *dest, color *src, size_t w, size_t h, int32_t texture);
 
/*
 * gui_draw_string
 *
 * Draws a string at position (x,y) with scalar value scalar and color color_value.
 * (x,y) are in screen-space coordinates (SCREEN_WIDTH x SCREEN_HEIGHT).
 */
void gui_draw_string(uint32_t x, uint32_t y, char *str, uint32_t scalar, uint32_t color_value);

/*
 * gui_draw_string
 *
 * Draws a string at position (x,y) with scalar value scalar and color color_value.
 * (x,y) are in screen-space coordinates (SCREEN_WIDTH x SCREEN_HEIGHT).
 *
 * This draws into the framebuffer instead of videomem (so it cannot be erased without a repaint!)
 */
void gui_draw_string_framebuffer(uint32_t x, uint32_t y, char *str, uint32_t scalar, uint32_t color_value);

/*
 * gui_set_region_background
 *
 * Clears a window, some text, or whatever off the screen and draws the background image at
 * the specified region.
 */
void gui_set_region_background(uint32_t x, uint32_t y, size_t w, size_t h);

/*
 * gui_erase_region
 *
 * Replace a region of video memory with the framebuffer
 */
void gui_erase_region(uint32_t x, uint32_t y, size_t w, size_t h);

/*
 * gui_erase_text_region
 *
 * Replace a region of video memory with the framebuffer
 * Very similar to gui_erase_region, except the frame here is in VGA text coords, not
 * absolute screen coords. (IE, x of 1 really means FONT_WIDTH / GUI_FONT_SCALAR).
 */
void gui_erase_text_region(uint32_t x, uint32_t y, size_t w, size_t h);

/*
 * gui_menubar
 *
 * Draws a menubar with name in top-left, and mode in top-right.
 */
void gui_menubar(char *name, char *mode);

/*
 * gui_redraw_menubar
 *
 * Redraw the menubar region of the screen immediately,
 * without affecting the rest of the screen.
 */
void gui_redraw_menubar ();

// 7x7 Gaussian blur kernel:
#define GAUSSIAN_BLUR_SIZE ((7))
#define GAUSSIAN_BLUR_SUM ((64))

// Address of physical framebuffer:
extern color *videomem;

// Buffer we are drawing into (eventually copied to framebuffer):
extern color *framebuffer;

#endif
