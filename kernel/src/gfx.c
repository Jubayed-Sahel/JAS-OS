#include "gfx.h"
#include "klib.h"

#define WALLPAPER_BASE ((uint32_t *)0x200000u)
#define BACKBUFFER_BASE ((uint32_t *)0x800000u)

static uint8_t *s_lfb;
static uint32_t *s_back;
static int s_w, s_h, s_pitch, s_bpp;
static uint32_t *s_wall;
static int s_wallpaper_theme;
static int s_cx = -1, s_cy = -1, s_cw, s_ch;

/* Public-domain 8x8 font for ASCII 32-126. */
static const uint8_t FONT8[95][8] = {
    {0,0,0,0,0,0,0,0},
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    {0x36,0x36,0x12,0,0,0,0,0},
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0},
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0},
    {0x06,0x06,0x03,0,0,0,0,0},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0},
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0,0},
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0,0},
    {0,0,0,0,0x0C,0x0C,0x06,0},
    {0,0,0,0x3F,0,0,0,0},
    {0,0,0,0,0,0x0C,0x0C,0},
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0},
    {0x1E,0x33,0x3B,0x3F,0x37,0x33,0x1E,0},
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0},
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0},
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0},
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0},
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0},
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0},
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0},
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0},
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0},
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0},
    {0x7F,0x06,0x06,0x3E,0x06,0x06,0x7F,0},
    {0x7F,0x06,0x06,0x3E,0x06,0x06,0x06,0},
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0},
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0},
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0},
    {0x06,0x06,0x06,0x06,0x06,0x06,0x7F,0},
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0},
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x06,0},
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0},
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0},
    {0x3F,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0},
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0},
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0},
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0},
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x0C,0},
    {0x7F,0x60,0x30,0x18,0x0C,0x06,0x7F,0},
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0},
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0},
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0},
    {0x08,0x1C,0x36,0x63,0,0,0,0},
    {0,0,0,0,0,0,0,0xFF},
    {0x0C,0x0C,0x18,0,0,0,0,0},
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0},
    {0x06,0x06,0x1E,0x36,0x36,0x36,0x1F,0},
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0},
    {0x30,0x30,0x3C,0x36,0x36,0x36,0x7C,0},
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0},
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x06,0},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    {0x06,0x06,0x3E,0x36,0x36,0x36,0x36,0},
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0},
    {0x18,0x00,0x1C,0x18,0x18,0x18,0x18,0x0F},
    {0x06,0x06,0x36,0x1E,0x36,0x36,0x36,0},
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0},
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0},
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0},
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0},
    {0x00,0x00,0x1B,0x36,0x36,0x1E,0x06,0x06},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x30},
    {0x00,0x00,0x1B,0x36,0x06,0x06,0x0F,0},
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0},
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0},
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0},
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0},
    {0x00,0x00,0x63,0x6B,0x7F,0x36,0x36,0},
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0},
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0},
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0},
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0},
    {0x6E,0x3B,0,0,0,0,0,0},
};

