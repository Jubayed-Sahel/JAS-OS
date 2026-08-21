#include "hw.h"

#define IDT_ENTRIES 256
#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1
#define PIT_CMD 0x43
#define PIT_CH0 0x40
#define COM1 0x3F8

extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq12_stub(void);
extern void default_irq_stub(void);

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t s_idt[IDT_ENTRIES];
static volatile uint32_t s_ms;

static void idt_set(int vector, void (*handler)(void), uint8_t flags)
{
    uint32_t base = (uint32_t)handler;
    s_idt[vector].base_low = (uint16_t)(base & 0xFFFF);
    s_idt[vector].selector = 0x08;
    s_idt[vector].zero = 0;
    s_idt[vector].flags = flags;
    s_idt[vector].base_high = (uint16_t)(base >> 16);
}

void pic_remap(void)
{
    outb(PIC1, 0x11);
    io_wait();
    outb(PIC2, 0x11);
    io_wait();
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    outb(PIC1_DATA, 0xF8); /* unmask timer, keyboard, cascade */
    outb(PIC2_DATA, 0xEF); /* unmask PS/2 mouse (IRQ12) */
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8) outb(PIC2, 0x20);
    outb(PIC1, 0x20);
}

void idt_init(void)
{
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set(i, default_irq_stub, 0x8E);
    }
    idt_set(0x20, irq0_stub, 0x8E);
    idt_set(0x21, irq1_stub, 0x8E);
    idt_set(0x2C, irq12_stub, 0x8E);

    idt_ptr_t ptr;
    ptr.limit = sizeof(s_idt) - 1;
    ptr.base = (uint32_t)&s_idt;
    __asm__ volatile ("lidt %0" : : "m"(ptr));
}

void pit_init(uint32_t hz)
{
    if (hz == 0) hz = 1000;
    uint32_t divisor = 1193182u / hz;
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    s_ms = 0;
}

void timer_irq(void)
{
    s_ms++;
    pic_eoi(0);
}

uint32_t now_ms(void)
{
    return s_ms;
}

uint32_t uptime_seconds(void)
{
    return s_ms / 1000u;
}

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03); /* 38400 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_write(char c)
{
    if (c == '\n') serial_write('\r');
    int spins = 0;
    while ((inb(COM1 + 5) & 0x20) == 0 && spins++ < 100000) {}
    outb(COM1, (uint8_t)c);
}

void serial_write_str(const char *s)
{
    if (!s) return;
    while (*s) serial_write(*s++);
}

void reboot_system(void)
{
    irq_disable();
    outb(0x64, 0xFE);
    for (;;) cpu_halt();
}

void shutdown_system(void)
{
    irq_disable();
    serial_write_str("JAS OS shutting down...\n");
    /* Bochs / Oracle VirtualBox ACPI power-off */
    outw(0xB004, 0x2000);
    io_wait();
    /* QEMU */
    outw(0x604, 0x2000);
    io_wait();
    /* Older VirtualBox */
    outw(0x4004, 0x3400);
    for (;;) cpu_halt();
}
