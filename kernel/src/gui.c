#include "gui.h"
#include "commands.h"
#include "filesystem.h"
#include "gfx.h"
#include "hw.h"
#include "input.h"
#include "klib.h"
#include "scheduler.h"
#include "task.h"

#define MAX_WIN 8
#define MAX_HITS 180
#define TERM_LINES 72
#define TERM_COLS 86
#define TBAR 42
#define TITLE_H 32
#define SIDE_W 176
#define FS_ROW_H 26
#define STATUS_MS 500
#define SPLASH_MS 3000
#define HIT_DESKTOP (-1)
#define HIT_BAR (-2)
#define HIT_SIDE (-3)

enum { WIN_FS = 0, WIN_NOTE, WIN_TERM, WIN_TASKS, WIN_CALC,
       WIN_SETTINGS, WIN_CLOCK, WIN_LAB };

enum { BTN_NORMAL = 0, BTN_ACCENT, BTN_OK, BTN_DANGER };

typedef struct {
    bool open;
    int x, y, w, h;
    int kind;
    const char *title;
} window_t;

typedef struct {
    int x, y, w, h;
    int owner;
    char command[80];
} hit_t;

static window_t s_win[MAX_WIN];
static int s_z[MAX_WIN];
static int s_focus = -1;
static int s_drag = -1, s_dx, s_dy;
static hit_t s_hits[MAX_HITS];
static int s_nhits;
static int s_hit_owner = HIT_DESKTOP;
static char s_term[TERM_LINES][TERM_COLS + 1];
static int s_term_count, s_term_start;
static int s_term_scroll;
static char s_input[140];
static size_t s_inlen;
static bool s_term_focus;
static bool s_dirty = true;
static uint32_t s_last_status;
static int s_cur_x = -999, s_cur_y = -999;
static int s_flash_x, s_flash_y, s_flash_w, s_flash_h;
static uint32_t s_flash_until;

/* Boot splash */
static bool s_splash = true;
static uint32_t s_splash_start;
static bool s_splash_drawn;

/* Files */
static char s_fs_sel[MINIFS_PATH_LENGTH];
static bool s_fs_sel_dir;
static bool s_fs_confirm;
static char s_fs_status[72];

/* Notepad */
enum { NOTE_LIST = 0, NOTE_EDIT, NOTE_NAME };
static int s_note_mode = NOTE_LIST;
static bool s_note_focus;
static char s_note_buf[MINIFS_FILE_CAPACITY];
static size_t s_note_len;
static char s_note_name[MINIFS_PATH_LENGTH];
static size_t s_note_name_len;
static bool s_note_modified;
static bool s_note_confirm;
static char s_note_status[64];

/* Task Manager */
static int s_tm_sel = -1;
static char s_tm_status[64];

/* Calculator */
static bool s_calc_focus;
static char s_calc_disp[24];
static int32_t s_calc_acc;
static char s_calc_op; /* 0, '+', '-', '*', '/' */
static bool s_calc_fresh;
static bool s_calc_error;

/* Settings */
static char s_settings_status[64];

/* Clock / stopwatch */
static bool s_stopwatch_running;
static uint32_t s_stopwatch_started;
static uint32_t s_stopwatch_elapsed;

static void note_command(const char *cmd);
static void fs_command(const char *cmd);
static void note_open_path(const char *path);
static void calc_command(const char *cmd);
static void calc_key(char c);
static void tm_command(const char *cmd);
static void settings_command(const char *cmd);
static void clock_command(const char *cmd);
static void lab_command(const char *cmd);
static void term_command(const char *cmd);
static void open_window(int kind);

static const char *APP_LABEL[] = {
    "Files", "Notes", "Terminal", "Task Mgr", "Calculator",
    "Settings", "Clock", "OS Lab"
};
static const uint32_t APP_COL[] = {
    0x0EA5E9u, 0xDB2777u, 0x16A34Au, 0xEA580Cu, 0xCA8A04u,
    0x64748Bu, 0x7C3AEDu, 0x0891B2u
};

void terminal_clear(void)
{
    s_term_count = 0;
    s_term_start = 0;
    s_term_scroll = 0;
    kmemset(s_term, 0, sizeof(s_term));
    s_dirty = true;
}

void terminal_write(const char *text)
{
    if (!text) return;
    static char line[TERM_COLS + 1];
    static int col;
    while (*text) {
        if (*text == '\n' || col >= TERM_COLS) {
            line[col] = 0;
            int slot;
            if (s_term_count < TERM_LINES) {
                slot = (s_term_start + s_term_count) % TERM_LINES;
                s_term_count++;
            } else {
                slot = s_term_start;
                s_term_start = (s_term_start + 1) % TERM_LINES;
            }
            ksnprintf(s_term[slot], sizeof(s_term[slot]), "%s", line);
            col = 0;
            line[0] = 0;
            s_dirty = true;
            if (*text == '\n') { text++; continue; }
        }
        if (*text != '\r') line[col++] = *text;
        text++;
    }
}

static void mark_dirty(void) { s_dirty = true; }

static void flash_hit(const hit_t *h)
{
    s_flash_x = h->x;
    s_flash_y = h->y;
    s_flash_w = h->w;
    s_flash_h = h->h;
    s_flash_until = now_ms() + 160;
    s_dirty = true;
}

static void add_hit(int x, int y, int w, int h, const char *cmd)
{
    if (s_nhits >= MAX_HITS || w <= 0 || h <= 0) return;
    if (s_hit_owner >= 0) {
        window_t *win = &s_win[s_hit_owner];
        int x2 = x + w, y2 = y + h;
        if (x < win->x) x = win->x;
        if (y < win->y) y = win->y;
        if (x2 > win->x + win->w) x2 = win->x + win->w;
        if (y2 > win->y + win->h) y2 = win->y + win->h;
        w = x2 - x;
        h = y2 - y;
        if (w <= 2 || h <= 2) return;
    }
    s_hits[s_nhits].x = x;
    s_hits[s_nhits].y = y;
    s_hits[s_nhits].w = w;
    s_hits[s_nhits].h = h;
    s_hits[s_nhits].owner = s_hit_owner;
    ksnprintf(s_hits[s_nhits].command, sizeof(s_hits[s_nhits].command), "%s", cmd);
    s_nhits++;
}