uint32_t gfx_mix(uint32_t a, uint32_t b, int t)
{
    if (t <= 0) return a;
    if (t >= 256) return b;
    int ra = (a >> 16) & 255, ga = (a >> 8) & 255, ba = a & 255;
    int rb = (b >> 16) & 255, gb = (b >> 8) & 255, bb = b & 255;
    int r = ra + ((rb - ra) * t) / 256;
    int g = ga + ((gb - ga) * t) / 256;
    int bl = ba + ((bb - ba) * t) / 256;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

static uint32_t hash_xy(int x, int y)
{
    uint32_t n = (uint32_t)(x * 374761393u + y * 668265263u);
    n = (n ^ (n >> 13)) * 1274126177u;
    return n ^ (n >> 16);
}

uint32_t wallpaper_pixel(int x, int y)
{
    /* Cool daylight mist: soft slate sky → pale horizon, no navy/cyan. */
    int w = s_w, h = s_h;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    int ty = (y * 256) / h;
    int tx = (x * 256) / w;
    int dither = (int)(hash_xy(x, y) & 3) - 1;
    ty += dither;
    if (ty < 0) ty = 0;
    if (ty > 256) ty = 256;

    uint32_t sky_l, sky_r, mist_l, mist_r, glow, wash, hill_color, edge;
    if (s_wallpaper_theme == 1) {
        sky_l = 0xD8ECF4u; sky_r = 0xB9DCEAu;
        mist_l = 0x89B8CCu; mist_r = 0x6CA2BAu;
        glow = 0xE7F8FFu; wash = 0xBBDDEAu; hill_color = 0x477F98u; edge = 0x5A899Du;
    } else if (s_wallpaper_theme == 2) {
        sky_l = 0x253247u; sky_r = 0x182235u;
        mist_l = 0x172033u; mist_r = 0x0F172Au;
        glow = 0x6B6A91u; wash = 0x334155u; hill_color = 0x0B1220u; edge = 0x111827u;
    } else {
        sky_l = 0xE4EAF0u; sky_r = 0xEEF2F6u;
        mist_l = 0xC8D2DCu; mist_r = 0xB8C4D0u;
        glow = 0xF3EDE2u; wash = 0xD5DEE8u; hill_color = 0x9AA8B6u; edge = 0xA8B4C0u;
    }
    uint32_t top = gfx_mix(sky_l, sky_r, tx);
    uint32_t bot = gfx_mix(mist_l, mist_r, tx);
    uint32_t col = gfx_mix(top, bot, ty);

    /* Soft warm sun disc (low contrast) */
    int mx = w * 24 / 100, my = h * 20 / 100;
    int dx = x - mx, dy = y - my;
    int dist2 = dx * dx + dy * dy;
    int glow_r = 200;
    if (dist2 < glow_r * glow_r) {
        int fall = 256 - (dist2 * 256) / (glow_r * glow_r);
        col = gfx_mix(col, glow, fall / 6);
    }

    /* Quiet horizontal wash — geometric, not noisy stars */
    if (((y / 64) & 1) == 0) col = gfx_mix(col, wash, 14);

    /* Soft distant hills */
    int hx = x - w / 2;
    int hill = h * 74 / 100 + (hx * hx) / 20000;
    if (y > hill) {
        int t = ((y - hill) * 256) / 100;
        if (t > 200) t = 200;
        col = gfx_mix(col, hill_color, t / 2 + 40);
    }

    int vx = (x - w / 2) * 256 / (w / 2 == 0 ? 1 : w / 2);
    int vy = (y - h / 2) * 256 / (h / 2 == 0 ? 1 : h / 2);
    int v = (vx * vx + vy * vy) / 800;
    if (v > 140) v = 140;
    col = gfx_mix(col, edge, v / 6);
    return col;
}

bool gfx_init(const boot_info_t *info)
{
    if (!info || info->magic != BOOT_MAGIC || !info->framebuffer) return false;
    s_lfb = (uint8_t *)info->framebuffer;
    s_w = info->width;
    s_h = info->height;
    s_pitch = info->pitch;
    s_bpp = info->bpp;
    /* VirtualBox often reports 24 bpp but uses a 32-bit padded scanline. */
    if (s_bpp == 24 && s_pitch >= s_w * 4) s_bpp = 32;
    s_wall = WALLPAPER_BASE;
    s_back = BACKBUFFER_BASE;
    return s_w > 0 && s_h > 0 && info->framebuffer != 0 &&
           (s_bpp == 16 || s_bpp == 24 || s_bpp == 32);
}

int gfx_width(void) { return s_w; }
int gfx_height(void) { return s_h; }
uint32_t *gfx_fb(void) { return (uint32_t *)s_lfb; }
uint32_t gfx_pitch_pixels(void) { return (uint32_t)(s_pitch / (s_bpp / 8)); }

void gfx_put(int x, int y, uint32_t color)
{
    if ((unsigned)x >= (unsigned)s_w || (unsigned)y >= (unsigned)s_h) return;
    s_back[y * s_w + x] = color;
}

uint32_t gfx_get(int x, int y)
{
    if ((unsigned)x >= (unsigned)s_w || (unsigned)y >= (unsigned)s_h) return 0;
    return s_back[y * s_w + x];
}

static void fill_row32(uint32_t *row, int count, uint32_t color)
{
    while (count >= 4) {
        row[0] = color; row[1] = color; row[2] = color; row[3] = color;
        row += 4;
        count -= 4;
    }
    while (count-- > 0) *row++ = color;
}

static void copy_row32(uint32_t *dest, const uint32_t *src, int count)
{
    while (count >= 4) {
        dest[0] = src[0]; dest[1] = src[1]; dest[2] = src[2]; dest[3] = src[3];
        dest += 4;
        src += 4;
        count -= 4;
    }
    while (count-- > 0) *dest++ = *src++;
}

static void clip_rect(int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > s_w) *w = s_w - *x;
    if (*y + *h > s_h) *h = s_h - *y;
}

