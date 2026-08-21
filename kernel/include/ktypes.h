#ifndef KTYPES_H
#define KTYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define KERNEL_VERSION "1.5.4"

typedef struct {
    uint32_t magic;
    uint32_t framebuffer;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint16_t bpp;
} boot_info_t;

#define BOOT_MAGIC 0x6D696E69u

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

static inline void cpu_halt(void)
{
    __asm__ volatile ("hlt");
}

static inline void irq_enable(void)
{
    __asm__ volatile ("sti");
}

static inline void irq_disable(void)
{
    __asm__ volatile ("cli");
}

#endif