static bool point_in(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static int tagged_id(const char *cmd, const char *tag)
{
    size_t n = kstrlen(tag);
    if (kstrncmp(cmd, tag, n) != 0) return -1;
    if (cmd[n] < '0' || cmd[n] > '9') return -1;
    return (int)kstrtoul(cmd + n, NULL, 10);
}

static void run_command(const char *cmd)
{
    if (!cmd || !cmd[0]) return;
    if (kstrncmp(cmd, "__note", 6) == 0) { note_command(cmd); mark_dirty(); return; }
    if (kstrncmp(cmd, "__fs", 4) == 0) { fs_command(cmd); mark_dirty(); return; }
    if (kstrncmp(cmd, "__calc", 6) == 0) { calc_command(cmd); mark_dirty(); return; }
    if (kstrncmp(cmd, "__tm", 4) == 0) { tm_command(cmd); mark_dirty(); return; }
    if (kstrncmp(cmd, "__term", 6) == 0) { term_command(cmd); mark_dirty(); return; }
    if (kstrncmp(cmd, "__settings", 10) == 0) { settings_command(cmd); mark_dirty(); return; }
    if (kstrncmp(cmd, "__clock", 7) == 0) { clock_command(cmd); mark_dirty(); return; }
    if (kstrncmp(cmd, "__lab", 5) == 0) { lab_command(cmd); mark_dirty(); return; }
    if (kstrcmp(cmd, "shutdown") == 0 || kstrcmp(cmd, "poweroff") == 0) {
        kprintf("[OK] Shutting down...\n");
        shutdown_system();
        return;
    }
    if (kstrcmp(cmd, "reboot") == 0) {
        kprintf("[OK] Rebooting...\n");
        reboot_system();
        return;
    }
    s_term_scroll = 0;
    commands_execute(cmd);
    mark_dirty();
}

static void draw_btn(int x, int y, int w, int h, const char *label, int style, const char *cmd)
{
    uint32_t bg = COL_BTN, fg = COL_INK, bd = COL_BTN_BD;
    if (style == BTN_ACCENT) { bg = COL_TAB_ON; fg = COL_WHITE; bd = 0x115E59u; }
    else if (style == BTN_OK) { bg = COL_OK; fg = COL_WHITE; bd = 0x166534u; }
    else if (style == BTN_DANGER) { bg = COL_DANGER; fg = COL_WHITE; bd = 0x7F1D1Du; }
    gfx_fill(x, y, w, h, bg);
    gfx_hline(x + 1, y + 1, w - 2, gfx_mix(bg, COL_WHITE, 40));
    gfx_rect(x, y, w, h, bd);
    int tx = x + (w - gfx_text_width(label)) / 2;
    if (tx < x + 4) tx = x + 4;
    gfx_text(tx, y + (h - 8) / 2, label, fg);
    add_hit(x, y, w, h, cmd);
}

static void draw_tab(int x, int y, int w, int h, const char *label, bool on, const char *cmd)
{
    gfx_fill(x, y, w, h, on ? COL_TAB_ON : COL_TAB);
    gfx_rect(x, y, w, h, on ? 0x14B8A6u : 0x4B5A6Eu);
    int tx = x + (w - gfx_text_width(label)) / 2;
    if (tx < x + 2) tx = x + 2;
    gfx_text(tx, y + (h - 8) / 2, label, COL_TEXT);
    add_hit(x, y, w, h, cmd);
}

static void clamp_window(window_t *w)
{
    int sw = gfx_width();
    int sh = gfx_height() - TBAR;
    int min_x = SIDE_W + 8;
    if (w->w > sw - min_x - 8) w->w = sw - min_x - 8;
    if (w->h > sh - 8) w->h = sh - 8;
    if (w->w < 300) w->w = (sw - min_x - 8 < 300) ? sw - min_x - 8 : 300;
    if (w->h < 200) w->h = (sh - 8 < 200) ? sh - 8 : 200;
    if (w->x < min_x) w->x = min_x;
    if (w->y < 4) w->y = 4;
    if (w->x + w->w > sw - 2) w->x = sw - 2 - w->w;
    if (w->y + w->h > sh - 2) w->y = sh - 2 - w->h;
    if (w->x < min_x) w->x = min_x;
    if (w->y < 0) w->y = 0;
}

static void open_window(int kind)
{
    if (kind < 0 || kind >= MAX_WIN) return;
    window_t *w = &s_win[kind];
    w->open = true;
    w->kind = kind;
    w->title = APP_LABEL[kind];
    if (w->w == 0) {
        w->w = 680;
        w->h = 420;
        w->x = SIDE_W + 20 + kind * 16;
        w->y = 24 + kind * 14;
        if (kind == WIN_TERM) { w->w = 850; w->h = 560; }
        if (kind == WIN_FS) { w->w = 680; w->h = 450; }
        if (kind == WIN_NOTE) { w->w = 700; w->h = 450; }
        if (kind == WIN_TASKS) { w->w = 720; w->h = 400; }
        if (kind == WIN_CALC) { w->w = 320; w->h = 420; }
        if (kind == WIN_SETTINGS) { w->w = 620; w->h = 400; }
        if (kind == WIN_CLOCK) { w->w = 500; w->h = 350; }
        if (kind == WIN_LAB) { w->w = 720; w->h = 470; }
    }
    clamp_window(w);
    s_focus = kind;
    s_term_focus = (kind == WIN_TERM);
    s_note_focus = (kind == WIN_NOTE);
    s_calc_focus = (kind == WIN_CALC);
    int pos = -1;
    for (int i = 0; i < MAX_WIN; i++) if (s_z[i] == kind) pos = i;
    if (pos >= 0) {
        for (int i = pos; i < MAX_WIN - 1; i++) s_z[i] = s_z[i + 1];
        s_z[MAX_WIN - 1] = kind;
    }
}

static void close_window(int kind)
{
    s_win[kind].open = false;
    if (s_focus == kind) {
        s_focus = -1;
        s_term_focus = false;
        s_note_focus = false;
        s_calc_focus = false;
        for (int i = MAX_WIN - 1; i >= 0; i--) {
            if (s_win[s_z[i]].open) { s_focus = s_z[i]; break; }
        }
        s_term_focus = (s_focus == WIN_TERM);
        s_note_focus = (s_focus == WIN_NOTE);
        s_calc_focus = (s_focus == WIN_CALC);
    }
}

static void fill_roundish(int x, int y, int w, int h, uint32_t c)
{
    gfx_fill(x + 3, y, w - 6, h, c);
    gfx_fill(x, y + 3, w, h - 6, c);
    gfx_fill(x + 1, y + 1, w - 2, h - 2, c);
}

/* -------------------- Files -------------------- */

static void fs_full_path(const char *name, char out[MINIFS_PATH_LENGTH])
{
    const char *cwd = commands_cwd();
    if (kstrcmp(cwd, "/") == 0) ksnprintf(out, MINIFS_PATH_LENGTH, "/%s", name);
    else ksnprintf(out, MINIFS_PATH_LENGTH, "%s/%s", cwd, name);
}

static void fs_command(const char *cmd)
{
    if (kstrncmp(cmd, "__fs_sel ", 9) == 0) {
        const char *rest = cmd + 9;
        s_fs_sel_dir = (rest[0] == 'D');
        ksnprintf(s_fs_sel, sizeof(s_fs_sel), "%s", rest + 2);
        s_fs_confirm = false;
        ksnprintf(s_fs_status, sizeof(s_fs_status), "Selected %s", s_fs_sel);
        return;
    }
    if (kstrncmp(cmd, "__fs_open ", 10) == 0) {
        char path[MINIFS_PATH_LENGTH];
        fs_full_path(cmd + 10, path);
        note_open_path(path);
        open_window(WIN_NOTE);
        return;
    }
    if (kstrcmp(cmd, "__fs_del") == 0) {
        if (!s_fs_sel[0]) {
            ksnprintf(s_fs_status, sizeof(s_fs_status), "Select a file or folder first");
            return;
        }
        if (!s_fs_confirm) {
            s_fs_confirm = true;
            ksnprintf(s_fs_status, sizeof(s_fs_status), "Click Delete again to confirm");
            return;
        }
        s_fs_confirm = false;
        char path[MINIFS_PATH_LENGTH];
        fs_full_path(s_fs_sel, path);
        bool ok = s_fs_sel_dir ? minifs_rmdir(path) : minifs_delete(path);
        if (ok) {
            kprintf("[FILES] Deleted %s\n", path);
            ksnprintf(s_fs_status, sizeof(s_fs_status), "Deleted %s", s_fs_sel);
            s_fs_sel[0] = 0;
        } else {
            ksnprintf(s_fs_status, sizeof(s_fs_status),
                      s_fs_sel_dir ? "rmdir failed (not empty?)" : "Delete failed");
        }
        return;
    }
    if (kstrcmp(cmd, "__fs_newfile") == 0) {
        run_command("write new.txt Created from Files");
        ksnprintf(s_fs_status, sizeof(s_fs_status), "Created new.txt");
    }
}

static void draw_fs(window_t *w)
{
    int x = w->x + 16, y = w->y + TITLE_H + 12;
    int btn_y = w->y + w->h - 42;
    int list_bottom = btn_y - 34;
    char buf[96];
    ksnprintf(buf, sizeof(buf), "MiniFS   cwd %s", commands_cwd());
    gfx_text(x, y, buf, COL_ACCENT);
    y += 18;
    gfx_text(x, y, "TYPE  NAME                              SIZE", COL_INK2);
    y += 16;

    minifs_dirent_t ents[MINIFS_MAX_ENTRIES];
    size_t n = minifs_listdir(commands_cwd(), ents, MINIFS_MAX_ENTRIES);
    if (n == 0) {
        gfx_text(x, y, "(directory is empty)", COL_INK2);
        y += FS_ROW_H;
    }
    for (size_t i = 0; i < n; i++) {
        if (y + FS_ROW_H > list_bottom) break;
        int row_w = w->w - 32;
        bool sel = (s_fs_sel[0] && kstrcmp(s_fs_sel, ents[i].name) == 0);
        gfx_fill(x, y, row_w, FS_ROW_H - 2, sel ? 0xCCFBF1u : COL_WIN2);
        gfx_rect(x, y, row_w, FS_ROW_H - 2, sel ? COL_ACCENT : COL_BORDER);
        ksnprintf(buf, sizeof(buf), "%-4s  %-32s  %u B",
                  ents[i].is_dir ? "DIR" : "FILE", ents[i].name, ents[i].size);
        gfx_text(x + 8, y + 8, buf, ents[i].is_dir ? COL_AMBER : COL_INK);

        char cmd[72];
        if (ents[i].is_dir) {
            ksnprintf(cmd, sizeof(cmd), "cd %s", ents[i].name);
            add_hit(x, y, row_w - 70, FS_ROW_H - 2, cmd);
            ksnprintf(cmd, sizeof(cmd), "__fs_sel D %s", ents[i].name);
            add_hit(x + row_w - 68, y, 66, FS_ROW_H - 2, cmd);
            gfx_text(x + row_w - 58, y + 8, "sel", COL_INK2);
        } else {
            ksnprintf(cmd, sizeof(cmd), "__fs_sel F %s", ents[i].name);
            add_hit(x, y, row_w - 70, FS_ROW_H - 2, cmd);
            ksnprintf(cmd, sizeof(cmd), "__fs_open %s", ents[i].name);
            add_hit(x + row_w - 68, y, 66, FS_ROW_H - 2, cmd);
            gfx_text(x + row_w - 58, y + 8, "edit", COL_ACCENT);
        }
        y += FS_ROW_H;
    }

    unsigned used, files, dirs; uint32_t commits;
    minifs_get_info(&used, &files, &dirs, &commits);
    ksnprintf(buf, sizeof(buf), "Entries %u/16   files %u   dirs %u", used, files, dirs);
    gfx_text(x, list_bottom + 4, buf, COL_INK2);
    if (s_fs_status[0]) gfx_text(x + 220, list_bottom + 4, s_fs_status,
                                 s_fs_confirm ? COL_DANGER : COL_ACCENT);

    draw_btn(x, btn_y, 50, 26, "Up", BTN_NORMAL, "cd ..");
    draw_btn(x + 58, btn_y, 100, 26, "New folder", BTN_NORMAL, "mkdir notes");
    draw_btn(x + 166, btn_y, 100, 26, "New file", BTN_ACCENT, "__fs_newfile");
    draw_btn(x + 274, btn_y, 50, 26, "ls", BTN_NORMAL, "ls");
    draw_btn(x + 332, btn_y, s_fs_confirm ? 110 : 90, 26,
             s_fs_confirm ? "Sure delete?" : "Delete", BTN_DANGER, "__fs_del");
}

/* -------------------- Terminal -------------------- */

static void term_command(const char *cmd)
{
    if (kstrcmp(cmd, "__term_up") == 0) {
        s_term_scroll += 5;
        if (s_term_scroll > 240) s_term_scroll = 240;
    } else if (kstrcmp(cmd, "__term_down") == 0) {
        s_term_scroll -= 5;
        if (s_term_scroll < 0) s_term_scroll = 0;
    } else if (kstrcmp(cmd, "__term_help") == 0) {
        s_term_scroll = 0;
        commands_execute("help");
    } else if (kstrcmp(cmd, "__term_lectures") == 0) {
        s_term_scroll = 0;
        commands_execute("lectures");
    } else if (kstrcmp(cmd, "__term_teacher") == 0) {
        s_term_scroll = 0;
        commands_execute("teacher");
    } else if (kstrcmp(cmd, "__term_stop") == 0) {
        s_term_scroll = 0;
        commands_execute("stop");
    } else if (kstrcmp(cmd, "__term_clear") == 0) {
        terminal_clear();
    }
}

static void draw_term(window_t *w)
{
    int x = w->x + 8, y = w->y + TITLE_H + 6;
    int width = w->w - 16;
    int prompt_y = w->y + w->h - 50;
    int panel_h = prompt_y - y - 6;
    if (panel_h < 80) panel_h = 80;

    /* High-contrast terminal shell and command toolbar. */
    gfx_fill(x, y, width, panel_h, 0x020617u);
    gfx_rect(x, y, width, panel_h, s_term_focus ? 0x14B8A6u : 0x475569u);
    gfx_fill(x + 1, y + 1, width - 2, 30, 0x111C2Fu);
    gfx_hline(x + 1, y + 30, width - 2, 0x334155u);
    gfx_text(x + 10, y + 11, "JAS OS TERMINAL", 0xCCFBF1u);

    int right = x + width - 6;
    draw_btn(right - 48, y + 4, 48, 22, "Clear", BTN_NORMAL, "__term_clear");
    right -= 54;
    draw_btn(right - 44, y + 4, 44, 22, "STOP", BTN_DANGER, "__term_stop");
    right -= 50;
    draw_btn(right - 48, y + 4, 48, 22, "Guide", BTN_ACCENT, "__term_teacher");
    right -= 54;
    draw_btn(right - 68, y + 4, 68, 22, "Lectures", BTN_NORMAL, "__term_lectures");
    right -= 74;
    draw_btn(right - 44, y + 4, 44, 22, "Help", BTN_NORMAL, "__term_help");
    right -= 50;
    draw_btn(right - 26, y + 4, 26, 22, "Dn", BTN_NORMAL, "__term_down");
    right -= 32;
    draw_btn(right - 26, y + 4, 26, 22, "Up", BTN_NORMAL, "__term_up");

    int body_y = y + 34;
    int body_h = panel_h - 38;
    int vis = body_h / 13;
    if (vis < 1) vis = 1;
    int max_cols = (width - 42) / 8;
    if (max_cols < 20) max_cols = 20;
    if (max_cols > TERM_COLS) max_cols = TERM_COLS;

    /* Reflow stored console lines to the current window width. */
    int row_slot[256], row_offset[256], rows = 0;
    for (int logical = 0; logical < s_term_count && rows < 256; ++logical) {
        int slot = (s_term_start + logical) % TERM_LINES;
        int length = (int)kstrlen(s_term[slot]);
        if (length == 0) {
            row_slot[rows] = slot; row_offset[rows++] = 0;
            continue;
        }
        for (int offset = 0; offset < length && rows < 256; offset += max_cols) {
            row_slot[rows] = slot;
            row_offset[rows++] = offset;
        }
    }
    int max_scroll = rows > vis ? rows - vis : 0;
    if (s_term_scroll > max_scroll) s_term_scroll = max_scroll;
    int start = rows - vis - s_term_scroll;
    if (start < 0) start = 0;
    int shown = rows - start;
    if (shown > vis) shown = vis;
    for (int i = 0; i < shown; ++i) {
        const char *source = s_term[row_slot[start + i]];
        int offset = row_offset[start + i];
        char segment[TERM_COLS + 1];
        int copied = 0;
        while (source[offset + copied] && copied < max_cols) {
            segment[copied] = source[offset + copied];
            ++copied;
        }
        segment[copied] = 0;
        uint32_t col = 0xF1F5F9u;
        uint32_t bg = 0;
        if (source[0] == '+' || (source[0] == '|' && source[1] == ' ')) col = 0x67E8F9u;
        else if (kstrncmp(source, "  [OK]", 6) == 0) { col = 0x86EFACu; bg = 0x092A1Au; }
        else if (kstrncmp(source, "  [X]", 5) == 0) { col = 0xFCA5A5u; bg = 0x351014u; }
        else if (kstrncmp(source, "  [!]", 5) == 0) { col = 0xFDE68Au; bg = 0x33260Au; }
        else if (source[0] == '/' || kstrncmp(source, "JAS OS", 6) == 0 ||
                 kstrncmp(source, "[OS LAB]", 8) == 0) col = 0x5EEAD4u;
        if (bg) gfx_fill(x + 5, body_y + i * 13 - 1, width - 30, 12, bg);
        gfx_text(x + 10, body_y + i * 13 + 1, segment, col);
    }

    /* Visible scroll position. */
    int track_x = x + width - 15;
    gfx_fill(track_x, body_y, 6, body_h, 0x1E293Bu);
    int thumb_h = rows > 0 ? (body_h * vis) / rows : body_h;
    if (thumb_h < 18) thumb_h = 18;
    if (thumb_h > body_h) thumb_h = body_h;
    int thumb_y = body_y;
    if (max_scroll > 0)
        thumb_y += ((max_scroll - s_term_scroll) * (body_h - thumb_h)) / max_scroll;
    gfx_fill(track_x, thumb_y, 6, thumb_h, 0x2DD4BFu);

    /* Large, unmistakable command input area. */
    gfx_fill(w->x + 8, prompt_y, w->w - 16, 40, 0x0F1B2Du);
    gfx_rect(w->x + 8, prompt_y, w->w - 16, 40, s_term_focus ? 0x2DD4BFu : 0x64748Bu);
    gfx_text(w->x + 16, prompt_y + 5, "COMMAND", s_term_focus ? 0x5EEAD4u : 0x94A3B8u);
    char prompt[180];
    ksnprintf(prompt, sizeof(prompt), "%s $ %s", commands_cwd(), s_input);
    int prompt_cols = (w->w - 34) / 8;
    const char *visible_prompt = prompt;
    int prompt_length = (int)kstrlen(prompt);
    if (prompt_length > prompt_cols) visible_prompt = prompt + prompt_length - prompt_cols;
    gfx_text(w->x + 16, prompt_y + 21, visible_prompt, 0xF0FDFAu);
    if (s_term_focus && ((now_ms() / 500) & 1) == 0) {
        int cx = w->x + 16 + gfx_text_width(visible_prompt) + 1;
        if (cx < w->x + w->w - 18) gfx_fill(cx, prompt_y + 19, 8, 13, 0x5EEAD4u);
    }
}

/* -------------------- Notes -------------------- */

static void note_full_path(char out[MINIFS_PATH_LENGTH])
{
    const char *cwd = commands_cwd();
    if (kstrcmp(cwd, "/") == 0) ksnprintf(out, MINIFS_PATH_LENGTH, "/%s", s_note_name);
    else ksnprintf(out, MINIFS_PATH_LENGTH, "%s/%s", cwd, s_note_name);
}

static void note_open_path(const char *path)
{
    size_t sz = 0;
    const char *text = minifs_read(path, &sz);
    if (!text) {
        ksnprintf(s_note_status, sizeof(s_note_status), "Cannot open file");
        return;
    }
    if (sz >= MINIFS_FILE_CAPACITY) sz = MINIFS_FILE_CAPACITY - 1;
    kmemcpy(s_note_buf, text, sz);
    s_note_buf[sz] = 0;
    s_note_len = sz;
    const char *slash = kstrrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    ksnprintf(s_note_name, sizeof(s_note_name), "%s", name);
    s_note_name_len = kstrlen(s_note_name);
    s_note_mode = NOTE_EDIT;
    s_note_modified = false;
    s_note_confirm = false;
    ksnprintf(s_note_status, sizeof(s_note_status), "Opened %u B", (unsigned)sz);
}

static void note_command(const char *cmd)
{
    if (kstrcmp(cmd, "__note_new") == 0) {
        s_note_mode = NOTE_NAME;
        s_note_name[0] = 0;
        s_note_name_len = 0;
        s_note_buf[0] = 0;
        s_note_len = 0;
        s_note_modified = false;
        s_note_confirm = false;
        ksnprintf(s_note_status, sizeof(s_note_status), "Type a filename, press Enter");
    } else if (kstrcmp(cmd, "__note_list") == 0) {
        s_note_mode = NOTE_LIST;
        s_note_confirm = false;
        s_note_status[0] = 0;
    } else if (kstrcmp(cmd, "__note_save") == 0) {
        if (s_note_mode != NOTE_EDIT || !s_note_name[0]) return;
        char path[MINIFS_PATH_LENGTH];
        note_full_path(path);
        s_note_buf[s_note_len] = 0;
        if (minifs_write(path, s_note_buf)) {
            s_note_modified = false;
            ksnprintf(s_note_status, sizeof(s_note_status), "Saved %u B", (unsigned)s_note_len);
            kprintf("[NOTE] Saved %s (%u B)\n", path, (unsigned)s_note_len);
        } else ksnprintf(s_note_status, sizeof(s_note_status), "Save FAILED");
    } else if (kstrcmp(cmd, "__note_del") == 0) {
        if (s_note_mode != NOTE_EDIT || !s_note_name[0]) return;
        if (!s_note_confirm) {
            s_note_confirm = true;
            ksnprintf(s_note_status, sizeof(s_note_status), "Click Sure? to confirm delete");
            return;
        }
        s_note_confirm = false;
        char path[MINIFS_PATH_LENGTH];
        note_full_path(path);
        if (minifs_delete(path)) {
            kprintf("[NOTE] Deleted %s\n", path);
            s_note_mode = NOTE_LIST;
            s_note_name[0] = 0;
            s_note_name_len = 0;
            s_note_buf[0] = 0;
            s_note_len = 0;
            s_note_modified = false;
            ksnprintf(s_note_status, sizeof(s_note_status), "File deleted");
        } else ksnprintf(s_note_status, sizeof(s_note_status), "Delete failed");
    } else if (kstrncmp(cmd, "__note_open ", 12) == 0) {
        char path[MINIFS_PATH_LENGTH];
        fs_full_path(cmd + 12, path);
        note_open_path(path);
    }
}

static void note_key(char c)
{
    s_note_confirm = false;
    if (s_note_mode == NOTE_NAME) {
        if (c == '\n') {
            if (s_note_name_len > 0) {
                s_note_mode = NOTE_EDIT;
                s_note_modified = true;
                ksnprintf(s_note_status, sizeof(s_note_status), "New file - type text, click Save");
            }
        } else if (c == '\b') {
            if (s_note_name_len) s_note_name[--s_note_name_len] = 0;
        } else if (c >= 32 && c < 127 && c != '/' &&
                   s_note_name_len + 1 < sizeof(s_note_name)) {
            s_note_name[s_note_name_len++] = c;
            s_note_name[s_note_name_len] = 0;
        }
        return;
    }
    if (s_note_mode != NOTE_EDIT) return;
    if (c == '\b') {
        if (s_note_len) { s_note_buf[--s_note_len] = 0; s_note_modified = true; }
    } else if ((c == '\n' || (c >= 32 && c < 127)) && s_note_len + 1 < MINIFS_FILE_CAPACITY) {
        s_note_buf[s_note_len++] = c;
        s_note_buf[s_note_len] = 0;
        s_note_modified = true;
    }
}

static void note_draw_editor(int x, int y, int wpx, int hpx)
{
    gfx_fill(x, y, wpx, hpx, 0xFFFFFFu);
    gfx_rect(x, y, wpx, hpx, s_note_focus ? COL_ACCENT : COL_BORDER);
    int cols = (wpx - 16) / 8;
    int rows = (hpx - 12) / 12;
    if (cols < 8) cols = 8;
    if (cols > 148) cols = 148;
    if (rows < 1) rows = 1;
    static uint16_t starts[MINIFS_FILE_CAPACITY + 1];
    int nlines = 1;
    starts[0] = 0;
    int col = 0;
    for (size_t i = 0; i < s_note_len && nlines < (int)MINIFS_FILE_CAPACITY; i++) {
        if (s_note_buf[i] == '\n') { starts[nlines++] = (uint16_t)(i + 1); col = 0; }
        else if (++col >= cols) { starts[nlines++] = (uint16_t)(i + 1); col = 0; }
    }
    int first = nlines > rows ? nlines - rows : 0;
    char tmp[152];
    for (int li = first; li < nlines; li++) {
        size_t s = starts[li];
        size_t e = (li + 1 < nlines) ? starts[li + 1] : s_note_len;
        size_t m = 0;
        for (size_t i = s; i < e && m + 1 < sizeof(tmp); i++) {
            char c = s_note_buf[i];
            if (c != '\n' && c != '\r') tmp[m++] = c;
        }
        tmp[m] = 0;
        gfx_text(x + 8, y + 6 + (li - first) * 12, tmp, COL_INK);
    }
    if (s_note_focus && ((now_ms() / 500) & 1) == 0) {
        int crow = (nlines - 1) - first;
        int ccol = (int)(s_note_len - starts[nlines - 1]);
        if (crow >= 0 && ccol <= cols)
            gfx_fill(x + 8 + ccol * 8, y + 6 + crow * 12, 8, 10, COL_ACCENT);
    }
}

static void draw_note(window_t *w)
{
    int x = w->x + 12, y = w->y + TITLE_H + 8;
    draw_btn(x, y, 60, 26, "New", BTN_ACCENT, "__note_new");
    int bx = x + 68;
    if (s_note_mode == NOTE_EDIT) {
        draw_btn(bx, y, 60, 26, "Save", BTN_OK, "__note_save");
        bx += 68;
        draw_btn(bx, y, 84, 26, s_note_confirm ? "Sure?" : "Delete", BTN_DANGER, "__note_del");
        bx += 92;
        draw_btn(bx, y, 60, 26, "Files", BTN_NORMAL, "__note_list");
        bx += 68;
    }
    if (s_note_status[0]) gfx_text(bx + 6, y + 9, s_note_status, COL_ACCENT);

    int top = y + 34;
    int sy = w->y + w->h - 22;
    int area_h = sy - top - 8;
    char buf[96];

    if (s_note_mode == NOTE_LIST) {
        ksnprintf(buf, sizeof(buf), "Files in %s   (click one to open)", commands_cwd());
        gfx_text(x, top, buf, COL_INK2);
        int ly = top + 18;
        minifs_dirent_t ents[MINIFS_MAX_ENTRIES];
        size_t n = minifs_listdir(commands_cwd(), ents, MINIFS_MAX_ENTRIES);
        int shown = 0;
        for (size_t i = 0; i < n; i++) {
            if (ents[i].is_dir) continue;
            if (ly + FS_ROW_H > sy - 4) break;
            int row_w = w->w - 24;
            gfx_fill(x, ly, row_w, FS_ROW_H - 2, COL_WIN2);
            gfx_rect(x, ly, row_w, FS_ROW_H - 2, COL_BORDER);
            ksnprintf(buf, sizeof(buf), "%-36s  %u B", ents[i].name, ents[i].size);
            gfx_text(x + 8, ly + 7, buf, COL_INK);
            char cmd[64];
            ksnprintf(cmd, sizeof(cmd), "__note_open %s", ents[i].name);
            add_hit(x, ly, row_w, FS_ROW_H - 2, cmd);
            ly += FS_ROW_H;
            shown++;
        }
        if (!shown) gfx_text(x, ly + 6, "(no files - click New)", COL_INK2);
        gfx_text(x, sy, "Notes - MiniFS text editor", COL_INK2);
        return;
    }
    if (s_note_mode == NOTE_NAME) {
        gfx_text(x, top + 8, "New file name:", COL_INK2);
        int fx = x + 124;
        int fw = w->x + w->w - fx - 16;
        gfx_fill(fx, top, fw, 24, 0xFFFFFFu);
        gfx_rect(fx, top, fw, 24, COL_ACCENT);
        gfx_text(fx + 6, top + 8, s_note_name, COL_INK);
        if (s_note_focus && ((now_ms() / 500) & 1) == 0) {
            int cx = fx + 6 + (int)s_note_name_len * 8;
            if (cx < fx + fw - 10) gfx_fill(cx, top + 7, 8, 10, COL_ACCENT);
        }
        gfx_text(x, top + 40, "Type a name, press Enter, then type text.", COL_INK2);
        gfx_text(x, sy, "Notes - MiniFS text editor", COL_INK2);
        return;
    }
    note_draw_editor(x, top, w->w - 24, area_h);
    ksnprintf(buf, sizeof(buf), "%s %s   %u / %u B",
              s_note_name, s_note_modified ? "[MODIFIED]" : "[saved]",
              (unsigned)s_note_len, (unsigned)(MINIFS_FILE_CAPACITY - 1));
    gfx_text(x, sy, buf, s_note_modified ? COL_AMBER : COL_GREEN);
}

/* -------------------- Task Manager -------------------- */

static void tm_command(const char *cmd)
{
    if (kstrncmp(cmd, "__tm_sel ", 9) == 0) {
        s_tm_sel = (int)kstrtoul(cmd + 9, NULL, 10);
        ksnprintf(s_tm_status, sizeof(s_tm_status), "Selected task %d", s_tm_sel);
        return;
    }
    if (kstrcmp(cmd, "__tm_kill") == 0) {
        if (s_tm_sel < 0) {
            ksnprintf(s_tm_status, sizeof(s_tm_status), "Select a task first");
            return;
        }
        uint32_t shell = commands_shell_task_id();
        if ((uint32_t)s_tm_sel == shell) {
            ksnprintf(s_tm_status, sizeof(s_tm_status), "Cannot kill shell task");
            return;
        }
        kernel_task_t *t = task_get((uint32_t)s_tm_sel);
        if (t && kstrcmp(t->name, "shell") == 0) {
            ksnprintf(s_tm_status, sizeof(s_tm_status), "Cannot kill shell task");
            return;
        }
        if (task_kill((uint32_t)s_tm_sel)) {
            kprintf("[TASKS] Killed task %d\n", s_tm_sel);
            ksnprintf(s_tm_status, sizeof(s_tm_status), "Killed task %d", s_tm_sel);
            s_tm_sel = -1;
        } else {
            ksnprintf(s_tm_status, sizeof(s_tm_status), "Kill failed");
        }
    }
}

static void draw_tasks(window_t *w)
{
    int x = w->x + 14, y = w->y + TITLE_H + 12;
    gfx_text(x, y, "Live process table  (refreshes ~1 Hz)", COL_INK2);
    y += 18;
    gfx_text(x, y, "ID  NAME              STATE         PRI  BURST  SLICES", COL_INK2);
    y += 16;

    kernel_task_t *tasks = task_table();
    int btn_y = w->y + w->h - 42;
    for (size_t i = 0; i < KERNEL_MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) continue;
        if (y + 22 > btn_y - 8) break;
        bool sel = (s_tm_sel >= 0 && (uint32_t)s_tm_sel == tasks[i].id);
        int row_w = w->w - 28;
        gfx_fill(x, y, row_w, 20, sel ? 0xCCFBF1u : COL_WIN2);
        gfx_rect(x, y, row_w, 20, sel ? COL_ACCENT : COL_BORDER);
        char line[80];
        ksnprintf(line, sizeof(line), "%-3u %-17s %-12s  %-3u  %-5u  %u",
                  tasks[i].id, tasks[i].name, task_state_name(tasks[i].state),
                  tasks[i].priority, tasks[i].burst_estimate, tasks[i].run_count);
        uint32_t col = COL_INK;
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING) col = COL_GREEN;
        else if (tasks[i].state == TASK_SLEEPING || tasks[i].state == TASK_BLOCKED) col = COL_AMBER;
        else if (tasks[i].state == TASK_SUSPENDED || tasks[i].state == TASK_FINISHED) col = COL_DANGER;
        gfx_text(x + 6, y + 6, line, col);
        char cmd[24];
        ksnprintf(cmd, sizeof(cmd), "__tm_sel %u", tasks[i].id);
        add_hit(x, y, row_w, 20, cmd);
        y += 22;
    }

    char buf[72];
    ksnprintf(buf, sizeof(buf), "Active %u / %u    policy %s",
              (unsigned)task_active_count(), (unsigned)KERNEL_MAX_TASKS,
              scheduler_policy_name(scheduler_get_policy()));
    gfx_text(x, btn_y - 18, buf, COL_INK2);
    if (s_tm_status[0]) gfx_text(x + 280, btn_y - 18, s_tm_status, COL_ACCENT);

    draw_btn(x, btn_y, 110, 26, "Kill selected", BTN_DANGER, "__tm_kill");
    draw_btn(x + 120, btn_y, 100, 26, "Create task", BTN_ACCENT, "create counter");
    draw_btn(x + 230, btn_y, 70, 26, "ps", BTN_NORMAL, "tasks");
}

