# JAS OS architecture

JAS OS is a freestanding i686 kernel that boots directly from an El Torito ISO.
There is no host operating system or C standard library underneath the kernel.

## Boot path

1. BIOS selects the bootable floppy image embedded in the ISO.
2. The 16-bit loader reads the kernel sectors into physical memory.
3. BIOS VESA services provide a 1024x768 linear framebuffer.
4. The loader enables A20 and installs a Global Descriptor Table.
5. The CPU switches to 32-bit protected mode and enters `_start`.
6. `_start` clears BSS, installs the kernel stack, and calls `kernel_main`.

## Kernel initialization order

```text
serial -> IDT/PIC -> PIT -> PS/2 keyboard/mouse -> graphics
       -> task table -> scheduler -> heap -> paging -> synchronization
       -> Banker -> MiniFS -> storage/I/O models -> event log -> GUI
```

## Major modules

| Module | Responsibility |
|---|---|
| `boot/boot.S` | BIOS loader, VESA, A20, GDT, protected-mode transition |
| `kernel/src/hw.c` | IDT, PIC, PIT, serial, reboot, shutdown |
| `kernel/src/input.c` | PS/2 keyboard and mouse drivers |
| `kernel/src/gfx.c` | Framebuffer drawing, fonts, wallpaper, backbuffer |
| `kernel/src/gui.c` | Desktop, windows, applications, terminal rendering |
| `kernel/src/task.c` | Task control blocks and lifecycle states |
| `kernel/src/scheduler.c` | RR, Priority, FCFS, SJF, CPU timeline |
| `kernel/src/sync.c` | Counting/binary semaphore behavior |
| `kernel/src/demos.c` | Process, thread, producer-consumer, readers-writers labs |
| `kernel/src/memory.c` | First-fit heap with splitting and coalescing |
| `kernel/src/paging.c` | Per-process virtual-page and frame model |
| `kernel/src/banker.c` | Deadlock avoidance and safe-sequence search |
| `kernel/src/filesystem.c` | Hierarchical MiniFS |
| `kernel/src/storage.c` | Disk scheduling and I/O statistics |
| `kernel/src/lecture.c` | IPC mailbox and replacement comparisons |
| `kernel/src/commands.c` | Shell, syscalls, teacher mode, chapter demos |

## Scheduling model

Tasks are cooperative step functions. Each scheduler slice selects a runnable
task according to the active policy, calls one bounded step, records the task ID
in the timeline, and returns to the GUI loop. Sleeping and semaphore-blocked
tasks become runnable when their condition changes.

## Presentation path

`present CH` prints three pieces of evidence before running the chapter command:

1. the course concept;
2. the concrete JAS OS implementation;
3. the live command that proves the implementation.

The OS Lab window calls this same path, keeping mouse and terminal demonstrations
consistent. `stop` shuts down named labs, kills remaining non-shell tasks, restores
the scheduler, and leaves the shell responsive.

## Scope boundary

Paging, disk scheduling, RAID, and I/O modes are explicit educational models.
MiniFS is RAM-backed for one VM session. JAS OS does not claim ring-3 isolation,
a hardware disk driver, networking, or chapter 14+ protection/security.
