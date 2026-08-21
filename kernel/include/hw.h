#ifndef HW_H
#define HW_H

#include "ktypes.h"

void pic_remap(void);
void idt_init(void);
void pit_init(uint32_t hz);
void serial_init(void);
void serial_write(char c);
void serial_write_str(const char *s);
uint32_t now_ms(void);
uint32_t uptime_seconds(void);
void pic_eoi(uint8_t irq);
void reboot_system(void);
void shutdown_system(void);

void timer_irq(void);

#endif