/* -------------------- Calculator -------------------- */

static void calc_reset(void)
{
    ksnprintf(s_calc_disp, sizeof(s_calc_disp), "0");
    s_calc_acc = 0;
    s_calc_op = 0;
    s_calc_fresh = true;
    s_calc_error = false;
}

static int32_t calc_parse_disp(void)
{
    const char *p = s_calc_disp;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    unsigned long v = kstrtoul(p, NULL, 10);
    int32_t n = (int32_t)v;
    return neg ? -n : n;
}

static void calc_set_disp(int32_t v)
{
    if (v < 0) {
        ksnprintf(s_calc_disp, sizeof(s_calc_disp), "-%d", (int)(-v));
    } else {
        ksnprintf(s_calc_disp, sizeof(s_calc_disp), "%d", (int)v);
    }
}

static void calc_apply(void)
{
    if (!s_calc_op) {
        s_calc_acc = calc_parse_disp();
        return;
    }
    int32_t b = calc_parse_disp();
    int32_t r = s_calc_acc;
    if (s_calc_op == '+') r = s_calc_acc + b;
    else if (s_calc_op == '-') r = s_calc_acc - b;
    else if (s_calc_op == '*') r = s_calc_acc * b;
    else if (s_calc_op == '/') {
        if (b == 0) {
            ksnprintf(s_calc_disp, sizeof(s_calc_disp), "Error");
            s_calc_error = true;
            s_calc_op = 0;
            s_calc_fresh = true;
            return;
        }
        r = s_calc_acc / b;
    }
    calc_set_disp(r);
    s_calc_acc = r;
    s_calc_op = 0;
    s_calc_fresh = true;
}

