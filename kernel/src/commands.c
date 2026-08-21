/*
 * commands.c - JAS OS x86 command interpreter.
 *
 * Mirrors the ESP32-S3 CSE323 project's shell (Project/main/shell.c):
 * the same command set and the same evidence style (title rules,
 * [OK]/[!]/[X] prefixes, aligned tables) rendered as plain text.
 */
#include "commands.h"
#include "banker.h"
#include "demos.h"
#include "event_log.h"
#include "filesystem.h"
#include "hw.h"
#include "input.h"
#include "klib.h"
#include "lecture.h"
#include "memory.h"
#include "paging.h"
#include "scheduler.h"
#include "storage.h"
#include "task.h"

static char s_cwd[MINIFS_PATH_LENGTH] = "/";
static uint32_t s_shell_task_id;

static bool move_demo_pointer(const char *target)
{
    int x = 620, y = 300;
    if (kstrcmp(target, "files") == 0) { x = 88; y = 84; }
    else if (kstrcmp(target, "notes") == 0) { x = 88; y = 126; }
    else if (kstrcmp(target, "settings") == 0) { x = 88; y = 294; }
    else if (kstrcmp(target, "clock") == 0) { x = 88; y = 336; }
    else if (kstrcmp(target, "oslab") == 0) { x = 88; y = 378; }
    else if (kstrcmp(target, "guide") == 0) { x = 880; y = 105; }
    else if (kstrcmp(target, "stop") == 0) { x = 932; y = 105; }
    else if (kstrcmp(target, "terminal") != 0) return false;
    mouse_demo_target(x, y);
    kprintf("  [POINTER] Moving to %s.\n", target);
    return true;
}

/* Visible "syscall" demo slots (malloc/free evidence for the course concepts). */
static void *s_sys_ptr[4];
static size_t s_sys_sz[4];

void commands_init(void)
{
    ksnprintf(s_cwd, sizeof(s_cwd), "/");
}

const char *commands_cwd(void) { return s_cwd; }
uint32_t commands_shell_task_id(void) { return s_shell_task_id; }
void commands_set_shell_task_id(uint32_t id) { s_shell_task_id = id; }

/* ------------------------------------------------------------------ */
/* Evidence helpers (same structure as the 323 shell, no ANSI).       */

static void print_rule(void)
{
    kprintf("+----------------------------------------------------------+\n");
}

static void print_title(const char *icon, const char *title, const char *subtitle)
{
    kprintf("\n+----------------------------------------------------------+\n");
    kprintf("| %-2s %-53s |\n", icon, title);
    if (subtitle != NULL) {
        kprintf("| %-56s |\n", subtitle);
    }
    print_rule();
}

static void print_ok(const char *message)      { kprintf("  [OK] %s\n", message); }
static void print_warning(const char *message) { kprintf("  [!]  %s\n", message); }
static void print_error(const char *message)   { kprintf("  [X]  %s\n", message); }
static void print_memory_stats(void);

/* ------------------------------------------------------------------ */
/* Small parsing helpers.                                             */

static void skip_spaces(const char **p)
{
    while (**p == ' ') (*p)++;
}

static bool parse_uints(const char *text, unsigned *out, int count)
{
    for (int i = 0; i < count; i++) {
        skip_spaces(&text);
        if (*text < '0' || *text > '9') return false;
        out[i] = (unsigned)kstrtoul(text, &text, 10);
    }
    skip_spaces(&text);
    return *text == 0;
}

static bool resolve_path(const char *input, char output[MINIFS_PATH_LENGTH])
{
    if (input == NULL || input[0] == '\0') input = s_cwd;
    char combined[MINIFS_PATH_LENGTH * 2];
    if (input[0] == '/') ksnprintf(combined, sizeof(combined), "%s", input);
    else if (kstrcmp(s_cwd, "/") == 0) ksnprintf(combined, sizeof(combined), "/%s", input);
    else ksnprintf(combined, sizeof(combined), "%s/%s", s_cwd, input);

    char normalized[MINIFS_PATH_LENGTH];
    kstrcpy(normalized, "/");
    const char *p = combined;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        char part[MINIFS_PATH_LENGTH];
        size_t n = 0;
        while (*p && *p != '/' && n + 1 < sizeof(part)) part[n++] = *p++;
        part[n] = 0;
        if (kstrcmp(part, ".") == 0 || part[0] == 0) continue;
        if (kstrcmp(part, "..") == 0) {
            if (kstrcmp(normalized, "/") != 0) {
                char *slash = kstrrchr(normalized, '/');
                if (slash == normalized) normalized[1] = 0;
                else if (slash) *slash = 0;
            }
            continue;
        }
        if (kstrlen(normalized) + kstrlen(part) + 2U > sizeof(normalized)) return false;
        if (kstrcmp(normalized, "/") != 0) kstrcat(normalized, "/");
        kstrcat(normalized, part);
    }
    ksnprintf(output, MINIFS_PATH_LENGTH, "%s", normalized);
    return true;
}

/* ------------------------------------------------------------------ */
/* Command centre.                                                    */

static void print_syscall_help(void)
{
    print_title("S", "SYSCALL ALIASES", "Lecture-friendly names that call real kernel modules");
    kprintf("  PROCESS / SCHEDULER\n");
    kprintf("  syscall create                 create counter task\n");
    kprintf("  syscall kill ID                terminate task\n");
    kprintf("  syscall schedule rr|prio|fcfs|sjf\n");
    kprintf("  syscall ps / syscall timeline  process table / CPU slices\n");
    kprintf("\n");
    kprintf("  MEMORY\n");
    kprintf("  syscall malloc N               allocate N bytes (slot 0..3)\n");
    kprintf("  syscall free SLOT              free a prior malloc slot\n");
    kprintf("  syscall mem                    show first-fit heap stats\n");
    kprintf("\n");
    kprintf("  DEADLOCK / SYNC / PAGING\n");
    kprintf("  syscall banker_request P A B C   banker request ...\n");
    kprintf("  syscall banker_release P A B C   banker release ...\n");
    kprintf("  syscall sync                   start producer-consumer\n");
    kprintf("  syscall page demo              FIFO demand-paging trace\n");
    kprintf("  syscall disk / syscall io      chapter 12/13 demos\n");
    kprintf("\n");
    kprintf("  FILE SYSTEM (MiniFS)\n");
    kprintf("  syscall open|read PATH         cat PATH\n");
    kprintf("  syscall write PATH TEXT        write PATH TEXT\n");
    kprintf("  syscall unlink PATH            rm PATH\n");
    kprintf("  syscall mkdir|chdir PATH       mkdir / cd\n");
    print_rule();
    kprintf("  Tip: short shell commands (rr, mem test, page demo...) still work.\n");
}

static void print_help(void)
{
    print_title("?", "COMMAND CENTER", "Run my CSE323 implementations and inspect their state");
    kprintf("  UNDERSTANDING  guide / explain CH / present CH\n");
    kprintf("                Example: present 5\n\n");
    kprintf("  DEMO POINTER   pointer guide|stop|files|notes|settings|clock|oslab|terminal\n\n");
    kprintf("  LECTURE MAP (chapters 1-13; chapter 14 onward excluded)\n");
    kprintf("  Overview      lectures / services / boot\n");
    kprintf("  Processes     tasks / create counter / ipc / thread demo\n");
    kprintf("  Scheduling    schedule rr|priority|fcfs|sjf / timeline\n");
    kprintf("                priority ID 0..9 / burst ID TICKS\n");
    kprintf("  Sync          start / hold / continue / demo stop / rw demo\n");
    kprintf("  Deadlocks     banker status|safe|request|release\n");
    kprintf("  Memory        mem / mem test / fit demo\n");
    kprintf("  Paging        page demo|status|test / replace demo\n");
    kprintf("  File system   ls mkdir cd write cat rm / fsalloc demo\n");
    kprintf("  Mass storage  disk demo|fcfs|sstf|scan|cscan|clook / raid\n");
    kprintf("  I/O systems   io demo|status|poll|interrupt|dma|modes\n");
    kprintf("  Syscalls      syscall ...   (type 'syscall' for aliases)\n");
    kprintf("\n");
    kprintf("  BASIC\n");
    kprintf("  status / dashboard / help / about / sysinfo / log / clear\n");
    kprintf("  calc A OP B / notes / note write|read|append|delete\n");
    kprintf("  stop / pause all / resume all / ticks / reboot / shutdown\n");
    kprintf("  rr / prio / fcfs / sjf\n");
    print_rule();
    kprintf("  Sidebar apps: Files, Notes, Terminal, Task Mgr, Calculator, Power.\n");
}

