#ifndef GFX_H
#define GFX_H

#include "ktypes.h"

/* Cool slate desktop — light mist wallpaper, dark sidebar, high ink contrast */
#define COL_BG      0xE8EEF4u
#define COL_SIDE    0x2A3340u
#define COL_TEXT    0xF8FAFCu
#define COL_MUTED   0xA8B4C4u
#define COL_CYAN    0x0F766Eu
#define COL_GREEN   0x15803Du
#define COL_PURPLE  0x64748Bu
#define COL_AMBER   0xB45309u
#define COL_RED     0xDC2626u
#define COL_WHITE   0xFFFFFFFFu
#define COL_SHADOW  0x1E293Bu
#define COL_ACCENT  0x0F766Eu

/* Light window chrome — near-black ink on white panels */
#define COL_WIN     0xFFFFFFFFu
#define COL_WIN2    0xF1F5F9u
#define COL_TITLE   0xF8FAFCu
#define COL_INK     0x0F172Au
#define COL_INK2    0x475569u
#define COL_BORDER  0x94A3B8u
#define COL_PANEL   0xFFFFFFFFu
#define COL_PANEL2  0xF1F5F9u
#define COL_BTN     0xE2E8F0u
#define COL_BTN_BD  0x64748Bu
#define COL_TAB     0x3D4A5Cu
#define COL_TAB_ON  0x0F766Eu
#define COL_OK      0x15803Du
#define COL_DANGER  0xB91C1Cu

bool gfx_init(const boot_info_t *info);
int gfx_width(void);
int gfx_height(void);
uint32_t *gfx_fb(void);
uint32_t gfx_pitch_pixels(void);

void gfx_put(int x, int y, uint32_t color);
uint32_t gfx_get(int x, int y);
void gfx_fill(int x, int y, int w, int h, uint32_t color);
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_hline(int x, int y, int w, uint32_t color);
void gfx_vline(int x, int y, int h, uint32_t color);
void gfx_text(int x, int y, const char *text, uint32_t color);
void gfx_text_scaled(int x, int y, const char *text, uint32_t color, int scale);
int gfx_text_width(const char *text);
void gfx_char(int x, int y, char c, uint32_t color);
void gfx_blend(int x, int y, int w, int h, uint32_t color, int alpha);
uint32_t gfx_mix(uint32_t a, uint32_t b, int t);

void wallpaper_build(void);
void wallpaper_set_theme(int theme);
int wallpaper_get_theme(void);
const char *wallpaper_theme_name(void);
void wallpaper_blit_rect(int x, int y, int w, int h);
void wallpaper_full(void);
uint32_t wallpaper_pixel(int x, int y);

void gfx_present(void);
void gfx_present_rect(int x, int y, int w, int h);

void cursor_draw(int x, int y);
void cursor_restore(void);
void cursor_discard(void);

#endif