static void lfb_put(int x, int y, uint32_t color)
{
    if ((unsigned)x >= (unsigned)s_w || (unsigned)y >= (unsigned)s_h) return;
    if (s_bpp == 32) {
        *(uint32_t *)(s_lfb + y * s_pitch + x * 4) = color;
    } else if (s_bpp == 16) {
        uint16_t p16 = (uint16_t)(((color >> 8) & 0xF800) | ((color >> 5) & 0x07E0) | ((color >> 3) & 0x001F));
        *(uint16_t *)(s_lfb + y * s_pitch + x * 2) = p16;
    } else {
        uint8_t *p = s_lfb + y * s_pitch + x * 3;
        p[0] = (uint8_t)(color);
        p[1] = (uint8_t)(color >> 8);
        p[2] = (uint8_t)(color >> 16);
    }
}

void gfx_fill(int x, int y, int w, int h, uint32_t color)
{
    clip_rect(&x, &y, &w, &h);
    if (w <= 0 || h <= 0) return;
    uint32_t *row = s_back + y * s_w + x;
    fill_row32(row, w, color);
    for (int yy = 1; yy < h; yy++)
        copy_row32(s_back + (y + yy) * s_w + x, row, w);
}

void gfx_rect(int x, int y, int w, int h, uint32_t color)
{
    gfx_hline(x, y, w, color);
    gfx_hline(x, y + h - 1, w, color);
    gfx_vline(x, y, h, color);
    gfx_vline(x + w - 1, y, h, color);
}

void gfx_hline(int x, int y, int w, uint32_t color)
{
    gfx_fill(x, y, w, 1, color);
}

void gfx_vline(int x, int y, int h, uint32_t color)
{
    if ((unsigned)x >= (unsigned)s_w) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > s_h) h = s_h - y;
    for (int i = 0; i < h; i++) s_back[(y + i) * s_w + x] = color;
}

void gfx_blend(int x, int y, int w, int h, uint32_t color, int alpha)
{
    clip_rect(&x, &y, &w, &h);
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; yy++) {
        uint32_t *row = s_back + (y + yy) * s_w + x;
        for (int xx = 0; xx < w; xx++) row[xx] = gfx_mix(row[xx], color, alpha);
    }
}

void gfx_char(int x, int y, char c, uint32_t color)
{
    unsigned idx = (unsigned char)c;
    if (idx < 32 || idx > 126) idx = '?';
    const uint8_t *g = FONT8[idx - 32];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1u << col)) gfx_put(x + col, y + row, color);
        }
    }
}

void gfx_text(int x, int y, const char *text, uint32_t color)
{
    if (!text) return;
    int cx = x;
    while (*text) {
        if (*text == '\n') { cx = x; y += 10; text++; continue; }
        gfx_char(cx, y, *text++, color);
        cx += 8;
    }
}