static bool execute_syscall(const char *args)
{
    skip_spaces(&args);
    if (*args == 0) {
        print_syscall_help();
        return true;
    }

    if (kstrcmp(args, "create") == 0) {
        commands_execute("create counter");
        return true;
    }
    if (kstrncmp(args, "kill ", 5) == 0) {
        char line[64];
        ksnprintf(line, sizeof(line), "kill %s", args + 5);
        commands_execute(line);
        return true;
    }
    if (kstrncmp(args, "schedule ", 9) == 0) {
        char line[80];
        ksnprintf(line, sizeof(line), "schedule %s", args + 9);
        commands_execute(line);
        return true;
    }
    if (kstrcmp(args, "ps") == 0 || kstrcmp(args, "tasks") == 0) {
        commands_execute("tasks");
        return true;
    }
    if (kstrcmp(args, "timeline") == 0) {
        commands_execute("timeline");
        return true;
    }
    if (kstrcmp(args, "mem") == 0) {
        commands_execute("mem");
        return true;
    }
    if (kstrncmp(args, "malloc ", 7) == 0) {
        unsigned n = (unsigned)kstrtoul(args + 7, NULL, 10);
        if (n == 0 || n > 8192) {
            print_warning("Usage: syscall malloc N   (1..8192 bytes)");
            return true;
        }
        int slot = -1;
        for (int i = 0; i < 4; i++) if (!s_sys_ptr[i]) { slot = i; break; }
        if (slot < 0) {
            print_error("All 4 syscall malloc slots are in use. Free one first.");
            return true;
        }
        void *p = kernel_malloc(n);
        if (!p) {
            print_error("kernel_malloc failed (heap exhausted).");
            return true;
        }
        s_sys_ptr[slot] = p;
        s_sys_sz[slot] = n;
        char msg[80];
        ksnprintf(msg, sizeof(msg), "malloc(%u) -> slot %d  ptr=%p", n, slot, p);
        print_ok(msg);
        print_memory_stats();
        return true;
    }
    if (kstrncmp(args, "free ", 5) == 0) {
        unsigned slot = (unsigned)kstrtoul(args + 5, NULL, 10);
        if (slot >= 4 || !s_sys_ptr[slot]) {
            print_warning("Usage: syscall free SLOT   (0..3, previously malloc'd)");
            return true;
        }
        if (kernel_free(s_sys_ptr[slot])) {
            char msg[64];
            ksnprintf(msg, sizeof(msg), "freed slot %u (%u B)", slot, (unsigned)s_sys_sz[slot]);
            print_ok(msg);
            s_sys_ptr[slot] = NULL;
            s_sys_sz[slot] = 0;
            print_memory_stats();
        } else print_error("kernel_free failed.");
        return true;
    }
    if (kstrncmp(args, "banker_request ", 15) == 0) {
        char line[80];
        ksnprintf(line, sizeof(line), "banker request %s", args + 15);
        commands_execute(line);
        return true;
    }
    if (kstrncmp(args, "banker_release ", 15) == 0) {
        char line[80];
        ksnprintf(line, sizeof(line), "banker release %s", args + 15);
        commands_execute(line);
        return true;
    }
    if (kstrcmp(args, "sync") == 0) {
        commands_execute("start");
        return true;
    }
    if (kstrcmp(args, "page") == 0 || kstrncmp(args, "page ", 5) == 0) {
        char line[80];
        if (kstrcmp(args, "page") == 0) ksnprintf(line, sizeof(line), "page demo");
        else ksnprintf(line, sizeof(line), "%s", args);
        commands_execute(line);
        return true;
    }
    if (kstrcmp(args, "disk") == 0) {
        commands_execute("disk demo");
        return true;
    }
    if (kstrcmp(args, "io") == 0) {
        commands_execute("io demo");
        return true;
    }
    if (kstrncmp(args, "open ", 5) == 0 || kstrncmp(args, "read ", 5) == 0) {
        char line[96];
        ksnprintf(line, sizeof(line), "cat %s", args + 5);
        commands_execute(line);
        return true;
    }
    if (kstrncmp(args, "write ", 6) == 0) {
        char line[160];
        ksnprintf(line, sizeof(line), "write %s", args + 6);
        commands_execute(line);
        return true;
    }
    if (kstrncmp(args, "unlink ", 7) == 0) {
        char line[96];
        ksnprintf(line, sizeof(line), "rm %s", args + 7);
        commands_execute(line);
        return true;
    }
    if (kstrncmp(args, "mkdir ", 6) == 0) {
        char line[96];
        ksnprintf(line, sizeof(line), "mkdir %s", args + 6);
        commands_execute(line);
        return true;
    }
    if (kstrncmp(args, "chdir ", 6) == 0) {
        char line[96];
        ksnprintf(line, sizeof(line), "cd %s", args + 6);
        commands_execute(line);
        return true;
    }

    print_warning("Unknown syscall. Type 'syscall' for the alias list.");
    return true;
}

static void print_about(void)
{
    print_title("*", "JAS OS - X86 COURSE CONCEPT KERNEL", "My implementation of CSE323 concepts on bare-metal i686");
    kprintf("\n  PROJECT MOTIVE\n");
    kprintf("  Build and test the lecture concepts myself so I can prove that I\n");
    kprintf("  understand how the algorithms change real kernel state.\n");
    kprintf("\n  WHAT I BUILT\n");
    kprintf("  A visible OS implementation where every scheduling decision,\n");
    kprintf("  task state, allocation, resource request, and file is inspectable.\n\n");
    kprintf("  CORE MODULES (CSE323 through chapter 13)\n");
    kprintf("  [01] Multi-policy scheduler  [04] Banker's deadlock avoidance\n");
    kprintf("  [02] Counting semaphores     [05] Hierarchical MiniFS\n");
    kprintf("  [03] First-fit allocator     [06] Desktop + Terminal + Notes\n");
    kprintf("  [07] Demand paging (FIFO)    [08] Visible syscall aliases\n");
    kprintf("  [09] CPU timeline + log      [10] Disk scheduling + RAID\n");
    kprintf("  [11] Poll/IRQ/DMA I/O model  [12] Calculator + Notes\n");
    kprintf("  [13] IPC mailbox + threads   [14] Readers-writers lab\n");
    kprintf("  [15] Page/fit/fs allocation comparison labs\n\n");
    kprintf("  SCOPE LIMIT: chapter 14 onward (protection and security).\n\n");
    kprintf("  PLATFORM  i686 (x86)   POLICY  Cooperative   VERSION  %s\n", KERNEL_VERSION);
    kprintf("\n  PRESENTED BY\n");
    kprintf("  Jubayed Ahmed Sahel  |  ID: 2221173642  |  CSE 323, Section 3\n");
    print_rule();
    kprintf("  Type 'help' or 'syscall' in Terminal for the lecture command map.\n");
}

static void print_dashboard(void)
{
    const kernel_heap_stats_t stats = memory_get_stats();
    const paging_stats_t page_stats = paging_get_stats();
    uint8_t sequence[BANKER_PROCESS_COUNT];
    const bool safe = banker_is_safe(sequence);
    const io_stats_t io = io_get_stats();
    const unsigned free_percent = stats.heap_size == 0 ? 0U :
        (unsigned)((stats.free_bytes * 100U) / stats.heap_size);

    print_title("#", "LIVE KERNEL DASHBOARD", "Understanding evidence - real-time subsystem state");
    kprintf("\n  SCHEDULER                 MEMORY\n");
    kprintf("  Active tasks   %-2u / %-2u    Heap free      %5u B\n",
            (unsigned)task_active_count(), (unsigned)KERNEL_MAX_TASKS, (unsigned)stats.free_bytes);
    kprintf("  Total slices   %-10u  Free capacity  %3u%%\n",
            scheduler_ticks(), free_percent);
    kprintf("  Policy         %-12s Largest block  %5u B\n",
            scheduler_policy_name(scheduler_get_policy()), (unsigned)stats.largest_free_block);
    kprintf("  Global state   %s\n\n", scheduler_is_paused() ? "PAUSED" : "RUNNING");
    kprintf("  SUBSYSTEM HEALTH\n");
    kprintf("  [ONLINE] Task scheduler      [ONLINE] Custom allocator\n");
    kprintf("  [ONLINE] Counting semaphore  %s Banker's state\n",
            safe ? "[ SAFE ]" : "[UNSAFE]");
    kprintf("  [READY ] Terminal shell      [READY ] MiniFS interface\n\n");
    kprintf("  [READY ] Paging MMU simulator  Faults: %-4u  Hits: %-4u\n\n",
            page_stats.faults, page_stats.hits);
    kprintf("  [READY ] Disk scheduler        FCFS/SSTF/SCAN/C-SCAN/C-LOOK\n");
    kprintf("  [READY ] I/O subsystem model   Requests: %-4u  Bytes: %-6u\n\n",
            io.requests, io.bytes);
    print_rule();
    kprintf("  Quick demos: lectures | ipc demo | rw demo | replace demo\n");
}

static void print_system_info(void)
{
    extern int gfx_width(void);
    extern int gfx_height(void);
    print_title("I", "SYSTEM INFORMATION", "Hardware, firmware, and kernel configuration");
    kprintf("  [SYS] Chip: i686 (32-bit x86)\n");
    kprintf("  [SYS] Firmware: JAS OS %s\n", KERNEL_VERSION);
    kprintf("  [SYS] CPU cores: 1\n");
    kprintf("  [SYS] Display: %dx%d framebuffer\n", gfx_width(), gfx_height());
    kprintf("  [SYS] Uptime: %u seconds\n", uptime_seconds());
    kprintf("  [SYS] Scheduler: %s\n", scheduler_policy_name(scheduler_get_policy()));
    kprintf("  [SYS] Shell: graphical terminal + PS/2 keyboard\n");
    print_rule();
}