static void calc_digit(char d)
{
    if (s_calc_error) calc_reset();
    if (s_calc_fresh || kstrcmp(s_calc_disp, "0") == 0) {
        s_calc_disp[0] = d;
        s_calc_disp[1] = 0;
        s_calc_fresh = false;
    } else if (kstrlen(s_calc_disp) < 12) {
        size_t n = kstrlen(s_calc_disp);
        s_calc_disp[n] = d;
        s_calc_disp[n + 1] = 0;
    }
}

static void calc_op(char op)
{
    if (s_calc_error) calc_reset();
    if (!s_calc_fresh && s_calc_op) calc_apply();
    else s_calc_acc = calc_parse_disp();
    s_calc_op = op;
    s_calc_fresh = true;
}

static void calc_command(const char *cmd)
{
    if (kstrncmp(cmd, "__calc_", 7) != 0) return;
    const char *a = cmd + 7;
    if (kstrcmp(a, "C") == 0) { calc_reset(); return; }
    if (kstrcmp(a, "eq") == 0) { calc_apply(); return; }
    if (kstrcmp(a, "add") == 0) { calc_op('+'); return; }
    if (kstrcmp(a, "sub") == 0) { calc_op('-'); return; }
    if (kstrcmp(a, "mul") == 0) { calc_op('*'); return; }
    if (kstrcmp(a, "div") == 0) { calc_op('/'); return; }
    if (a[0] >= '0' && a[0] <= '9' && a[1] == 0) calc_digit(a[0]);
}