void gfx_text_scaled(int x, int y, const char *text, uint32_t color, int scale)
{
    if (scale <= 1) { gfx_text(x, y, text, color); return; }
    while (text && *text) {
        unsigned idx = (unsigned char)*text++;
        if (idx < 32 || idx > 126) idx = '?';
        const uint8_t *g = FONT8[idx - 32];
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (g[row] & (1u << col)) gfx_fill(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
        x += 8 * scale;
    }
}

int gfx_text_width(const char *text)
{
    return text ? (int)kstrlen(text) * 8 : 0;
}

void wallpaper_build(void)
{
    for (int y = 0; y < s_h; y++) {
        for (int x = 0; x < s_w; x++) s_wall[y * s_w + x] = wallpaper_pixel(x, y);
    }
}

void wallpaper_set_theme(int theme)
{
    if (theme < 0 || theme > 2) return;
    s_wallpaper_theme = theme;
    wallpaper_build();
}

int wallpaper_get_theme(void) { return s_wallpaper_theme; }

const char *wallpaper_theme_name(void)
{
    return s_wallpaper_theme == 1 ? "Ocean" : s_wallpaper_theme == 2 ? "Night" : "Daylight";
}

void wallpaper_blit_rect(int x, int y, int w, int h)
{
    clip_rect(&x, &y, &w, &h);
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; yy++)
        copy_row32(s_back + (y + yy) * s_w + x, s_wall + (y + yy) * s_w + x, w);
}

void wallpaper_full(void)
{
    wallpaper_blit_rect(0, 0, s_w, s_h);
}

void gfx_present_rect(int x, int y, int w, int h)
{
    clip_rect(&x, &y, &w, &h);
    if (w <= 0 || h <= 0) return;
    if (s_bpp == 32) {
        for (int yy = 0; yy < h; yy++) {
            copy_row32((uint32_t *)(s_lfb + (y + yy) * s_pitch + x * 4),
                       s_back + (y + yy) * s_w + x, w);
        }
        return;
    }
    for (int yy = 0; yy < h; yy++) {
        const uint32_t *src = s_back + (y + yy) * s_w + x;
        if (s_bpp == 16) {
            uint16_t *dest = (uint16_t *)(s_lfb + (y + yy) * s_pitch + x * 2);
            for (int xx = 0; xx < w; xx++) {
                uint32_t color = src[xx];
                dest[xx] = (uint16_t)(((color >> 8) & 0xF800) | ((color >> 5) & 0x07E0) | ((color >> 3) & 0x001F));
            }
        } else {
            uint8_t *dest = s_lfb + (y + yy) * s_pitch + x * 3;
            for (int xx = 0; xx < w; xx++) {
                uint32_t color = src[xx];
                dest[0] = (uint8_t)color;
                dest[1] = (uint8_t)(color >> 8);
                dest[2] = (uint8_t)(color >> 16);
                dest += 3;
            }
        }
    }
}

void gfx_present(void)
{
    gfx_present_rect(0, 0, s_w, s_h);
}

static const uint16_t CURSOR[16] = {
    0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xF800, 0xD800, 0x8C00, 0x0C00, 0x0600, 0x0600, 0x0300, 0x0300
};

static void cursor_stamp_lfb(int x, int y)
{
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 12; col++) {
            if (CURSOR[row] & (0x8000 >> col)) {
                lfb_put(x + col, y + row, 0xFFFFFF);
                if (col + 1 < 12 && (CURSOR[row] & (0x8000 >> (col + 1))) == 0)
                    lfb_put(x + col + 1, y + row, 0x1A2332);
            }
        }
    }
}

void cursor_restore(void)
{
    if (s_cx < 0) return;
    gfx_present_rect(s_cx, s_cy, s_cw, s_ch);
    s_cx = -1;
}

void cursor_discard(void)
{
    s_cx = -1;
}

void cursor_draw(int x, int y)
{
    int nw = 12, nh = 16;
    if (s_cx >= 0) {
        /* Only restore the pixels covered by the previous cursor.  Restoring
         * the union of the old and new positions makes a large framebuffer
         * copy when PS/2 packets jump, which can visibly wipe the desktop. */
        gfx_present_rect(s_cx, s_cy, s_cw, s_ch);
    }
    s_cx = x;
    s_cy = y;
    s_cw = nw;
    s_ch = nh;
    cursor_stamp_lfb(x, y);
}