static void print_tasks(void)
{
    kernel_task_t *tasks = task_table();
    print_title("P", "PROCESS MONITOR", "Task state, priority, burst estimate, and CPU slices");
    kprintf("  ID  NAME              STATE         PRI  BURST  RUN SLICES\n");
    kprintf("  --  ----------------  ------------  ---  -----  ----------\n");
    for (size_t i = 0; i < KERNEL_MAX_TASKS; ++i) {
        if (tasks[i].state == TASK_UNUSED) continue;
        kprintf("  %-3u %-17s %-12s  %-3u  %-5u  %10u\n",
                tasks[i].id, tasks[i].name, task_state_name(tasks[i].state),
                tasks[i].priority, tasks[i].burst_estimate, tasks[i].run_count);
    }
    print_rule();
    kprintf("  Active tasks: %u / %u\n",
            (unsigned)task_active_count(), (unsigned)KERNEL_MAX_TASKS);
}

static void print_schedule_status(void)
{
    print_title("C", "CPU SCHEDULER", "Selectable cooperative scheduling policy");
    kprintf("  Policy        %s\n", scheduler_policy_name(scheduler_get_policy()));
    kprintf("  Global state  %s\n", scheduler_is_paused() ? "PAUSED" : "RUNNING");
    kprintf("  Policies      rr | priority | fcfs | sjf\n");
    kprintf("  Tuning        priority ID 0..9 | burst ID TICKS\n");
    print_rule();
}

static void print_timeline(void)
{
    uint32_t ids[SCHEDULER_TIMELINE_LENGTH];
    const size_t count = scheduler_get_timeline(ids, SCHEDULER_TIMELINE_LENGTH);
    kprintf("  CPU timeline (last %u slices): ", (unsigned)count);
    if (count == 0) { kprintf("(empty)\n"); return; }
    for (size_t i = 0; i < count; ++i) {
        kprintf("T%u%s", ids[i], i + 1U == count ? "\n" : "-");
    }
}

static void print_event_log(void)
{
    print_title("L", "KERNEL EVENT LOG", "Newest events first; rolling 32-entry history");
    const size_t count = event_log_count();
    for (size_t i = 0; i < count; ++i) {
        event_log_entry_t entry;
        if (event_log_get(i, &entry)) {
            kprintf("  #%03u  %-10s %s\n", entry.sequence, entry.category, entry.message);
        }
    }
    if (count == 0) kprintf("  (event log is empty)\n");
    print_rule();
}

static void print_memory_stats(void)
{
    const kernel_heap_stats_t stats = memory_get_stats();
    const unsigned used_percent = stats.heap_size == 0 ? 0U :
        (unsigned)((stats.allocated_bytes * 100U) / stats.heap_size);
    const unsigned filled = used_percent / 5U;

    print_title("M", "CUSTOM KERNEL HEAP", "32 KiB first-fit allocator with split + coalesce");
    kprintf("  Usage  [");
    for (unsigned i = 0; i < 20; ++i) kprintf("%s", i < filled ? "#" : "-");
    kprintf("] %u%%\n\n", used_percent);
    kprintf("  Allocated  %6u B  in %-2u blocks\n",
            (unsigned)stats.allocated_bytes, (unsigned)stats.allocated_blocks);
    kprintf("  Free       %6u B  in %-2u blocks\n",
            (unsigned)stats.free_bytes, (unsigned)stats.free_blocks);
    kprintf("  Largest    %6u B  contiguous free block\n",
            (unsigned)stats.largest_free_block);
    print_rule();
}

/* ------------------------------------------------------------------ */
/* Paging.                                                            */

static void print_paging_result(const paging_result_t *result)
{
    kprintf("  P%u  virtual 0x%03X  page %-2u + offset %-3u  ->  frame %-2u / physical 0x%03X\n",
            result->process,
            (unsigned)(result->page * PAGING_PAGE_SIZE + result->offset),
            result->page, result->offset, result->frame, result->physical_address);
    if (result->page_fault) {
        kprintf("  [FAULT] Page was loaded into frame %u.\n", result->frame);
    } else {
        kprintf("  [HIT]   Page was already resident.\n");
    }
    if (result->evicted) {
        kprintf("  [FIFO]  Evicted P%u/page %u%s.\n",
                result->victim_process, result->victim_page,
                result->victim_dirty ? "; dirty page written back" : "");
    }
    if (result->write) kprintf("  [DIRTY] Frame %u modified by this write.\n", result->frame);
}

static void print_paging_status(void)
{
    const paging_stats_t stats = paging_get_stats();
    const unsigned hit_rate = stats.accesses == 0 ? 0U :
        (unsigned)((stats.hits * 100U) / stats.accesses);
    print_title("V", "VIRTUAL MEMORY PAGING", "3 processes, 8 pages each, 6 frames, FIFO replacement");
    kprintf("  FRAME  OWNER       DIRTY  LOADED  LAST USED\n");
    kprintf("  -----  ----------  -----  ------  ---------\n");
    for (uint8_t index = 0; index < PAGING_FRAME_COUNT; ++index) {
        const paging_frame_t frame = paging_get_frame(index);
        if (!frame.occupied) {
            kprintf("  %-6u FREE\n", index);
        } else {
            char owner[16];
            ksnprintf(owner, sizeof(owner), "P%u/page %u", frame.process, frame.page);
            kprintf("  %-6u %-11s %-6s %-7u %u\n", index, owner,
                    frame.dirty ? "YES" : "NO", frame.loaded_at, frame.last_used);
        }
    }
    kprintf("\n  Accesses %u  Hits %u  Faults %u  Hit rate %u%%\n",
            stats.accesses, stats.hits, stats.faults, hit_rate);
    kprintf("  Evictions %u  Dirty write-backs %u  Page size %u B\n",
            stats.evictions, stats.writebacks, (unsigned)PAGING_PAGE_SIZE);
    print_rule();
}

static void execute_paging_command(const char *line)
{
    if (kstrcmp(line, "page status") == 0) {
        print_paging_status();
        return;
    }
    if (kstrcmp(line, "page reset") == 0) {
        paging_init();
        print_ok("Page tables, frames, and paging statistics reset.");
        return;
    }
    if (kstrcmp(line, "page test") == 0) {
        if (paging_self_test())
            print_ok("Paging test passed: hit, fault, FIFO eviction, and dirty write-back.");
        else
            print_error("Paging self-test FAILED.");
        return;
    }
    if (kstrcmp(line, "page demo") == 0) {
        /* Same reference trace as the ESP32-S3 shell. */
        static const struct { uint8_t process; uint16_t address; bool write; } trace[] = {
            {0, 42, false}, {0, 300, true}, {1, 12, false}, {2, 700, false},
            {0, 42, false}, {1, 900, true}, {2, 1200, false}, {0, 1600, false},
            {1, 1300, false},
        };
        paging_init();
        print_title("V", "FIFO PAGE-REFERENCE TRACE", "R = read, W = write; faults load pages on demand");
        for (size_t index = 0; index < sizeof(trace) / sizeof(trace[0]); ++index) {
            paging_result_t result;
            paging_translate(trace[index].process, trace[index].address,
                             trace[index].write, &result);
            kprintf("  %u. %c\n", (unsigned)(index + 1), trace[index].write ? 'W' : 'R');
            print_paging_result(&result);
        }
        print_paging_status();
        return;
    }

    bool is_read = kstrncmp(line, "page read ", 10) == 0;
    bool is_write = kstrncmp(line, "page write ", 11) == 0;
    if (is_read || is_write) {
        unsigned args[2];
        if (parse_uints(line + (is_read ? 10 : 11), args, 2) &&
            args[0] < PAGING_PROCESS_COUNT &&
            args[1] < PAGING_VIRTUAL_PAGES * PAGING_PAGE_SIZE) {
            paging_result_t result;
            if (paging_translate((uint8_t)args[0], (uint16_t)args[1], is_write, &result)) {
                print_title("V", "ADDRESS TRANSLATION", "Virtual page + offset mapped through the page table");
                print_paging_result(&result);
                print_rule();
                return;
            }
        }
        print_error("Invalid process/address. Use P=0..2 and ADDRESS=0..2047.");
        return;
    }
    print_warning("Usage: page status | demo | test | reset | read P ADDRESS | write P ADDRESS");
}

/* ------------------------------------------------------------------ */
/* Banker's algorithm.                                                */

static void print_safe_sequence(void)
{
    uint8_t sequence[BANKER_PROCESS_COUNT];
    if (!banker_is_safe(sequence)) {
        print_error("UNSAFE state - no complete execution sequence exists.");
        return;
    }
    kprintf("  SAFE  Execution sequence: ");
    for (size_t i = 0; i < BANKER_PROCESS_COUNT; ++i) {
        kprintf("P%u%s", sequence[i], i + 1U == BANKER_PROCESS_COUNT ? "\n" : " -> ");
    }
}

static void print_banker_table(void)
{
    uint16_t available[BANKER_RESOURCE_COUNT];
    banker_get_available(available);
    kprintf("  PID |  ALLOCATION  |   MAXIMUM    |     NEED\n");
    kprintf("      |   A   B   C  |   A   B   C  |   A   B   C\n");
    kprintf("  ----+--------------+--------------+-------------\n");
    for (uint8_t p = 0; p < BANKER_PROCESS_COUNT; ++p) {
        uint16_t alloc[3], maximum[3], need[3];
        banker_get_allocation(p, alloc);
        banker_get_maximum(p, maximum);
        banker_get_need(p, need);
        kprintf("  P%u  |  %2u  %2u  %2u  |  %2u  %2u  %2u  |  %2u  %2u  %2u\n",
                p, alloc[0], alloc[1], alloc[2],
                maximum[0], maximum[1], maximum[2],
                need[0], need[1], need[2]);
    }
    kprintf("\n  Available   A=%u  B=%u  C=%u\n", available[0], available[1], available[2]);
}