static void calc_key(char c)
{
    if (c >= '0' && c <= '9') calc_digit(c);
    else if (c == '+') calc_op('+');
    else if (c == '-') calc_op('-');
    else if (c == '*') calc_op('*');
    else if (c == '/') calc_op('/');
    else if (c == '=' || c == '\n') calc_apply();
    else if (c == 'c' || c == 'C' || c == '\b') {
        if (c == '\b' && !s_calc_fresh && kstrlen(s_calc_disp) > 1) {
            s_calc_disp[kstrlen(s_calc_disp) - 1] = 0;
        } else calc_reset();
    }
}

static void draw_calc(window_t *w)
{
    int x = w->x + 16, y = w->y + TITLE_H + 14;
    int dw = w->w - 32;
    gfx_fill(x, y, dw, 44, 0x0F172Au);
    gfx_rect(x, y, dw, 44, s_calc_focus ? COL_ACCENT : COL_BORDER);
    int tw = gfx_text_width(s_calc_disp);
    gfx_text(x + dw - tw - 12, y + 16, s_calc_disp, 0xF8FAFCu);
    if (s_calc_op) {
        char opstr[4] = { s_calc_op, 0, 0, 0 };
        gfx_text(x + 8, y + 16, opstr, 0x5EEAD4u);
    }

    y += 58;
    const char *keys[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    const char *cmds[] = {
        "__calc_7", "__calc_8", "__calc_9", "__calc_div",
        "__calc_4", "__calc_5", "__calc_6", "__calc_mul",
        "__calc_1", "__calc_2", "__calc_3", "__calc_sub",
        "__calc_C", "__calc_0", "__calc_eq", "__calc_add"
    };
    int bw = (dw - 18) / 4;
    int bh = 42;
    for (int i = 0; i < 16; i++) {
        int cx = x + (i % 4) * (bw + 6);
        int cy = y + (i / 4) * (bh + 8);
        int style = BTN_NORMAL;
        if (i == 12) style = BTN_DANGER;
        else if (i == 14) style = BTN_OK;
        else if ((i % 4) == 3) style = BTN_ACCENT;
        draw_btn(cx, cy, bw, bh, keys[i], style, cmds[i]);
    }
    gfx_text(x, w->y + w->h - 20, "Integer calculator  (keys when focused)", COL_INK2);
}

/* -------------------- Settings -------------------- */

static void settings_command(const char *cmd)
{
    if (kstrcmp(cmd, "__settings_day") == 0) wallpaper_set_theme(0);
    else if (kstrcmp(cmd, "__settings_ocean") == 0) wallpaper_set_theme(1);
    else if (kstrcmp(cmd, "__settings_night") == 0) wallpaper_set_theme(2);
    else if (kstrcmp(cmd, "__settings_rr") == 0) scheduler_set_policy(SCHEDULER_ROUND_ROBIN);
    else if (kstrcmp(cmd, "__settings_prio") == 0) scheduler_set_policy(SCHEDULER_PRIORITY);
    else if (kstrcmp(cmd, "__settings_fcfs") == 0) scheduler_set_policy(SCHEDULER_FCFS);
    else if (kstrcmp(cmd, "__settings_sjf") == 0) scheduler_set_policy(SCHEDULER_SJF);
    else if (kstrcmp(cmd, "__settings_clear") == 0) terminal_clear();
    else return;
    ksnprintf(s_settings_status, sizeof(s_settings_status), "Applied: %s / %s",
              wallpaper_theme_name(), scheduler_policy_name(scheduler_get_policy()));
}

static void draw_settings(window_t *w)
{
    int x = w->x + 18, y = w->y + TITLE_H + 16;
    gfx_text(x, y, "Appearance", COL_INK);
    gfx_text(x, y + 16, "Wallpaper changes are rendered by the kernel.", COL_INK2);
    y += 40;
    int bw = (w->w - 52) / 3;
    int theme = wallpaper_get_theme();
    draw_btn(x, y, bw, 34, "Daylight", theme == 0 ? BTN_ACCENT : BTN_NORMAL, "__settings_day");
    draw_btn(x + bw + 8, y, bw, 34, "Ocean", theme == 1 ? BTN_ACCENT : BTN_NORMAL, "__settings_ocean");
    draw_btn(x + (bw + 8) * 2, y, bw, 34, "Night", theme == 2 ? BTN_ACCENT : BTN_NORMAL, "__settings_night");

    y += 58;
    gfx_text(x, y, "CPU scheduling policy", COL_INK);
    gfx_text(x, y + 16, "This changes the live cooperative scheduler.", COL_INK2);
    y += 40;
    int sw = (w->w - 60) / 4;
    scheduler_policy_t policy = scheduler_get_policy();
    draw_btn(x, y, sw, 34, "Round Robin", policy == SCHEDULER_ROUND_ROBIN ? BTN_ACCENT : BTN_NORMAL, "__settings_rr");
    draw_btn(x + sw + 6, y, sw, 34, "Priority", policy == SCHEDULER_PRIORITY ? BTN_ACCENT : BTN_NORMAL, "__settings_prio");
    draw_btn(x + (sw + 6) * 2, y, sw, 34, "FCFS", policy == SCHEDULER_FCFS ? BTN_ACCENT : BTN_NORMAL, "__settings_fcfs");
    draw_btn(x + (sw + 6) * 3, y, sw, 34, "SJF", policy == SCHEDULER_SJF ? BTN_ACCENT : BTN_NORMAL, "__settings_sjf");

    y += 58;
    gfx_text(x, y, "Terminal", COL_INK);
    draw_btn(x, y + 22, 140, 32, "Clear terminal", BTN_NORMAL, "__settings_clear");
    if (s_settings_status[0]) gfx_text(x + 158, y + 34, s_settings_status, COL_ACCENT);
    gfx_text(x, w->y + w->h - 20, "Settings are session-local and reset when JAS OS reboots.", COL_INK2);
}

/* -------------------- Clock -------------------- */

static uint32_t stopwatch_value(void)
{
    return s_stopwatch_elapsed + (s_stopwatch_running ? now_ms() - s_stopwatch_started : 0U);
}

static void clock_command(const char *cmd)
{
    if (kstrcmp(cmd, "__clock_toggle") == 0) {
        if (s_stopwatch_running) {
            s_stopwatch_elapsed += now_ms() - s_stopwatch_started;
            s_stopwatch_running = false;
        } else {
            s_stopwatch_started = now_ms();
            s_stopwatch_running = true;
        }
    } else if (kstrcmp(cmd, "__clock_reset") == 0) {
        s_stopwatch_elapsed = 0;
        if (s_stopwatch_running) s_stopwatch_started = now_ms();
    }
}

static void format_time(uint32_t total_seconds, char *out, size_t capacity)
{
    uint32_t hours = total_seconds / 3600U;
    uint32_t minutes = (total_seconds / 60U) % 60U;
    uint32_t seconds = total_seconds % 60U;
    ksnprintf(out, capacity, "%02u:%02u:%02u", hours, minutes, seconds);
}

static void draw_clock(window_t *w)
{
    int x = w->x + 20, y = w->y + TITLE_H + 18;
    char value[32];
    format_time(uptime_seconds(), value, sizeof(value));
    gfx_text(x, y, "SYSTEM UPTIME", COL_INK2);
    gfx_fill(x, y + 18, w->w - 40, 58, 0x0F172Au);
    gfx_rect(x, y + 18, w->w - 40, 58, COL_ACCENT);
    int ux = x + (w->w - 40 - gfx_text_width(value) * 3) / 2;
    gfx_text_scaled(ux, y + 34, value, 0xA7F3D0u, 3);

    y += 98;
    gfx_text(x, y, "STOPWATCH", COL_INK2);
    format_time(stopwatch_value() / 1000U, value, sizeof(value));
    gfx_fill(x, y + 18, w->w - 40, 50, COL_WIN2);
    gfx_rect(x, y + 18, w->w - 40, 50, s_stopwatch_running ? COL_OK : COL_BORDER);
    int sx = x + (w->w - 40 - gfx_text_width(value) * 2) / 2;
    gfx_text_scaled(sx, y + 34, value, COL_INK, 2);

    y += 84;
    draw_btn(x, y, 130, 34, s_stopwatch_running ? "Pause" : "Start",
             s_stopwatch_running ? BTN_DANGER : BTN_OK, "__clock_toggle");
    draw_btn(x + 142, y, 110, 34, "Reset", BTN_NORMAL, "__clock_reset");
    gfx_text(x, w->y + w->h - 20, "Driven by the 1 kHz programmable interval timer.", COL_INK2);
}

/* -------------------- OS Lab -------------------- */

static void lab_command(const char *cmd)
{
    if (kstrcmp(cmd, "__lab_stop") == 0) {
        open_window(WIN_TERM);
        kprintf("\n[OS LAB] Stop all\n");
        commands_execute("lab stop");
        return;
    }
    int chapter = tagged_id(cmd, "__lab_");
    if (chapter < 1 || chapter > 13) return;
    open_window(WIN_TERM);
    char presentation[24];
    ksnprintf(presentation, sizeof(presentation), "present %d", chapter);
    kprintf("\n[OS LAB] Teacher presentation - Chapter %d\n", chapter);
    commands_execute(presentation);
}

static void draw_lab(window_t *w)
{
    int x = w->x + 16, y = w->y + TITLE_H + 12;
    gfx_text(x, y, "Click a lecture to run its main demonstration in Terminal.", COL_INK2);
    y += 22;
    static const char *labels[] = {
        "01  OS Overview", "02  Services + Boot", "03  Processes + IPC",
        "04  Threads", "05  CPU Scheduling", "06  Synchronization",
        "07  Deadlocks", "08  Main Memory", "09  Virtual Memory",
        "10  File Interface", "11  FS Implementation", "12  Mass Storage",
        "13  I/O Systems"
    };
    int gap = 10;
    int bw = (w->w - 42) / 2;
    int bh = 38;
    for (int i = 0; i < 13; ++i) {
        int col = i & 1;
        int row = i / 2;
        int bx = x + col * (bw + gap);
        int by = y + row * (bh + 8);
        char command[16];
        ksnprintf(command, sizeof(command), "__lab_%d", i + 1);
        draw_btn(bx, by, bw, bh, labels[i], i < 11 ? BTN_ACCENT : BTN_NORMAL, command);
    }
    draw_btn(x + bw + gap, y + 6 * (bh + 8), bw, bh,
             "STOP ALL ACTIVITY", BTN_DANGER, "__lab_stop");
    gfx_text(x, w->y + w->h - 20, "Chapters 1-11 core | 12-13 focused labs | 14+ excluded", COL_INK2);
}

static void draw_window(window_t *w)
{
    s_hit_owner = w->kind;
    fill_roundish(w->x + 4, w->y + 5, w->w, w->h, 0x64748Bu);
    fill_roundish(w->x, w->y, w->w, w->h, COL_WIN);
    gfx_fill(w->x + 3, w->y, w->w - 6, TITLE_H, COL_TITLE);
    gfx_fill(w->x, w->y + 3, w->w, TITLE_H - 3, COL_TITLE);
    gfx_fill(w->x, w->y + TITLE_H, w->w, 1, w->kind == s_focus ? COL_ACCENT : COL_BORDER);
    gfx_text_scaled(w->x + 12, w->y + 8, w->title, COL_INK, 2);
    gfx_fill(w->x + w->w - 28, w->y + 7, 18, 18, COL_DANGER);
    gfx_text(w->x + w->w - 23, w->y + 12, "x", COL_WHITE);
    char close_cmd[16];
    ksnprintf(close_cmd, sizeof(close_cmd), "__close_%d", w->kind);
    add_hit(w->x + w->w - 30, w->y + 5, 22, 22, close_cmd);
    gfx_rect(w->x + 1, w->y + 1, w->w - 2, w->h - 2,
             w->kind == s_focus ? COL_ACCENT : COL_BORDER);

    switch (w->kind) {
        case WIN_FS: draw_fs(w); break;
        case WIN_NOTE: draw_note(w); break;
        case WIN_TERM: draw_term(w); break;
        case WIN_TASKS: draw_tasks(w); break;
        case WIN_CALC: draw_calc(w); break;
        case WIN_SETTINGS: draw_settings(w); break;
        case WIN_CLOCK: draw_clock(w); break;
        case WIN_LAB: draw_lab(w); break;
    }
}

static void draw_sidebar(void)
{
    int sh = gfx_height() - TBAR;
    s_hit_owner = HIT_SIDE;
    gfx_fill(0, 0, SIDE_W, sh, COL_SIDE);
    gfx_fill(SIDE_W - 2, 0, 2, sh, 0x1E293Bu);
    gfx_text_scaled(16, 16, "JAS OS", 0x5EEAD4u, 2);
    gfx_text(16, 44, "CSE 323 x86", COL_MUTED);

    int row_h = 42;
    int top = 66;
    int avail = sh - top - 90;
    if (MAX_WIN * row_h > avail) row_h = avail / MAX_WIN;
    if (row_h < 30) row_h = 30;

    for (int i = 0; i < MAX_WIN; i++) {
        int iy = top + i * row_h;
        bool on = (s_win[i].open && s_focus == i);
        gfx_fill(10, iy, SIDE_W - 20, row_h - 6, on ? 0x3D4A5Cu : 0x343E4Cu);
        gfx_rect(10, iy, SIDE_W - 20, row_h - 6, on ? 0x14B8A6u : 0x475569u);
        int icon = row_h >= 38 ? 22 : 18;
        int icon_y = iy + (row_h - 6 - icon) / 2;
        gfx_fill(18, icon_y, icon, icon, APP_COL[i]);
        gfx_fill(22, icon_y + 4, icon - 8, icon - 8, gfx_mix(APP_COL[i], COL_WHITE, 40));
        gfx_text(50, iy + (row_h - 6 - 8) / 2, APP_LABEL[i], COL_TEXT);
        char command[16];
        ksnprintf(command, sizeof(command), "__side_%d", i);
        add_hit(10, iy, SIDE_W - 20, row_h - 6, command);
    }

    int py = top + MAX_WIN * row_h + 4;
    draw_btn(10, py, (SIDE_W - 26) / 2, 26, "Reboot", BTN_NORMAL, "reboot");
    draw_btn(10 + (SIDE_W - 26) / 2 + 6, py, (SIDE_W - 26) / 2, 26, "Power", BTN_DANGER, "shutdown");
    gfx_text(12, sh - 28, "1-8 apps | OS Lab", COL_MUTED);
}

static void draw_splash(uint32_t elapsed)
{
    int sw = gfx_width(), sh = gfx_height();
    gfx_fill(0, 0, sw, sh, 0x050505u);

    /* Mac-style centered Hello World with soft fade */
    int alpha = 255;
    if (elapsed < 500) alpha = (int)((elapsed * 255) / 500);
    else if (elapsed > SPLASH_MS - 600) {
        uint32_t left = SPLASH_MS - elapsed;
        alpha = (int)((left * 255) / 600);
    }
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    uint32_t ink = gfx_mix(0x050505u, 0xFFFFFFFFu, alpha);
    uint32_t sub = gfx_mix(0x050505u, 0xA3A3A3u, alpha);
    const char *msg = "Hello World";
    int scale = 4;
    int tw = gfx_text_width(msg) * scale;
    int tx = (sw - tw) / 2;
    int ty = sh / 2 - 36;
    if (tx < 8) tx = 8;
    gfx_text_scaled(tx, ty, msg, ink, scale);
    gfx_text((sw - gfx_text_width("JAS OS")) / 2, ty + 52, "JAS OS", sub);
    char edition[48];
    ksnprintf(edition, sizeof(edition), "v%s | CSE 323 chapters 1-13", KERNEL_VERSION);
    gfx_text((sw - gfx_text_width(edition)) / 2, ty + 70, edition, sub);
}

static void draw_desktop(void)
{
    s_nhits = 0;
    wallpaper_full();
    draw_sidebar();

    for (int i = 0; i < MAX_WIN; i++) {
        int kind = s_z[i];
        if (s_win[kind].open) draw_window(&s_win[kind]);
    }

    s_hit_owner = HIT_BAR;
    int sw = gfx_width();
    int tb = gfx_height() - TBAR;
    gfx_fill(0, tb, sw, TBAR, 0x1E293Bu);
    gfx_fill(0, tb, sw, 1, 0x475569u);
    gfx_text(14, tb + 16, "JAS OS", 0x5EEAD4u);

    int shut_x = sw - 100;
    int reboot_x = shut_x - 84;
    draw_tab(reboot_x, tb + 8, 76, 24, "Reboot", false, "reboot");
    draw_btn(shut_x, tb + 8, 88, 24, "Shutdown", BTN_DANGER, "shutdown");

    char clock[48];
    ksnprintf(clock, sizeof(clock), "uptime %us  %s", uptime_seconds(),
              scheduler_policy_name(scheduler_get_policy()));
    int clock_w = gfx_text_width(clock);
    int clock_x = reboot_x - 12 - clock_w;
    if (clock_x < SIDE_W + 10) clock_x = SIDE_W + 10;
    gfx_text(clock_x, tb + 16, clock, 0xCBD5E1u);

    int bx = SIDE_W + 12;
    int tab_limit = clock_x - 8;
    for (int i = 0; i < MAX_WIN; i++) {
        if (!s_win[i].open) continue;
        if (bx + 80 > tab_limit) break;
        char bc[16];
        ksnprintf(bc, sizeof(bc), "__bar_%d", i);
        draw_tab(bx, tb + 8, 76, 24, APP_LABEL[i], i == s_focus, bc);
        bx += 80;
    }

    if (s_flash_until && now_ms() < s_flash_until) {
        gfx_rect(s_flash_x - 2, s_flash_y - 2, s_flash_w + 4, s_flash_h + 4, COL_WHITE);
        gfx_rect(s_flash_x - 1, s_flash_y - 1, s_flash_w + 2, s_flash_h + 2, COL_ACCENT);
    }
}

static int window_at(int mx, int my)
{
    for (int i = MAX_WIN - 1; i >= 0; i--) {
        int k = s_z[i];
        window_t *w = &s_win[k];
        if (!w->open) continue;
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) return k;
    }
    return -1;
}

