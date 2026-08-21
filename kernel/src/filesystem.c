#include "filesystem.h"
#include "event_log.h"
#include "klib.h"

#define MINIFS_MAGIC 0x4D465332UL
#define MINIFS_VERSION 2U
#define ENTRY_FILE 1U
#define ENTRY_DIRECTORY 2U

typedef struct {
    uint8_t used;
    uint8_t type;
    char path[MINIFS_PATH_LENGTH];
    uint32_t size;
    char data[MINIFS_FILE_CAPACITY + 1];
} minifs_entry_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t entry_count;
    uint32_t write_sequence;
    minifs_entry_t entries[MINIFS_MAX_ENTRIES];
} minifs_image_t;

static minifs_image_t s_image;

static minifs_entry_t *find_entry(const char *path)
{
    for (size_t i = 0; i < MINIFS_MAX_ENTRIES; ++i) {
        if (s_image.entries[i].used && kstrcmp(s_image.entries[i].path, path) == 0) {
            return &s_image.entries[i];
        }
    }
    return NULL;
}

static bool valid_path(const char *path)
{
    return path != NULL && path[0] == '/' && kstrlen(path) < MINIFS_PATH_LENGTH &&
           (path[1] == '\0' || path[kstrlen(path) - 1U] != '/');
}

static void parent_path(const char *path, char *parent, size_t capacity)
{
    ksnprintf(parent, capacity, "%s", path);
    char *slash = kstrrchr(parent, '/');
    if (slash == parent) parent[1] = '\0';
    else if (slash != NULL) *slash = '\0';
}

bool minifs_directory_exists(const char *path)
{
    if (path != NULL && kstrcmp(path, "/") == 0) return true;
    minifs_entry_t *entry = find_entry(path);
    return entry != NULL && entry->type == ENTRY_DIRECTORY;
}

static bool image_is_valid(void)
{
    if (s_image.magic != MINIFS_MAGIC || s_image.version != MINIFS_VERSION ||
        s_image.entry_count > MINIFS_MAX_ENTRIES) return false;
    size_t counted = 0;
    for (size_t i = 0; i < MINIFS_MAX_ENTRIES; ++i) {
        const minifs_entry_t *entry = &s_image.entries[i];
        if (entry->used > 1U) return false;
        if (!entry->used) continue;
        ++counted;
        if ((entry->type != ENTRY_FILE && entry->type != ENTRY_DIRECTORY) ||
            !valid_path(entry->path) || entry->size > MINIFS_FILE_CAPACITY ||
            entry->data[entry->size] != '\0') return false;
    }
    return counted == s_image.entry_count;
}

static bool commit_image(void)
{
    ++s_image.write_sequence;
    return true;
}

static minifs_entry_t *allocate_entry(void)
{
    for (size_t i = 0; i < MINIFS_MAX_ENTRIES; ++i) {
        if (!s_image.entries[i].used) {
            kmemset(&s_image.entries[i], 0, sizeof(s_image.entries[i]));
            s_image.entries[i].used = 1;
            ++s_image.entry_count;
            return &s_image.entries[i];
        }
    }
    return NULL;
}

bool minifs_format(void)
{
    kmemset(&s_image, 0, sizeof(s_image));
    s_image.magic = MINIFS_MAGIC;
    s_image.version = MINIFS_VERSION;
    event_log_add("FS", "MiniFS v2 formatted");
    return commit_image();
}

bool minifs_init(void)
{
    if (!image_is_valid()) return minifs_format();
    return true;
}

