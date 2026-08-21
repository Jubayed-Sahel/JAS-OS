#include "input.h"
#include "hw.h"
#include "klib.h"

#define KBD_DATA 0x60
#define KBD_STATUS 0x64

static volatile char s_queue[32];
static volatile uint8_t s_qhead, s_qtail;
static volatile bool s_shift;
static volatile int s_mx = 80, s_my = 80;
static volatile bool s_left, s_right;
static volatile uint8_t s_press_latch;
static volatile uint8_t s_release_latch;
static bool s_left_prev;
static bool s_left_pressed, s_left_released;
static int s_max_x = 1024, s_max_y = 768;
static uint8_t s_mouse_cycle;
static uint8_t s_mouse_bytes[3];

static const char kbd_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0
};

static const char kbd_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0
};

static void kbd_wait_write(void)
{
    int spins = 0;
    while ((inb(KBD_STATUS) & 2) && spins++ < 100000) {}
}

static void kbd_wait_read(void)
{
    int spins = 0;
    while (((inb(KBD_STATUS) & 1) == 0) && spins++ < 100000) {}
}

static void kbd_write_cmd(uint8_t value)
{
    kbd_wait_write();
    outb(KBD_STATUS, value);
}

static void kbd_write_data(uint8_t value)
{
    kbd_wait_write();
    outb(KBD_DATA, value);
}

static uint8_t kbd_read_data(void)
{
    kbd_wait_read();
    return inb(KBD_DATA);
}

static void mouse_write(uint8_t value)
{
    kbd_write_cmd(0xD4);
    kbd_write_data(value);
    kbd_read_data(); /* ack */
}

void keyboard_init(void)
{
    s_qhead = s_qtail = 0;
    s_shift = false;
}

void mouse_init(void)
{
    kbd_write_cmd(0xA8);
    kbd_write_cmd(0x20);
    uint8_t status = kbd_read_data();
    status |= 0x02;
    status &= (uint8_t)~0x20;
    kbd_write_cmd(0x60);
    kbd_write_data(status);
    mouse_write(0xF6);
    mouse_write(0xF4);
    s_mouse_cycle = 0;
    s_mx = 120;
    s_my = 90;
}

void keyboard_irq(void)
{
    uint8_t sc = inb(KBD_DATA);
    if (sc == 0x2A || sc == 0x36) s_shift = true;
    else if (sc == 0xAA || sc == 0xB6) s_shift = false;
    else if ((sc & 0x80) == 0) {
        char c = s_shift ? kbd_shift[sc] : kbd_map[sc];
        if (c) {
            uint8_t next = (uint8_t)((s_qhead + 1u) % sizeof(s_queue));
            if (next != s_qtail) {
                s_queue[s_qhead] = c;
                s_qhead = next;
            }
        }
    }
    pic_eoi(1);
}

void mouse_irq(void)
{
    uint8_t data = inb(KBD_DATA);
    /* Byte 0 always has bit 3 set; drop desynced noise. */
    if (s_mouse_cycle == 0 && (data & 0x08) == 0) {
        pic_eoi(12);
        return;
    }
    s_mouse_bytes[s_mouse_cycle++] = data;
    if (s_mouse_cycle == 3) {
        s_mouse_cycle = 0;
        uint8_t flags = s_mouse_bytes[0];
        if ((flags & 0x08) == 0) {
            pic_eoi(12);
            return;
        }
        int dx = (int8_t)s_mouse_bytes[1];
        int dy = (int8_t)s_mouse_bytes[2];
        if (flags & 0x40) dx = 0;
        if (flags & 0x80) dy = 0;
        if (dx > 40) dx = 40;
        if (dx < -40) dx = -40;
        if (dy > 40) dy = 40;
        if (dy < -40) dy = -40;
        s_mx += dx;
        s_my -= dy;
        if (s_mx < 0) s_mx = 0;
        if (s_my < 0) s_my = 0;
        if (s_mx > s_max_x - 2) s_mx = s_max_x - 2;
        if (s_my > s_max_y - 2) s_my = s_max_y - 2;
        bool left = (flags & 1) != 0;
        if (left && !s_left) s_press_latch++;
        if (!left && s_left) s_release_latch++;
        s_left = left;
        s_right = (flags & 2) != 0;
    }
    pic_eoi(12);
}

bool keyboard_has_char(void)
{
    return s_qhead != s_qtail;
}

char keyboard_read_char(void)
{
    if (s_qhead == s_qtail) return 0;
    char c = s_queue[s_qtail];
    s_qtail = (uint8_t)((s_qtail + 1u) % sizeof(s_queue));
    return c;
}

int mouse_x(void) { return s_mx; }
int mouse_y(void) { return s_my; }
bool mouse_left(void) { return s_left; }
bool mouse_right(void) { return s_right; }
bool mouse_left_pressed(void) { return s_left_pressed; }
bool mouse_left_released(void) { return s_left_released; }

void mouse_set_bounds(int width, int height)
{
    s_max_x = width;
    s_max_y = height;
}

void input_begin_frame(void)
{
    irq_disable();
    uint8_t presses = s_press_latch;
    uint8_t releases = s_release_latch;
    s_press_latch = 0;
    s_release_latch = 0;
    bool left = s_left;
    irq_enable();
    s_left_pressed = presses > 0 || (left && !s_left_prev);
    s_left_released = releases > 0 || (!left && s_left_prev);
    s_left_prev = left;
}