static void handle_click(int mx, int my)
{
    int top = window_at(mx, my);
    int tb = gfx_height() - TBAR;
    bool on_bar = my >= tb;
    bool on_side = mx < SIDE_W && !on_bar;

    for (int i = s_nhits - 1; i >= 0; i--) {
        hit_t *h = &s_hits[i];
        if (!point_in(mx, my, h->x, h->y, h->w, h->h)) continue;

        int close_id = tagged_id(h->command, "__close_");
        if (close_id >= 0) {
            if (h->owner >= 0 && top != h->owner) continue;
            close_window(close_id);
            mark_dirty();
            return;
        }
        int side_id = tagged_id(h->command, "__side_");
        if (side_id >= 0) {
            if (!on_side) continue;
            flash_hit(h);
            open_window(side_id);
            mark_dirty();
            return;
        }
        int bar_id = tagged_id(h->command, "__bar_");
        if (bar_id >= 0) {
            if (!on_bar) continue;
            flash_hit(h);
            open_window(bar_id);
            mark_dirty();
            return;
        }

        if (h->owner == HIT_BAR) {
            if (!on_bar) continue;
            flash_hit(h);
            run_command(h->command);
            return;
        }
        if (h->owner == HIT_SIDE) {
            if (!on_side) continue;
            flash_hit(h);
            run_command(h->command);
            return;
        }
        if (h->owner == HIT_DESKTOP) {
            if (top >= 0 || on_bar || on_side) continue;
            flash_hit(h);
            run_command(h->command);
            return;
        }
        if (h->owner >= 0) {
            if (top != h->owner) continue;
            flash_hit(h);
            open_window(top);
            run_command(h->command);
            return;
        }
    }

    if (on_bar || on_side) return;
    if (top >= 0) {
        open_window(top);
        window_t *w = &s_win[top];
        if (my >= w->y && my < w->y + TITLE_H && mx < w->x + w->w - 32) {
            s_drag = top;
            s_dx = mx - w->x;
            s_dy = my - w->y;
        }
        s_term_focus = (top == WIN_TERM);
        s_note_focus = (top == WIN_NOTE);
        s_calc_focus = (top == WIN_CALC);
        mark_dirty();
    }
}

