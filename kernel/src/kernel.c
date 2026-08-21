#include "banker.h"
#include "commands.h"
#include "event_log.h"
#include "filesystem.h"
#include "gfx.h"
#include "gui.h"
#include "hw.h"
#include "input.h"
#include "klib.h"
#include "lecture.h"
#include "memory.h"
#include "paging.h"
#include "scheduler.h"
#include "storage.h"
#include "task.h"

static void shell_step(kernel_task_t *task)
{
    commands_set_shell_task_id(task->id);
    task_sleep(task, 30);
}

void kernel_main(boot_info_t *info)
{
    serial_init();
    serial_write_str("JAS OS x86 booting...\n");

    pic_remap();
    idt_init();
    pit_init(1000);
    keyboard_init();
    mouse_init();

    if (!gfx_init(info)) {
        serial_write_str("Framebuffer init failed.\n");
        for (;;) cpu_halt();
    }
    mouse_set_bounds(gfx_width(), gfx_height());
    wallpaper_build();

    task_system_init();
    event_log_init();
    scheduler_init();
    memory_init();
    paging_init();
    banker_init_demo();
    storage_init();
    lecture_init();
    commands_init();
    minifs_init();
    minifs_write("/welcome.txt", "Welcome to JAS OS. Click the desktop icons or type help.");

    if (task_create("shell", shell_step, NULL) < 0) {
        serial_write_str("Could not create shell task.\n");
        for (;;) cpu_halt();
    }

    gui_init();
    irq_enable();
    event_log_add("BOOT", "JAS OS x86 %s graphical kernel online", KERNEL_VERSION);

    for (;;) {
        scheduler_run_one_slice();
        gui_tick();
        cpu_halt();
    }
}
