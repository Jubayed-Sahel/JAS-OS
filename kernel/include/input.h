#ifndef INPUT_H
#define INPUT_H

#include "ktypes.h"

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
void input_begin_frame(void);

#endif