static void execute_banker_command(const char *line)
{
    if (kstrcmp(line, "banker status") == 0) {
        print_title("B", "BANKER'S ALGORITHM", "Allocation, maximum claim, and remaining need");
        print_banker_table();
        print_safe_sequence();
        print_rule();
        return;
    }
    if (kstrcmp(line, "banker safe") == 0) {
        print_title("B", "SAFETY CHECK", "Searching for a complete process sequence");
        print_safe_sequence();
        print_rule();
        return;
    }

    bool is_request = kstrncmp(line, "banker request ", 15) == 0;
    bool is_release = kstrncmp(line, "banker release ", 15) == 0;
    if (is_request || is_release) {
        unsigned args[4];
        if (!parse_uints(line + 15, args, 4) || args[0] >= BANKER_PROCESS_COUNT) {
            print_warning("Usage: banker request P A B C | banker release P A B C");
            return;
        }
        const uint16_t vec[BANKER_RESOURCE_COUNT] =
            {(uint16_t)args[1], (uint16_t)args[2], (uint16_t)args[3]};
        if (is_request) {
            uint8_t sequence[BANKER_PROCESS_COUNT];
            if (banker_request((uint8_t)args[0], vec, sequence)) {
                print_ok("Resource request granted; the system remains safe.");
                print_safe_sequence();
            } else print_error("Request denied - invalid, unavailable, or unsafe.");
        } else {
            if (banker_release((uint8_t)args[0], vec)) print_ok("Resources released.");
            else print_error("Release denied - process does not hold those resources.");
        }
        return;
    }
    print_warning("Usage: banker status | safe | request P A B C | release P A B C");
}

/* ------------------------------------------------------------------ */
/* Producer-consumer demo status.                                     */

static void print_demo_status(void)
{
    demo_pc_status_t st;
    demo_get_pc_status(&st);
    print_title("S", "SYNCHRONIZATION MONITOR", "Bounded buffer protected by custom semaphores");
    kprintf("  Pipeline    %s\n",
            !st.started ? "STOPPED" : (st.paused ? "PAUSED" : "RUNNING"));
    kprintf("  Buffer      [");
    for (int i = 0; i < DEMO_BUFFER_SIZE; ++i) {
        if (i < (int)st.buffer_count) kprintf("%2d", st.buffer[i]);
        else kprintf(" -");
        kprintf(i + 1 == DEMO_BUFFER_SIZE ? " ]" : " |");
    }
    kprintf("  %u / %u items\n", (unsigned)st.buffer_count, DEMO_BUFFER_SIZE);
    kprintf("  Semaphores  empty=%d  full=%d  mutex=%d\n",
            st.empty_slots, st.full_slots, st.mutex);
    print_rule();
}

/* ------------------------------------------------------------------ */
/* Focused lecture 1-11 command labs.                                 */

static void print_lecture_map(void)
{
    print_title("L", "LECTURE IMPLEMENTATION MAP", "Main features from chapters 1-13");
    kprintf("  CH  MAIN IDEA                         LIVE COMMAND\n");
    kprintf("  --  --------------------------------  -------------------------\n");
    kprintf("  01  OS organization and resources    status / sysinfo\n");
    kprintf("  02  services, syscalls, boot          services / syscall / boot\n");
    kprintf("  03  processes and IPC                 tasks / ipc demo\n");
    kprintf("  04  shared-address-space workers      thread demo / thread status\n");
    kprintf("  05  CPU scheduling                    schedule ... / timeline\n");
    kprintf("  06  synchronization                   start / rw demo\n");
    kprintf("  07  deadlocks                         banker status / safe\n");
    kprintf("  08  main-memory allocation            mem test / fit demo\n");
    kprintf("  09  virtual memory                    page demo / replace demo\n");
    kprintf("  10  file-system interface             ls / write / cat / mkdir\n");
    kprintf("  11  file-system implementation        fsalloc demo / fsinfo\n");
    kprintf("  12  mass storage                      disk demo / raid\n");
    kprintf("  13  I/O systems                       io demo / io status\n");
    print_rule();
    kprintf("  Chapter 14 onward is intentionally outside project scope.\n");
}

static void print_services(void)
{
    print_title("O", "OPERATING-SYSTEM SERVICES", "Lectures 1-2 represented by this kernel");
    kprintf("  User interface       graphical desktop + command terminal\n");
    kprintf("  Program execution    task_create, scheduler, normal/forced exit\n");
    kprintf("  I/O operations       PS/2 IRQ input and framebuffer output\n");
    kprintf("  File manipulation    hierarchical MiniFS + Notes application\n");
    kprintf("  Communication        single-slot kernel mailbox (ipc command)\n");
    kprintf("  Error handling       validation, status results, event log\n");
    kprintf("  Resource allocation  scheduler, heap, frames, Banker, disk queue\n");
    kprintf("  Accounting           task slices, heap/page/I/O statistics\n");
    print_rule();
}

static void print_boot_path(void)
{
    print_title("B", "X86 BOOT PATH", "Lecture 2: system boot and kernel initialization");
    kprintf("  1. BIOS selects the El Torito boot image from the ISO\n");
    kprintf("  2. 16-bit loader reads kernel sectors into physical memory\n");
    kprintf("  3. VESA mode supplies a linear framebuffer\n");
    kprintf("  4. GDT + A20 enable 32-bit protected mode\n");
    kprintf("  5. kernel_main installs IDT/PIC/PIT and PS/2 drivers\n");
    kprintf("  6. scheduler, memory, paging, MiniFS, and desktop start\n");
    print_rule();
}

static void execute_ipc_command(const char *line)
{
    if (kstrcmp(line, "ipc") == 0 || kstrcmp(line, "ipc status") == 0) {
        print_title("I", "IPC MAILBOX", "Lecture 3: message passing between processes");
        kprintf("  State      %s\n", ipc_has_message() ? "FULL (sender would block)" : "EMPTY (receiver would block)");
        kprintf("  Sent       %u\n", ipc_sent_count());
        kprintf("  Received   %u\n", ipc_received_count());
        kprintf("  Commands   ipc send TEXT | ipc receive | ipc demo\n");
        print_rule();
        return;
    }
    if (kstrcmp(line, "ipc demo") == 0) {
        char message[96];
        if (ipc_has_message()) ipc_receive(message, sizeof(message));
        ipc_send("hello from producer process");
        print_title("I", "IPC MESSAGE-PASSING TRACE", "Bounded mailbox with capacity one");
        kprintf("  Sender   -> send('hello from producer process') -> mailbox FULL\n");
        if (!ipc_send("second message"))
            kprintf("  Sender   -> second send blocked/rejected while mailbox is full\n");
        ipc_receive(message, sizeof(message));
        kprintf("  Receiver <- receive() = '%s' -> mailbox EMPTY\n", message);
        kprintf("  This models explicit send/receive rather than shared memory.\n");
        print_rule();
        return;
    }
    if (kstrncmp(line, "ipc send ", 9) == 0) {
        if (ipc_send(line + 9)) print_ok("Message placed in the kernel mailbox.");
        else print_warning("Send failed: mailbox full, empty text, or text exceeds 95 bytes.");
        return;
    }
    if (kstrcmp(line, "ipc receive") == 0) {
        char message[96];
        if (ipc_receive(message, sizeof(message))) kprintf("  [RECV] %s\n", message);
        else print_warning("Receive would block: mailbox is empty.");
        return;
    }
    print_warning("Usage: ipc status | demo | send TEXT | receive");
}

static void execute_thread_command(const char *line)
{
    if (kstrcmp(line, "thread demo") == 0 || kstrcmp(line, "thread start") == 0) {
        if (demo_start_threads())
            print_ok("Two schedulable workers now share one counter protected by a semaphore.");
        else print_warning("Thread lab already running or the task table is full.");
    } else if (kstrcmp(line, "thread stop") == 0) {
        if (demo_stop_threads()) print_ok("Thread workers stopped.");
        else print_warning("Thread lab is not running.");
    } else if (kstrcmp(line, "thread") == 0 || kstrcmp(line, "thread status") == 0) {
        demo_thread_status_t status;
        demo_get_thread_status(&status);
        print_title("T", "THREAD LAB", "Lecture 4: independent execution sharing process resources");
        kprintf("  State          %s\n", status.started ? "RUNNING" : "STOPPED");
        kprintf("  Worker tasks   T%u and T%u\n", status.worker_a_id, status.worker_b_id);
        kprintf("  Shared counter %u\n", status.shared_counter);
        kprintf("  Protection     counting semaphore used as a mutex\n");
        kprintf("  Commands       thread demo | status | stop\n");
        print_rule();
    } else print_warning("Usage: thread demo | status | stop");
}

