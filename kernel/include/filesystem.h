#ifndef MINI_KERNEL_FILESYSTEM_H
#define MINI_KERNEL_FILESYSTEM_H

#include "ktypes.h"

#define MINIFS_MAX_ENTRIES 16
#define MINIFS_PATH_LENGTH 64
#define MINIFS_FILE_CAPACITY 1024

typedef struct {
    bool is_dir;
    char name[MINIFS_PATH_LENGTH];
    uint32_t size;
} minifs_dirent_t;

bool minifs_init(void);
bool minifs_format(void);
bool minifs_write(const char *path, const char *text);
const char *minifs_read(const char *path, size_t *size);
bool minifs_delete(const char *path);
bool minifs_mkdir(const char *path);
bool minifs_rmdir(const char *path);
bool minifs_directory_exists(const char *path);
size_t minifs_listdir(const char *directory, minifs_dirent_t *out, size_t capacity);
void minifs_get_info(unsigned *used, unsigned *files, unsigned *dirs, uint32_t *commits);

#endif
