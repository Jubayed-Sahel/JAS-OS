#ifndef INPUT_H
#define INPUT_H

#include "ktypes.h"

/* Non-printing key events returned by keyboard_read_char(). */
#define KBD_KEY_CANCEL    0x03
#define KBD_KEY_CLEAR     0x0C
#define KBD_KEY_UP        0x11
#define KBD_KEY_DOWN      0x12
#define KBD_KEY_LEFT      0x13
#define KBD_KEY_RIGHT     0x14
#define KBD_KEY_HOME      0x15
#define KBD_KEY_END       0x16
#define KBD_KEY_PAGE_UP   0x17
#define KBD_KEY_PAGE_DOWN 0x18
#define KBD_KEY_DELETE    0x7F

void keyboard_init(void);
void mouse_init(void);
void keyboard_irq(void);
void mouse_irq(void);

bool keyboard_has_char(void);
char keyboard_read_char(void);

int mouse_x(void);
int mouse_y(void);
bool mouse_left(void);
bool mouse_right(void);
bool mouse_left_pressed(void);   /* edge: down this frame */
bool mouse_left_released(void);
void mouse_set_bounds(int width, int height);
void mouse_demo_target(int x, int y);
void input_begin_frame(void);

#endif