static void execute_rw_command(const char *line)
{
    if (kstrcmp(line, "rw demo") == 0 || kstrcmp(line, "rw start") == 0) {
        if (demo_start_readers_writers())
            print_ok("Two readers and one writer started; use 'rw status' or 'tasks'.");
        else print_warning("Readers-writers already running or the task table is full.");
    } else if (kstrcmp(line, "rw stop") == 0) {
        if (demo_stop_readers_writers()) print_ok("Readers-writers tasks stopped.");
        else print_warning("Readers-writers lab is not running.");
    } else if (kstrcmp(line, "rw") == 0 || kstrcmp(line, "rw status") == 0) {
        demo_rw_status_t status;
        demo_get_rw_status(&status);
        print_title("R", "READERS-WRITERS", "Lecture 6: concurrent reads, exclusive writes");
        kprintf("  State          %s\n", status.started ? "RUNNING" : "STOPPED");
        kprintf("  Tasks          readers T%u/T%u, writer T%u\n",
                status.reader_ids[0], status.reader_ids[1], status.writer_id);
        kprintf("  Active readers %u\n", status.active_readers);
        kprintf("  Shared value   %u\n", status.shared_value);
        kprintf("  Commands       rw demo | status | stop\n");
        print_rule();
    } else print_warning("Usage: rw demo | status | stop");
}

static void stop_all_labs(void)
{
    bool stopped = false;
    unsigned extra_tasks = 0;
    if (demo_stop_threads()) stopped = true;
    if (demo_stop_readers_writers()) stopped = true;
    if (demo_stop_producer_consumer()) stopped = true;

    /* Counter tasks and future background demos may not belong to a named lab.
       Stop every non-shell task so `stop` is dependable during assessment. */
    kernel_task_t *tasks = task_table();
    for (size_t i = 0; i < KERNEL_MAX_TASKS; ++i) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_FINISHED ||
            tasks[i].id == s_shell_task_id) continue;
        if (task_kill(tasks[i].id)) {
            ++extra_tasks;
            stopped = true;
        }
    }
    scheduler_resume_all();
    if (stopped) print_ok("STOP complete: all terminal background activity ended.");
    else print_ok("STOP complete: nothing was running.");
    kprintf("  Named labs stopped; extra background tasks terminated: %u\n", extra_tasks);
    kprintf("  Terminal shell remains READY and accepts commands.\n");
}

static void print_fit_demo(void)
{
    static const uint16_t holes[] = {100, 500, 200, 300, 600};
    const uint16_t request = 212;
    fit_result_t result = fit_compare(holes, sizeof(holes) / sizeof(holes[0]), request);
    print_title("M", "CONTIGUOUS-ALLOCATION FIT", "Lecture 8: selecting a free memory hole");
    kprintf("  Holes (KiB)  100, 500, 200, 300, 600\n");
    kprintf("  Request      %u KiB\n\n", request);
    kprintf("  First fit    hole %d (%u KiB) - first sufficient block\n",
            result.first_fit + 1, holes[result.first_fit]);
    kprintf("  Best fit     hole %d (%u KiB) - smallest sufficient block\n",
            result.best_fit + 1, holes[result.best_fit]);
    kprintf("  Worst fit    hole %d (%u KiB) - largest available block\n",
            result.worst_fit + 1, holes[result.worst_fit]);
    print_rule();
}

static void print_replacement_demo(void)
{
    static const uint8_t references[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2};
    replacement_result_t result = replacement_compare(
        references, sizeof(references) / sizeof(references[0]), 3);
    print_title("V", "PAGE-REPLACEMENT COMPARISON", "Lecture 9: same trace, three frames");
    kprintf("  Reference string  7 0 1 2 0 3 0 4 2 3 0 3 2\n");
    kprintf("  FIFO faults       %u   oldest loaded page is replaced\n", result.fifo_faults);
    kprintf("  LRU faults        %u   least recently used page is replaced\n", result.lru_faults);
    kprintf("  OPT faults        %u   farthest future use (theoretical minimum)\n", result.optimal_faults);
    print_rule();
}

static void print_fsalloc_demo(void)
{
    print_title("F", "FILE-ALLOCATION METHODS", "Lecture 11: mapping five logical blocks");
    kprintf("  Contiguous   start=40, length=5 -> 40 41 42 43 44\n");
    kprintf("  Linked       directory=9 -> 9 -> 16 -> 1 -> 10 -> 25\n");
    kprintf("  Indexed      index block 7 contains [9 16 1 10 25]\n\n");
    kprintf("  Contiguous: fast random access, external fragmentation.\n");
    kprintf("  Linked: no external fragmentation, weak random access.\n");
    kprintf("  Indexed: direct access with index-block overhead.\n");
    kprintf("  MiniFS uses a fixed entry table and per-file data capacity.\n");
    print_rule();
}

/* ------------------------------------------------------------------ */
/* Chapters 12-13: mass storage and kernel I/O.                       */

static bool disk_policy_from_name(const char *name, disk_policy_t *policy)
{
    if (kstrcmp(name, "fcfs") == 0) *policy = DISK_FCFS;
    else if (kstrcmp(name, "sstf") == 0) *policy = DISK_SSTF;
    else if (kstrcmp(name, "scan") == 0) *policy = DISK_SCAN;
    else if (kstrcmp(name, "cscan") == 0 || kstrcmp(name, "c-scan") == 0) *policy = DISK_CSCAN;
    else if (kstrcmp(name, "clook") == 0 || kstrcmp(name, "c-look") == 0) *policy = DISK_CLOOK;
    else return false;
    return true;
}

static void print_disk_result(disk_policy_t policy, uint16_t head,
                              const disk_result_t *result)
{
    kprintf("  %-6s  %3u", disk_policy_name(policy), head);
    for (size_t i = 0; i < result->count; ++i) kprintf(" -> %u", result->order[i]);
    kprintf("\n           Total head movement: %u cylinders\n", result->head_movement);
}

static void execute_disk_command(const char *line)
{
    static const uint16_t requests[] = {98, 183, 37, 122, 14, 124, 65, 67};
    const uint16_t head = 53;
    if (kstrcmp(line, "disk") == 0 || kstrcmp(line, "disk help") == 0) {
        print_title("D", "MASS-STORAGE LAB", "Chapter 12: disk scheduling and RAID");
        kprintf("  disk demo                 compare all scheduling policies\n");
        kprintf("  disk fcfs|sstf            run the classic queue\n");
        kprintf("  disk scan|cscan|clook     run upward from cylinder 53\n");
        kprintf("  disk <policy> down        choose the initial direction\n");
        kprintf("  raid                      compare common RAID levels\n");
        kprintf("\n  Queue: 98 183 37 122 14 124 65 67; cylinders 0..199\n");
        print_rule();
        return;
    }
    if (kstrcmp(line, "disk demo") == 0) {
        print_title("D", "DISK-SCHEDULING COMPARISON", "Classic lecture queue; head=53, direction=up");
        for (disk_policy_t policy = DISK_FCFS; policy <= DISK_CLOOK; ++policy) {
            disk_result_t result;
            disk_schedule(policy, requests, sizeof(requests) / sizeof(requests[0]),
                          head, true, &result);
            print_disk_result(policy, head, &result);
        }
        print_rule();
        kprintf("  SSTF minimizes nearby seeks but may starve distant requests.\n");
        kprintf("  SCAN is the elevator; circular policies make waits more uniform.\n");
        return;
    }

    const char *args = line + 5;
    while (*args == ' ') ++args;
    char name[12];
    size_t length = 0;
    while (args[length] && args[length] != ' ' && length + 1 < sizeof(name)) {
        name[length] = args[length];
        ++length;
    }
    name[length] = 0;
    disk_policy_t policy;
    if (!disk_policy_from_name(name, &policy)) {
        print_warning("Usage: disk demo | fcfs | sstf | scan | cscan | clook [up|down]");
        return;
    }
    const char *direction = args + length;
    while (*direction == ' ') ++direction;
    bool up = kstrcmp(direction, "down") != 0;
    if (*direction && kstrcmp(direction, "up") != 0 && kstrcmp(direction, "down") != 0) {
        print_warning("Direction must be 'up' or 'down'.");
        return;
    }
    disk_result_t result;
    if (!disk_schedule(policy, requests, sizeof(requests) / sizeof(requests[0]),
                       head, up, &result)) {
        print_error("Disk scheduler rejected the request queue.");
        return;
    }
    print_title("D", "DISK SCHEDULE", up ? "Initial direction: toward cylinder 199" :
                                         "Initial direction: toward cylinder 0");
    print_disk_result(policy, head, &result);
    print_rule();
}

static void print_raid(void)
{
    print_title("R", "RAID STRUCTURE", "Chapter 12: striping, mirroring, and parity");
    kprintf("  LEVEL  LAYOUT                    MIN DISKS  FAILURE TOLERANCE\n");
    kprintf("  -----  ------------------------  ---------  -----------------\n");
    kprintf("  RAID 0 Striping                  2          none\n");
    kprintf("  RAID 1 Mirroring                 2          one per mirror\n");
    kprintf("  RAID 5 Block striping + parity   3          one disk\n");
    kprintf("  RAID 6 Dual distributed parity   4          two disks\n");
    kprintf("  RAID10 Mirrors, then stripes     4          topology-dependent\n");
    print_rule();
    kprintf("  RAID improves availability/performance; it is not a backup.\n");
}

