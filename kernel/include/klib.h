#ifndef KLIB_H
#define KLIB_H

#include "ktypes.h"

void *kmemcpy(void *dest, const void *src, size_t n);
void *kmemset(void *dest, int value, size_t n);
void *kmemmove(void *dest, const void *src, size_t n);
int kmemcmp(const void *a, const void *b, size_t n);
size_t kstrlen(const char *s);
int kstrcmp(const char *a, const char *b);
int kstrncmp(const char *a, const char *b, size_t n);
char *kstrcpy(char *dest, const char *src);
char *kstrncpy(char *dest, const char *src, size_t n);
char *kstrcat(char *dest, const char *src);
char *kstrchr(const char *s, int c);
char *kstrrchr(const char *s, int c);
unsigned long kstrtoul(const char *s, const char **end, int base);
int ksnprintf(char *buffer, size_t size, const char *fmt, ...);
int kvsnprintf(char *buffer, size_t size, const char *fmt, va_list args);
void kprintf(const char *fmt, ...);

#define memcpy kmemcpy
#define memset kmemset
#define memmove kmemmove
#define memcmp kmemcmp
#define strlen kstrlen
#define strcmp kstrcmp
#define strncmp kstrncmp
#define strcpy kstrcpy
#define strncpy kstrncpy
#define strcat kstrcat
#define strchr kstrchr
#define strrchr kstrrchr
#define snprintf ksnprintf

#endif