bool minifs_write(const char *path, const char *text)
{
    if (!valid_path(path) || kstrcmp(path, "/") == 0 || text == NULL ||
        kstrlen(text) > MINIFS_FILE_CAPACITY) return false;
    char parent[MINIFS_PATH_LENGTH];
    parent_path(path, parent, sizeof(parent));
    if (!minifs_directory_exists(parent)) return false;
    minifs_entry_t *entry = find_entry(path);
    if (entry != NULL && entry->type != ENTRY_FILE) return false;
    if (entry == NULL) entry = allocate_entry();
    if (entry == NULL) return false;
    entry->type = ENTRY_FILE;
    ksnprintf(entry->path, sizeof(entry->path), "%s", path);
    entry->size = (uint32_t)kstrlen(text);
    kmemcpy(entry->data, text, entry->size + 1U);
    event_log_add("FS", "write %s (%u B)", path, entry->size);
    return commit_image();
}

const char *minifs_read(const char *path, size_t *size)
{
    minifs_entry_t *entry = find_entry(path);
    if (entry == NULL || entry->type != ENTRY_FILE) return NULL;
    if (size != NULL) *size = entry->size;
    return entry->data;
}

bool minifs_delete(const char *path)
{
    minifs_entry_t *entry = find_entry(path);
    if (entry == NULL || entry->type != ENTRY_FILE) return false;
    kmemset(entry, 0, sizeof(*entry));
    --s_image.entry_count;
    event_log_add("FS", "deleted %s", path);
    return commit_image();
}

bool minifs_mkdir(const char *path)
{
    if (!valid_path(path) || kstrcmp(path, "/") == 0 || find_entry(path) != NULL) return false;
    char parent[MINIFS_PATH_LENGTH];
    parent_path(path, parent, sizeof(parent));
    if (!minifs_directory_exists(parent)) return false;
    minifs_entry_t *entry = allocate_entry();
    if (entry == NULL) return false;
    entry->type = ENTRY_DIRECTORY;
    ksnprintf(entry->path, sizeof(entry->path), "%s", path);
    event_log_add("FS", "mkdir %s", path);
    return commit_image();
}

bool minifs_rmdir(const char *path)
{
    minifs_entry_t *entry = find_entry(path);
    if (entry == NULL || entry->type != ENTRY_DIRECTORY) return false;
    const size_t length = kstrlen(path);
    for (size_t i = 0; i < MINIFS_MAX_ENTRIES; ++i) {
        if (s_image.entries[i].used && kstrncmp(s_image.entries[i].path, path, length) == 0 &&
            s_image.entries[i].path[length] == '/') return false;
    }
    kmemset(entry, 0, sizeof(*entry));
    --s_image.entry_count;
    event_log_add("FS", "rmdir %s", path);
    return commit_image();
}

size_t minifs_listdir(const char *directory, minifs_dirent_t *out, size_t capacity)
{
    size_t shown = 0;
    for (size_t i = 0; i < MINIFS_MAX_ENTRIES && shown < capacity; ++i) {
        const minifs_entry_t *entry = &s_image.entries[i];
        if (!entry->used) continue;
        char parent[MINIFS_PATH_LENGTH];
        parent_path(entry->path, parent, sizeof(parent));
        if (kstrcmp(parent, directory) != 0) continue;
        const char *name = kstrrchr(entry->path, '/');
        if (name) name++;
        else name = entry->path;
        out[shown].is_dir = entry->type == ENTRY_DIRECTORY;
        ksnprintf(out[shown].name, sizeof(out[shown].name), "%s", name);
        out[shown].size = entry->type == ENTRY_FILE ? entry->size : 0;
        shown++;
    }
    return shown;
}

void minifs_get_info(unsigned *used, unsigned *files, unsigned *dirs, uint32_t *commits)
{
    unsigned f = 0, d = 0;
    for (size_t i = 0; i < MINIFS_MAX_ENTRIES; ++i) {
        if (!s_image.entries[i].used) continue;
        if (s_image.entries[i].type == ENTRY_DIRECTORY) d++;
        else f++;
    }
    if (used) *used = s_image.entry_count;
    if (files) *files = f;
    if (dirs) *dirs = d;
    if (commits) *commits = s_image.write_sequence;
}