static void print_io_status(void)
{
    const io_stats_t stats = io_get_stats();
    print_title("I", "KERNEL I/O SUBSYSTEM", "Chapter 13: devices, queues, and transfer modes");
    kprintf("  DEVICE       CLASS       INTERFACE       EXAMPLE OPERATIONS\n");
    kprintf("  -----------  ----------  --------------  ------------------\n");
    kprintf("  PS/2 keys    character   IRQ 1           get / line input\n");
    kprintf("  PS/2 mouse   character   IRQ 12          packet input\n");
    kprintf("  framebuffer  mapped I/O  linear memory   put / blit\n");
    kprintf("  MiniFS       block model system calls    read / write / seek\n\n");
    kprintf("  Requests %-5u Bytes %-7u Polls %-6u IRQs %-6u DMA %-5u\n",
            stats.requests, stats.bytes, stats.polls, stats.interrupts, stats.dma_transfers);
    kprintf("  Kernel services: scheduling, buffering, caching, spooling, errors.\n");
    print_rule();
}

static void execute_io_command(const char *line)
{
    if (kstrcmp(line, "io") == 0 || kstrcmp(line, "io help") == 0) {
        print_title("I", "I/O SYSTEMS LAB", "Chapter 13 terminal demonstrations");
        kprintf("  io demo                 compare polling, IRQ, and DMA\n");
        kprintf("  io status               device table and counters\n");
        kprintf("  io poll|interrupt|dma N simulate an N-byte transfer\n");
        kprintf("  io modes                blocking/nonblocking/asynchronous\n");
        kprintf("  io reset                clear I/O counters\n");
        print_rule();
        return;
    }
    if (kstrcmp(line, "io status") == 0) { print_io_status(); return; }
    if (kstrcmp(line, "io reset") == 0) {
        io_reset_stats();
        print_ok("I/O counters reset.");
        return;
    }
    if (kstrcmp(line, "io modes") == 0) {
        print_title("I", "I/O COMPLETION MODES", "How a process observes device progress");
        kprintf("  Blocking     caller sleeps until the operation completes\n");
        kprintf("  Nonblocking  returns immediately with currently available data\n");
        kprintf("  Asynchronous caller continues; completion is signaled later\n");
        print_rule();
        return;
    }
    if (kstrcmp(line, "io demo") == 0) {
        io_simulate(IO_POLLING, 4096);
        io_simulate(IO_INTERRUPT, 4096);
        io_simulate(IO_DMA, 4096);
        print_title("I", "4096-BYTE I/O TRANSFER", "Same payload, different CPU involvement");
        kprintf("  POLLING    64 readiness checks; CPU busy-waits\n");
        kprintf("  INTERRUPT   8 device interrupts; CPU works between events\n");
        kprintf("  DMA         1 programmed transfer + 1 completion interrupt\n");
        kprintf("\n  DMA moves bulk data device <-> memory without per-byte CPU copies.\n");
        print_rule();
        return;
    }

    io_mode_t mode;
    const char *amount;
    if (kstrncmp(line, "io poll ", 8) == 0) { mode = IO_POLLING; amount = line + 8; }
    else if (kstrncmp(line, "io interrupt ", 13) == 0) { mode = IO_INTERRUPT; amount = line + 13; }
    else if (kstrncmp(line, "io dma ", 7) == 0) { mode = IO_DMA; amount = line + 7; }
    else {
        print_warning("Usage: io demo|status|modes|reset|poll N|interrupt N|dma N");
        return;
    }
    unsigned bytes = (unsigned)kstrtoul(amount, NULL, 10);
    if (bytes == 0 || bytes > 1048576U) {
        print_warning("Transfer size must be 1..1048576 bytes.");
        return;
    }
    io_simulate(mode, bytes);
    char message[80];
    ksnprintf(message, sizeof(message), "%u-byte %s transfer completed in the simulator.",
              bytes, mode == IO_POLLING ? "polling" : mode == IO_INTERRUPT ? "interrupt" : "DMA");
    print_ok(message);
    print_io_status();
}

/* ------------------------------------------------------------------ */
/* Small desktop utilities, also exposed in Terminal.                 */

static bool parse_signed(const char **text, int32_t *value)
{
    skip_spaces(text);
    bool negative = false;
    if (**text == '-') { negative = true; ++*text; }
    if (**text < '0' || **text > '9') return false;
    uint32_t number = 0;
    while (**text >= '0' && **text <= '9') {
        number = number * 10U + (uint32_t)(**text - '0');
        ++*text;
    }
    if ((!negative && number > 2147483647U) || (negative && number > 2147483648U)) return false;
    *value = negative ? (number == 2147483648U ? (-2147483647 - 1) : -(int32_t)number)
                      : (int32_t)number;
    return true;
}

static void execute_calc(const char *args)
{
    int32_t a, b;
    if (!parse_signed(&args, &a)) { print_warning("Usage: calc A +|-|*|/|% B"); return; }
    skip_spaces(&args);
    char operation = *args++;
    if (operation != '+' && operation != '-' && operation != '*' &&
        operation != '/' && operation != '%') {
        print_warning("Usage: calc A +|-|*|/|% B"); return;
    }
    if (!parse_signed(&args, &b)) { print_warning("Usage: calc A +|-|*|/|% B"); return; }
    skip_spaces(&args);
    if (*args) { print_warning("Usage: calc A +|-|*|/|% B"); return; }
    if ((operation == '/' || operation == '%') && b == 0) {
        print_error("Division by zero is undefined."); return;
    }
    int64_t answer = operation == '+' ? (int64_t)a + b :
                     operation == '-' ? (int64_t)a - b :
                     operation == '*' ? (int64_t)a * b :
                     operation == '/' ? a / b : a % b;
    kprintf("  %d %c %d = %d\n", a, operation, b, (int32_t)answer);
    if (answer > 2147483647LL || answer < -2147483648LL)
        print_warning("Result overflowed the 32-bit calculator display.");
}

static bool ensure_notes_directory(void)
{
    return minifs_directory_exists("/notes") || minifs_mkdir("/notes");
}

static bool note_path(const char *name, char path[MINIFS_PATH_LENGTH])
{
    if (!name || !name[0] || kstrchr(name, '/') || kstrcmp(name, ".") == 0 ||
        kstrcmp(name, "..") == 0) return false;
    return ksnprintf(path, MINIFS_PATH_LENGTH, "/notes/%s", name) < MINIFS_PATH_LENGTH;
}

static void execute_notes(const char *line)
{
    if (!ensure_notes_directory()) { print_error("Could not create /notes."); return; }
    if (kstrcmp(line, "notes") == 0 || kstrcmp(line, "note list") == 0) {
        print_title("N", "NOTES", "MiniFS notes shared with the desktop editor");
        minifs_dirent_t entries[MINIFS_MAX_ENTRIES];
        size_t count = minifs_listdir("/notes", entries, MINIFS_MAX_ENTRIES);
        if (!count) kprintf("  (no notes yet)\n");
        for (size_t i = 0; i < count; ++i)
            if (!entries[i].is_dir) kprintf("  %-36s %u B\n", entries[i].name, entries[i].size);
        print_rule();
        kprintf("  note write NAME TEXT | read NAME | append NAME TEXT | delete NAME\n");
        return;
    }

    bool write = kstrncmp(line, "note write ", 11) == 0;
    bool append = kstrncmp(line, "note append ", 12) == 0;
    bool read = kstrncmp(line, "note read ", 10) == 0;
    bool remove = kstrncmp(line, "note delete ", 12) == 0;
    const char *argument = line + (write ? 11 : append ? 12 : read ? 10 : remove ? 12 : 0);
    if (!write && !append && !read && !remove) {
        print_warning("Usage: notes | note write|read|append|delete NAME [TEXT]");
        return;
    }
    char name[MINIFS_PATH_LENGTH];
    size_t n = 0;
    while (argument[n] && argument[n] != ' ' && n + 1 < sizeof(name)) {
        name[n] = argument[n]; ++n;
    }
    name[n] = 0;
    char path[MINIFS_PATH_LENGTH];
    if (!note_path(name, path)) { print_error("Invalid note name (slashes are not allowed)."); return; }
    const char *text = argument + n;
    while (*text == ' ') ++text;
    if (read) {
        size_t size = 0; const char *contents = minifs_read(path, &size);
        if (!contents) print_error("Note not found.");
        else { print_title("N", name, "Note contents"); kprintf("  %s\n", contents); print_rule(); }
        return;
    }
    if (remove) {
        if (minifs_delete(path)) print_ok("Note deleted."); else print_error("Note not found.");
        return;
    }
    if (!*text) { print_warning("A note needs text after its name."); return; }
    char combined[MINIFS_FILE_CAPACITY + 1];
    if (append) {
        size_t old_size = 0; const char *old = minifs_read(path, &old_size);
        if (old && old_size + 1U + kstrlen(text) <= MINIFS_FILE_CAPACITY)
            ksnprintf(combined, sizeof(combined), "%s\n%s", old, text);
        else if (!old && kstrlen(text) <= MINIFS_FILE_CAPACITY)
            ksnprintf(combined, sizeof(combined), "%s", text);
        else { print_error("Append would exceed the 1024-byte note capacity."); return; }
        text = combined;
    }
    if (minifs_write(path, text)) print_ok(append ? "Note appended." : "Note saved.");
    else print_error("Could not save note (filesystem full or text too long).");
}

/* ------------------------------------------------------------------ */
/* Understanding and assessment evidence mode.                        */

static const char *s_chapter_title[] = {
    "", "OS OVERVIEW", "OS SERVICES + BOOT", "PROCESSES + IPC",
    "THREADS", "CPU SCHEDULING", "SYNCHRONIZATION", "DEADLOCKS",
    "MAIN MEMORY", "VIRTUAL MEMORY", "FILE-SYSTEM INTERFACE",
    "FILE-SYSTEM IMPLEMENTATION", "MASS STORAGE", "I/O SYSTEMS"
};