static void handle_keys(void)
{
    if (!keyboard_has_char()) return;
    mark_dirty();
    while (keyboard_has_char()) {
        char c = keyboard_read_char();
        if (s_note_focus) { note_key(c); continue; }
        if (s_calc_focus) { calc_key(c); continue; }
        if (!s_term_focus) {
            if (c >= '1' && c <= '8') open_window(c - '1');
            else if (c == '\n') open_window(WIN_TERM);
            continue;
        }
        if (c == '\n') {
            s_input[s_inlen] = 0;
            s_term_scroll = 0;
            kprintf("%s $ %s\n", commands_cwd(), s_input);
            run_command(s_input);
            s_inlen = 0;
            s_input[0] = 0;
        } else if (c == '\b') {
            if (s_inlen) s_input[--s_inlen] = 0;
        } else if (s_inlen + 1 < sizeof(s_input)) {
            s_input[s_inlen++] = c;
            s_input[s_inlen] = 0;
        }
    }
}

void gui_init(void)
{
    for (int i = 0; i < MAX_WIN; i++) {
        s_win[i].open = false;
        s_win[i].kind = i;
        s_win[i].title = APP_LABEL[i];
        s_z[i] = i;
    }
    s_fs_sel[0] = 0;
    s_fs_status[0] = 0;
    s_tm_sel = -1;
    s_tm_status[0] = 0;
    calc_reset();
    terminal_clear();
    kprintf("JAS OS x86 v%s ready.\n", KERNEL_VERSION);
    kprintf("Sidebar: Files | Notes | Terminal | Tasks | Calc | Settings | Clock | OS Lab\n");
    kprintf("Teacher demo mode is ready - click Guide or type teacher\n");
    kprintf("Use present 1..13 to explain and run each lecture feature\n");

    s_splash = true;
    s_splash_start = now_ms();
    s_splash_drawn = false;
    s_dirty = true;
}

