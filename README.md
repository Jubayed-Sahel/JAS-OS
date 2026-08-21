# JAS OS

![Version](https://img.shields.io/badge/version-1.5.4-0f766e)
![Architecture](https://img.shields.io/badge/architecture-i686-334155)
![Platform](https://img.shields.io/badge/platform-freestanding_x86-2563eb)
![Build](https://img.shields.io/badge/build-passing-15803d)

JAS OS is a bootable, freestanding 32-bit x86 course-project operating system
created by **Jubayed Ahmed Sahel** for CSE 323. It boots from an ISO in Oracle
VirtualBox and exposes its kernel subsystems through a graphical desktop,
terminal, OS Lab, and live status views.

## Submission package

| Deliverable | Link |
|---|---|
| Bootable operating system | [Download `jas-os.iso`](jas-os.iso) |
| Demonstration video | [4:20 narrated overview](docs/JAS-OS-demo.mp4) |
| Editable demonstration | [PowerPoint with narration](docs/JAS-OS-Demonstration.pptx) |
| Technical report | [PDF](docs/JAS-OS-Technical-Report.pdf) · [Editable DOCX](docs/JAS-OS-Technical-Report.docx) · [GitHub-readable STAR report](docs/TECHNICAL_REPORT.md) |
| Presentation support | [Timed narration and command script](docs/DEMO_SCRIPT.md) |
| Architecture | [System architecture notes](docs/ARCHITECTURE.md) |
| Submission checklist | [Final checklist](SUBMISSION.md) |

Repository: <https://github.com/Jubayed-Sahel/JAS-OS>

## What is implemented

| Feature | Live proof |
|---|---|
| OS organization and resources | `status` / `sysinfo` |
| Services, syscalls, and boot | `services` / `syscall` / `boot` |
| Processes and IPC | `tasks` / `ipc demo` |
| Threads and shared state | `thread demo` / `thread status` |
| CPU scheduling | `schedule status` / `timeline` |
| Synchronization | `start` / `rw demo` |
| Deadlock avoidance | `banker status` / `banker safe` |
| Main-memory allocation | `mem test` / `fit demo` |
| Virtual memory | `page demo` / `replace demo` |
| File system | `fsinfo` / `fsalloc demo` |
| Mass storage | `disk demo` / `raid` |
| I/O systems | `io demo` / `io status` |

The algorithms are backed by separate kernel modules rather than static GUI
labels. Live output includes task IDs and states, CPU slices, semaphore values,
heap statistics, safe sequences, page faults and frame ownership, file-system
metadata, disk-head movement, and I/O counters.

## Desktop applications

- **Files** - browse, create, open, and delete MiniFS entries.
- **Notes** - edit files stored in MiniFS.
- **Terminal** - high-contrast shell with Feature Guide, Modules, scrolling, and STOP.
- **Task Manager** - inspect processes and terminate a selected task.
- **Calculator** - perform signed integer arithmetic.
- **Settings** - change wallpaper and CPU scheduling policy.
- **Clock** - view PIT-driven uptime and use a stopwatch.
- **OS Lab** - run kernel features and inspect their live state.

## Feature demonstration mode

Open Terminal and click **Guide**, or type:

```text
guide
```

These commands provide a short path through the implemented features:

```text
stop         # terminate all background activity but keep the shell responsive
modules      # show the complete feature-to-command map
```

`stop`, `stop all`, and `lab stop` are global emergency-stop aliases. Specific
lab controls such as `demo stop`, `thread stop`, and `rw stop` remain available.

## Boot and kernel architecture

```text
BIOS / El Torito ISO
        |
16-bit loader: sectors, VESA, A20, GDT
        |
32-bit protected mode and freestanding C kernel
        |
IDT + PIC + PIT + serial + PS/2 + framebuffer
        |
tasks | scheduler | sync | heap | paging | Banker | MiniFS | storage/I/O
        |
graphical desktop | applications | terminal | OS Lab
```

## Run in Oracle VirtualBox

1. Create a VM using **Other/Unknown (32-bit)**.
2. Allocate at least 64 MB RAM (512 MB is recommended).
3. Use **BIOS**, not EFI.
4. Set the pointing device to **PS/2 Mouse**.
5. Mount [`jas-os.iso`](jas-os.iso) as the optical disk.
6. Boot from DVD.

For QEMU:

```bash
qemu-system-i386 -cdrom jas-os.iso -m 128 -boot d
```

## Build from source

The build requires Python 3 and an `i686-elf` GCC/binutils toolchain. The build
script checks `tools/bin` first and then the system `PATH`.

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

or:

```bash
make
```

The build generates `build/jas-os.iso` and compatibility copies for older
VirtualBox attachments. The verified submission ISO is 1,523,712 bytes with:

```text
SHA-256  CC89E64B14337AFAE5DAEE51B2C7A059C92BE3D808551495B41C981EAF17A2C1
```

## Terminal command reference

```text
guide / modules
status / services / boot / syscall
tasks / create counter / ipc demo / thread demo|status|stop
schedule rr|priority|fcfs|sjf / priority ID 0..9 / burst ID TICKS / timeline
start / hold / continue / demo stop / rw demo|status|stop
banker status|safe|request|release
mem / mem test / fit demo
page demo|status|test / replace demo
ls / mkdir / cd / write / cat / rm / fsinfo / fsalloc demo
disk demo|fcfs|sstf|scan|cscan|clook / raid
io demo|status|poll|interrupt|dma|modes
calc A OP B / notes / note write|read|append|delete
stop / stop all / lab stop / clear / reboot / shutdown
```

## Course-project boundaries

JAS OS is a course-project kernel, not a production OS. Tasks are cooperative
step functions; applications execute in kernel space; MiniFS is RAM-backed for
the VM session; and paging, disk, RAID, and I/O are transparent software models.
The project does not claim user-mode isolation,
networking, a real disk driver, or production security.

## Author

**Jubayed Ahmed Sahel**<br>
Student ID: 2221173642<br>
CSE 323, Section 3