static const char *s_chapter_concept[] = {
    "", "The kernel manages CPU, memory, devices, tasks, and files.",
    "Applications reach kernel services through controlled interfaces.",
    "Processes have states; IPC transfers data without shared variables.",
    "Workers share one address space and synchronize shared state.",
    "A scheduling policy selects the next READY task for the CPU.",
    "Semaphores prevent races in producer-consumer and readers-writers.",
    "Banker's algorithm grants only requests that leave a safe sequence.",
    "The heap tracks allocation, release, splitting, and coalescing.",
    "Demand paging maps virtual pages to frames and replaces victims.",
    "Directories and file operations provide a named storage interface.",
    "MiniFS tracks entries, allocation methods, and persistent commits.",
    "Disk scheduling reduces head movement; RAID trades space for resilience.",
    "Polling, interrupts, and DMA use different CPU/device workflows."
};

static const char *s_chapter_impl[] = {
    "", "32-bit kernel dashboard plus live subsystem counters.",
    "BIOS loader, protected-mode setup, drivers, syscalls, and services.",
    "Task control blocks, lifecycle states, scheduler, and kernel mailbox.",
    "Two schedulable workers update a semaphore-protected shared counter.",
    "Runtime RR, Priority, FCFS, and SJF policies with a CPU timeline.",
    "Custom counting/binary semaphores and bounded-buffer task demos.",
    "A real resource-allocation table, safety test, request, and release.",
    "First-fit kernel heap with statistics and an allocation self-test.",
    "Page tables, frame ownership, FIFO trace, and replacement comparison.",
    "Hierarchical RAM file system with mkdir, cd, read, write, and delete.",
    "MiniFS metadata plus contiguous, linked, and indexed allocation demo.",
    "FCFS/SSTF/SCAN/C-SCAN/C-LOOK calculations and RAID comparison.",
    "Simulated device controller statistics for polling, IRQ, and DMA."
};

static const char *s_chapter_command[] = {
    "", "status", "services", "ipc demo", "thread demo", "schedule status",
    "rw demo", "banker status", "mem test", "page demo", "ls",
    "fsalloc demo", "disk demo", "io demo"
};

static void print_understanding_guide(void)
{
    print_title("U", "MY UNDERSTANDING GUIDE", "How I prove each CSE323 concept with live kernel state");
    kprintf("  1  Start with:  present 1     kernel overview and live resources\n");
    kprintf("  2  Continue:    present 2     boot path and OS services\n");
    kprintf("  3  Core jobs:   present 3     processes and IPC\n");
    kprintf("  4  Compare:     present 5     four CPU schedulers\n");
    kprintf("  5  Prove sync:  present 6     protected shared data\n");
    kprintf("  6  Safety:      present 7     Banker's safe state\n");
    kprintf("  7  Storage:     present 10    real file interface\n");
    kprintf("  8  Finish:      present 12 / present 13\n\n");
    kprintf("  present CH  states what I understand, then runs my implementation.\n");
    kprintf("  explain CH  shows my concept and implementation without changing state.\n");
    kprintf("  lectures    shows the complete chapter-to-command map.\n");
    kprintf("  stop        immediately ends all terminal background activity.\n");
    print_rule();
    kprintf("  Evidence statement: 'This output is generated by my kernel module,\n");
    kprintf("  not hard-coded UI; the counters and states change while tasks run.'\n");
}

static bool valid_chapter_argument(const char *text, unsigned *chapter)
{
    unsigned value = 0;
    if (!parse_uints(text, &value, 1) || value < 1U || value > 13U) return false;
    *chapter = value;
    return true;
}

static void explain_chapter(unsigned chapter)
{
    print_title("D", s_chapter_title[chapter], "WHAT I IMPLEMENTED AND WHAT THE DEMO PROVES");
    kprintf("  CONCEPT\n  %s\n\n", s_chapter_concept[chapter]);
    kprintf("  MY IMPLEMENTATION\n  %s\n\n", s_chapter_impl[chapter]);
    kprintf("  LIVE PROOF\n  Command: %s\n", s_chapter_command[chapter]);
    print_rule();
}

static void present_chapter(unsigned chapter)
{
    explain_chapter(chapter);
    kprintf("  [RUN] Executing chapter %u live proof now...\n", chapter);
    commands_execute(s_chapter_command[chapter]);
    if (chapter == 2U) commands_execute("boot");
    else if (chapter == 3U) commands_execute("tasks");
    else if (chapter == 5U) commands_execute("timeline");
    else if (chapter == 6U) commands_execute("rw status");
    else if (chapter == 10U) commands_execute("fsinfo");
}

/* ------------------------------------------------------------------ */
/* Main dispatcher.                                                   */

