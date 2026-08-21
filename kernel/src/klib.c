#include "klib.h"
#include "hw.h"

void *kmemcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void *kmemset(void *dest, int value, size_t n)
{
    uint8_t *d = dest;
    while (n--) *d++ = (uint8_t)value;
    return dest;
}

void *kmemmove(void *dest, const void *src, size_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int kmemcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *pa = a, *pb = b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++;
        pb++;
    }
    return 0;
}

size_t kstrlen(const char *s)
{
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

int kstrcmp(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *kstrcpy(char *dest, const char *src)
{
    char *out = dest;
    while ((*dest++ = *src++)) {}
    return out;
}

char *kstrncpy(char *dest, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = 0;
    if (n) dest[n - 1] = 0;
    return dest;
}

char *kstrcat(char *dest, const char *src)
{
    char *out = dest + kstrlen(dest);
    while ((*out++ = *src++)) {}
    return dest;
}

char *kstrchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return c == 0 ? (char *)s : NULL;
}

char *kstrrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (char *)last;
}

unsigned long kstrtoul(const char *s, const char **end, int base)
{
    while (*s == ' ') s++;
    unsigned long value = 0;
    if (base == 0) base = 10;
    while (*s) {
        int digit = -1;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        if (digit < 0 || digit >= base) break;
        value = value * (unsigned long)base + (unsigned long)digit;
        s++;
    }
    if (end) *end = s;
    return value;
}

static void put_char(char *buffer, size_t size, size_t *pos, char c)
{
    if (*pos + 1 < size) buffer[*pos] = c;
    (*pos)++;
}

static void put_str(char *buffer, size_t size, size_t *pos, const char *s)
{
    if (!s) s = "(null)";
    while (*s) put_char(buffer, size, pos, *s++);
}

static void put_uint(char *buffer, size_t size, size_t *pos, unsigned long value, int base, int width, bool zero, bool left, bool upper)
{
    char tmp[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    if (value == 0) tmp[n++] = '0';
    while (value) {
        tmp[n++] = digits[value % (unsigned long)base];
        value /= (unsigned long)base;
    }
    int pad = width - n;
    if (pad < 0) pad = 0;
    if (!left) {
        while (pad--) put_char(buffer, size, pos, zero ? '0' : ' ');
    }
    while (n--) put_char(buffer, size, pos, tmp[n]);
    if (left) {
        while (pad--) put_char(buffer, size, pos, ' ');
    }
}

int kvsnprintf(char *buffer, size_t size, const char *fmt, va_list args)
{
    size_t pos = 0;
    if (!fmt) fmt = "";
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            put_char(buffer, size, &pos, *fmt);
            continue;
        }
        fmt++;
        bool zero = false;
        bool left = false;
        int width = 0;
        if (*fmt == '-') {
            left = true;
            fmt++;
        }
        if (*fmt == '0') {
            zero = true;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        if (*fmt == 'l') fmt++;
        switch (*fmt) {
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                int len = (int)kstrlen(s);
                if (!left) {
                    while (width > len) { put_char(buffer, size, &pos, ' '); width--; }
                }
                put_str(buffer, size, &pos, s);
                if (left) {
                    while (width > len) { put_char(buffer, size, &pos, ' '); width--; }
                }
                break;
            }
            case 'c': put_char(buffer, size, &pos, (char)va_arg(args, int)); break;
            case 'd': {
                long v = va_arg(args, int);
                if (v < 0) {
                    put_char(buffer, size, &pos, '-');
                    put_uint(buffer, size, &pos, (unsigned long)(-v), 10, width, zero, left, false);
                } else {
                    put_uint(buffer, size, &pos, (unsigned long)v, 10, width, zero, left, false);
                }
                break;
            }
            case 'u': put_uint(buffer, size, &pos, va_arg(args, unsigned int), 10, width, zero, left, false); break;
            case 'x': put_uint(buffer, size, &pos, va_arg(args, unsigned int), 16, width, zero, left, false); break;
            case 'X': put_uint(buffer, size, &pos, va_arg(args, unsigned int), 16, width, zero, left, true); break;
            case 'p':
                put_str(buffer, size, &pos, "0x");
                put_uint(buffer, size, &pos, va_arg(args, unsigned int), 16, 8, true, false, false);
                break;
            case '%': put_char(buffer, size, &pos, '%'); break;
            default:
                put_char(buffer, size, &pos, '%');
                if (*fmt) put_char(buffer, size, &pos, *fmt);
                break;
        }
    }
    if (size) buffer[pos < size ? pos : size - 1] = 0;
    return (int)pos;
}

int ksnprintf(char *buffer, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = kvsnprintf(buffer, size, fmt, args);
    va_end(args);
    return n;
}

void kprintf(const char *fmt, ...)
{
    char line[256];
    va_list args;
    va_start(args, fmt);
    kvsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    serial_write_str(line);
    extern void terminal_write(const char *text);
    terminal_write(line);
}