void gui_tick(void)
{
    uint32_t now = now_ms();
    input_begin_frame();

    if (s_splash) {
        uint32_t elapsed = now - s_splash_start;
        bool skip = (elapsed >= SPLASH_MS) || mouse_left_pressed() || keyboard_has_char();
        if (keyboard_has_char()) {
            while (keyboard_has_char()) (void)keyboard_read_char();
        }
        if (skip) {
            s_splash = false;
            open_window(WIN_TERM);
            s_dirty = true;
        } else {
            cursor_discard();
            draw_splash(elapsed);
            gfx_present();
            s_splash_drawn = true;
            return;
        }
    }

    handle_keys();

    int mx = mouse_x(), my = mouse_y();
    if (mouse_left_pressed()) {
        if (s_dirty) {
            cursor_discard();
            draw_desktop();
        }
        handle_click(mx, my);
    }
    if (s_drag >= 0 && mouse_left()) {
        window_t *w = &s_win[s_drag];
        int nx = mx - s_dx;
        int ny = my - s_dy;
        if (nx != w->x || ny != w->y) {
            w->x = nx;
            w->y = ny;
            clamp_window(w);
            s_dirty = true;
        }
    }
    if (mouse_left_released()) s_drag = -1;

    if (now - s_last_status >= STATUS_MS) {
        s_last_status = now;
        s_dirty = true;
    }
    if (s_flash_until && now >= s_flash_until) {
        s_flash_until = 0;
        s_dirty = true;
    }

    if (s_dirty) {
        cursor_discard();
        draw_desktop();
        gfx_present();
        cursor_draw(mx, my);
        s_dirty = false;
        s_cur_x = mx;
        s_cur_y = my;
    } else if (mx != s_cur_x || my != s_cur_y) {
        cursor_draw(mx, my);
        s_cur_x = mx;
        s_cur_y = my;
    }
}