void commands_execute(const char *raw)
{
    char line[256];
    ksnprintf(line, sizeof(line), "%s", raw ? raw : "");
    size_t n = kstrlen(line);
    while (n && line[n - 1] == ' ') line[--n] = 0;
    char *p = line;
    while (*p == ' ') p++;
    if (*p == 0) return;

    if (kstrcmp(p, "help") == 0) print_help();
    else if (kstrncmp(p, "pointer ", 8) == 0) {
        if (!move_demo_pointer(p + 8))
            print_warning("Usage: pointer guide|stop|files|notes|settings|clock|oslab|terminal");
    }
    else if (kstrcmp(p, "understanding") == 0 || kstrcmp(p, "viva") == 0 ||
             kstrcmp(p, "guide") == 0) print_understanding_guide();
    else if (kstrncmp(p, "explain ", 8) == 0) {
        unsigned chapter;
        if (valid_chapter_argument(p + 8, &chapter)) explain_chapter(chapter);
        else print_warning("Usage: explain CH   (CH is 1..13)");
    }
    else if (kstrncmp(p, "present ", 8) == 0) {
        unsigned chapter;
        if (valid_chapter_argument(p + 8, &chapter)) present_chapter(chapter);
        else print_warning("Usage: present CH   (CH is 1..13)");
    }
    else if (kstrcmp(p, "syscall") == 0 || kstrncmp(p, "syscall ", 8) == 0) {
        execute_syscall(kstrcmp(p, "syscall") == 0 ? "" : p + 8);
    }
    else if (kstrcmp(p, "dashboard") == 0 || kstrcmp(p, "status") == 0) print_dashboard();
    else if (kstrcmp(p, "lectures") == 0 || kstrcmp(p, "lecture map") == 0) print_lecture_map();
    else if (kstrcmp(p, "services") == 0) print_services();
    else if (kstrcmp(p, "boot") == 0) print_boot_path();
    else if (kstrcmp(p, "stop") == 0 || kstrcmp(p, "stop all") == 0 ||
             kstrcmp(p, "lab stop") == 0) stop_all_labs();
    else if (kstrcmp(p, "about") == 0) print_about();
    else if (kstrcmp(p, "sysinfo") == 0) print_system_info();
    else if (kstrcmp(p, "timeline") == 0) print_timeline();
    else if (kstrcmp(p, "log") == 0) print_event_log();
    else if (kstrcmp(p, "log clear") == 0) {
        event_log_clear();
        print_ok("Kernel event log cleared.");
    }
    else if (kstrcmp(p, "schedule status") == 0) print_schedule_status();
    else if (kstrncmp(p, "schedule ", 9) == 0 || kstrcmp(p, "rr") == 0 ||
             kstrcmp(p, "prio") == 0 || kstrcmp(p, "fcfs") == 0 || kstrcmp(p, "sjf") == 0) {
        const char *name = kstrncmp(p, "schedule ", 9) == 0 ? p + 9 : p;
        scheduler_policy_t policy = SCHEDULER_ROUND_ROBIN;
        bool valid = true;
        if (kstrcmp(name, "rr") == 0 || kstrcmp(name, "roundrobin") == 0) policy = SCHEDULER_ROUND_ROBIN;
        else if (kstrcmp(name, "priority") == 0 || kstrcmp(name, "prio") == 0) policy = SCHEDULER_PRIORITY;
        else if (kstrcmp(name, "fcfs") == 0) policy = SCHEDULER_FCFS;
        else if (kstrcmp(name, "sjf") == 0) policy = SCHEDULER_SJF;
        else valid = false;
        if (valid && scheduler_set_policy(policy)) print_ok("Scheduler policy changed.");
        else print_warning("Usage: schedule rr | priority | fcfs | sjf | status");
    }
    else if (kstrncmp(p, "priority ", 9) == 0) {
        unsigned args[2];
        if (parse_uints(p + 9, args, 2) && args[1] <= 9U &&
            task_set_priority(args[0], (uint8_t)args[1]))
            print_ok("Task priority updated (9 is highest).");
        else print_warning("Usage: priority ID 0..9");
    }
    else if (kstrncmp(p, "burst ", 6) == 0) {
        unsigned args[2];
        if (parse_uints(p + 6, args, 2) && args[1] > 0U && args[1] <= 65535U &&
            task_set_burst(args[0], (uint16_t)args[1]))
            print_ok("Task burst estimate updated for SJF.");
        else print_warning("Usage: burst ID TICKS");
    }
    else if (kstrcmp(p, "ps") == 0 || kstrcmp(p, "tasks") == 0) print_tasks();
    else if (kstrcmp(p, "ticks") == 0) {
        kprintf("  [OK] Scheduler has completed %u custom slices.\n", scheduler_ticks());
    }
    else if (kstrcmp(p, "pause") == 0 || kstrcmp(p, "pause all") == 0) {
        scheduler_pause_except(s_shell_task_id);
        kprintf("  [PAUSED] All application tasks are frozen; the shell remains active.\n");
    }
    else if (kstrcmp(p, "resume all") == 0) {
        scheduler_resume_all();
        print_ok("All paused application tasks may run again.");
    }
    else if (kstrcmp(p, "create counter") == 0) {
        int id = demo_create_counter();
        if (id < 0) print_error("Could not create task - the task table is full.");
        else {
            char message[64];
            ksnprintf(message, sizeof(message), "Counter task %d created and READY.", id);
            print_ok(message);
        }
    }
    else if (kstrcmp(p, "demo producer") == 0 || kstrcmp(p, "demo start") == 0 || kstrcmp(p, "start") == 0) {
        if (demo_start_producer_consumer()) print_ok("Producer-consumer pipeline started.");
        else print_warning("Demo already started or the task table is full.");
    }
    else if (kstrcmp(p, "demo pause") == 0 || kstrcmp(p, "hold") == 0) {
        if (demo_pause_producer_consumer()) print_warning("Producer-consumer demo paused.");
        else print_warning("Demo is not running or is already paused.");
    }
    else if (kstrcmp(p, "demo resume") == 0 || kstrcmp(p, "continue") == 0) {
        if (demo_resume_producer_consumer()) print_ok("Producer-consumer demo resumed.");
        else print_warning("Demo is not paused.");
    }
    else if (kstrcmp(p, "demo stop") == 0) {
        if (demo_stop_producer_consumer()) print_ok("Producer-consumer demo stopped and tasks terminated.");
        else print_warning("Demo is not running.");
    }
    else if (kstrcmp(p, "demo status") == 0) print_demo_status();
    else if (kstrcmp(p, "ipc") == 0 || kstrncmp(p, "ipc ", 4) == 0) execute_ipc_command(p);
    else if (kstrcmp(p, "thread") == 0 || kstrncmp(p, "thread ", 7) == 0) execute_thread_command(p);
    else if (kstrcmp(p, "rw") == 0 || kstrncmp(p, "rw ", 3) == 0) execute_rw_command(p);
    else if (kstrncmp(p, "suspend ", 8) == 0) {
        uint32_t id = (uint32_t)kstrtoul(p + 8, NULL, 10);
        if (id == s_shell_task_id) print_warning("The shell cannot suspend itself.");
        else if (task_suspend(id)) print_ok("Task moved to SUSPENDED.");
        else print_error("Task not found.");
    }
    else if (kstrncmp(p, "resume ", 7) == 0) {
        if (task_resume((uint32_t)kstrtoul(p + 7, NULL, 10))) print_ok("Task moved to READY.");
        else print_error("Task is not suspended.");
    }
    else if (kstrncmp(p, "kill ", 5) == 0) {
        uint32_t id = (uint32_t)kstrtoul(p + 5, NULL, 10);
        if (id == s_shell_task_id) print_warning("The shell cannot terminate itself.");
        else if (task_kill(id)) print_ok("Task moved to FINISHED.");
        else print_error("Task not found.");
    }
    else if (kstrcmp(p, "mem") == 0) print_memory_stats();
    else if (kstrcmp(p, "mem test") == 0) {
        if (memory_self_test()) print_ok("Allocator test passed: allocate, free, and coalesce.");
        else print_error("Allocator self-test FAILED.");
        print_memory_stats();
    }
    else if (kstrcmp(p, "fit demo") == 0) print_fit_demo();
    else if (kstrcmp(p, "replace demo") == 0) print_replacement_demo();
    else if (kstrcmp(p, "fsalloc demo") == 0) print_fsalloc_demo();
    else if (kstrncmp(p, "page", 4) == 0) execute_paging_command(p);
    else if (kstrncmp(p, "banker", 6) == 0) execute_banker_command(p);
    else if (kstrcmp(p, "raid") == 0) print_raid();
    else if (kstrcmp(p, "disk") == 0 || kstrncmp(p, "disk ", 5) == 0) execute_disk_command(p);
    else if (kstrcmp(p, "io") == 0 || kstrncmp(p, "io ", 3) == 0) execute_io_command(p);
    else if (kstrcmp(p, "calc") == 0 || kstrncmp(p, "calc ", 5) == 0)
        execute_calc(kstrcmp(p, "calc") == 0 ? "" : p + 5);
    else if (kstrcmp(p, "notes") == 0 || kstrcmp(p, "note") == 0 ||
             kstrncmp(p, "note ", 5) == 0) execute_notes(p);
    else if (kstrcmp(p, "pwd") == 0) kprintf("  %s\n", s_cwd);
    else if (kstrcmp(p, "ls") == 0 || kstrncmp(p, "ls ", 3) == 0) {
        char path[MINIFS_PATH_LENGTH];
        const char *argument = kstrcmp(p, "ls") == 0 ? s_cwd : p + 3;
        while (*argument == ' ') argument++;
        if (!resolve_path(argument, path) || !minifs_directory_exists(path)) {
            print_error("Directory not found.");
        } else {
            print_title("F", path, "MiniFS v2 persistent directory");
            minifs_dirent_t entries[MINIFS_MAX_ENTRIES];
            size_t count = minifs_listdir(path, entries, MINIFS_MAX_ENTRIES);
            kprintf("  TYPE  NAME                            SIZE\n");
            kprintf("  ----  ------------------------------  --------\n");
            if (count == 0) kprintf("  (directory is empty)\n");
            for (size_t i = 0; i < count; ++i) {
                kprintf("  %-4s  %-30s  %6u B\n",
                        entries[i].is_dir ? "DIR" : "FILE", entries[i].name, entries[i].size);
            }
            print_rule();
        }
    }
    else if (kstrncmp(p, "mkdir ", 6) == 0) {
        char path[MINIFS_PATH_LENGTH];
        if (resolve_path(p + 6, path) && minifs_mkdir(path)) print_ok("Directory created.");
        else print_error("mkdir failed: invalid path, missing parent, duplicate, or full filesystem.");
    }
    else if (kstrncmp(p, "cd ", 3) == 0) {
        char path[MINIFS_PATH_LENGTH];
        if (resolve_path(p + 3, path) && minifs_directory_exists(path)) {
            ksnprintf(s_cwd, sizeof(s_cwd), "%s", path);
            print_ok("Current directory changed.");
        } else print_error("Directory not found.");
    }
    else if (kstrncmp(p, "rmdir ", 6) == 0) {
        char path[MINIFS_PATH_LENGTH];
        if (resolve_path(p + 6, path) && minifs_rmdir(path)) print_ok("Empty directory removed.");
        else print_error("rmdir failed: directory missing or not empty.");
    }
    else if (kstrncmp(p, "write ", 6) == 0) {
        char *name = p + 6;
        char *text = kstrchr(name, ' ');
        if (text == NULL) print_warning("Usage: write NAME TEXT");
        else {
            char path[MINIFS_PATH_LENGTH];
            *text++ = 0;
            if (resolve_path(name, path) && minifs_write(path, text)) print_ok("File written.");
            else print_error("Write failed: invalid path, missing directory, or full filesystem.");
        }
    }
    else if (kstrncmp(p, "cat ", 4) == 0) {
        char path[MINIFS_PATH_LENGTH];
        size_t size = 0;
        const char *text = resolve_path(p + 4, path) ? minifs_read(path, &size) : NULL;
        if (text == NULL) print_error("File not found.");
        else {
            print_title("F", path, "MiniFS file contents");
            kprintf("  %s\n", text);
            print_rule();
        }
    }
    else if (kstrncmp(p, "rm ", 3) == 0) {
        char path[MINIFS_PATH_LENGTH];
        if (resolve_path(p + 3, path) && minifs_delete(path)) print_ok("File deleted.");
        else print_error("File not found.");
    }
    else if (kstrcmp(p, "fsinfo") == 0) {
        print_title("F", "MINIFS STORAGE", "Custom persistent file system");
        unsigned used, files, dirs;
        uint32_t commits;
        minifs_get_info(&used, &files, &dirs, &commits);
        kprintf("  Entries  %u / %u\n", used, (unsigned)MINIFS_MAX_ENTRIES);
        kprintf("  Files    %u\n", files);
        kprintf("  Dirs     %u (+ root)\n", dirs);
        kprintf("  Commits  %u\n", commits);
        print_rule();
    }
    else if (kstrcmp(p, "format YES") == 0) {
        if (minifs_format()) print_ok("MiniFS formatted; all files erased.");
        else print_error("Format failed.");
    }
    else if (kstrncmp(p, "format", 6) == 0) print_warning("Destructive action. Type exactly: format YES");
    else if (kstrcmp(p, "clear") == 0) {
        extern void terminal_clear(void);
        terminal_clear();
    }
    else if (kstrcmp(p, "reboot") == 0) {
        print_ok("Restarting the machine now...");
        reboot_system();
    }
    else if (kstrcmp(p, "shutdown") == 0 || kstrcmp(p, "poweroff") == 0) {
        print_ok("Shutting down...");
        shutdown_system();
    }
    else print_warning("Unknown command. Type 'help' to open the command center.");
}
