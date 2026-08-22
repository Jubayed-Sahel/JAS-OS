# JAS OS technical report

**Student:** Jubayed Ahmed Sahel

**Student ID:** 2221173642

**Course:** CSE 323, Section 3

**Version:** JAS OS v1.5.4
**GitHub:** <https://github.com/Jubayed-Sahel/JAS-OS>

The formatted report is available as [PDF](JAS-OS-Technical-Report.pdf) and
[editable DOCX](JAS-OS-Technical-Report.docx).

## 1. Executive summary

JAS OS is a bootable, freestanding 32-bit x86 course-project operating system.
It boots from an ISO in Oracle VirtualBox and provides a graphical desktop,
terminal, applications, and observable kernel subsystems. Live output includes
task states, scheduling activity, synchronization values, memory statistics,
deadlock safety results, paging state, file-system metadata, storage calculations,
and I/O counters.

## 2. System overview and scope

JAS OS transfers the useful operating-system components of an earlier ESP32-S3
mini-kernel to a PC-style x86 environment. The result includes a custom boot
path, protected-mode kernel, graphical desktop, terminal, Files, Notes, Task
Manager, Calculator, Settings, Clock, and OS Lab.

The kernel includes scheduling, synchronization, deadlock avoidance, memory
allocation, paging, file-system, storage, and I/O modules. It does not claim
user-mode isolation, networking, production hardware drivers, or permanent
disk-backed persistence.

## 3. Implemented features

| Feature | Evidence in JAS OS |
|---|---|
| OS organization | Live subsystem dashboard |
| Services and boot | BIOS-to-protected-mode trace and syscall aliases |
| Processes and IPC | Task states and bounded mailbox send/receive |
| Threads | Shared-counter workers |
| CPU scheduling | RR, Priority, FCFS, SJF, and timeline |
| Synchronization | Semaphores and readers-writers |
| Deadlock avoidance | Banker's safety algorithm and safe sequence |
| Main memory | First-fit heap, splitting, and coalescing |
| Virtual memory | Paging, faults, frames, and FIFO replacement |
| File system | MiniFS directories, files, and allocation comparison |
| Mass storage | Disk-scheduling algorithms and RAID model |
| I/O systems | Polling, interrupts, DMA, and live counters |

## 4. Demonstration interface

The terminal Feature Guide and Modules screen map each subsystem to direct
commands. Scheduling changes the CPU timeline; readers-writers changes
semaphore-protected state; Banker's algorithm calculates a safe sequence; and
paging changes frame ownership and fault counts. OS Lab invokes the same kernel
commands through buttons. `stop` ends all non-shell activity while keeping the
terminal usable.

## 5. Challenges using the STAR format

### Challenge 1 — Moving from ESP32-S3 to a bootable x86 ISO

**Situation:** The previous mini-kernel depended on an ESP32-S3 runtime, while
this project needed to boot as a PC operating system in Oracle VirtualBox.

**Task:** Preserve the relevant operating-system components without a host
operating system or standard library and provide keyboard, mouse, and graphical
output.

**Action:** I implemented a BIOS/El Torito loader, VESA framebuffer setup, A20,
GDT and protected-mode transition, then added IDT/PIC/PIT, serial, PS/2 input,
graphics, and freestanding runtime modules.

**Result:** JAS OS boots from its own ISO into an interactive 1024×768 desktop,
and the cross-compiler build completes without warnings.

### Challenge 2 — Producing live evidence instead of static labels

**Situation:** Static interface labels would not show whether the algorithms
were connected to changing system state.

**Task:** Connect every major subsystem to stateful code and observable results.

**Action:** I separated scheduling, tasks, synchronization, memory, paging,
Banker's algorithm, files, storage, and I/O into kernel modules. I exposed task
states, timelines, semaphore values, heap statistics, safe sequences, page
faults, disk-head movement, and I/O counters.

**Result:** Each feature has a terminal command and OS Lab button backed by live
kernel state, making state changes and algorithm results directly observable.

### Challenge 3 — Organizing demonstrations during assessment

**Situation:** The command set became difficult to remember, and background
output could hide the result being discussed.

**Task:** Make implementation evidence easy to find without replacing the
underlying algorithms with static interface text.

**Action:** I added the Feature Guide, Modules screen, readable terminal layout,
scrolling controls, and OS Lab shortcuts. Each path opens a direct command for a
specific kernel subsystem.

**Result:** The interface now provides a consistent route from a feature name to
its source-backed live result.

### Challenge 4 — Stopping every background task safely

**Situation:** Producer-consumer, thread, readers-writers, and counter jobs can
all generate asynchronous terminal output. The old stop action controlled only
one demonstration.

**Task:** Stop all background activity without terminating the shell.

**Action:** The global stop handler calls each lab's normal shutdown, scans the
task table, terminates remaining non-shell tasks, and restores scheduler state.
Specific lab controls remain available.

**Result:** `stop`, `stop all`, `lab stop`, and the red STOP button end terminal
background activity while the shell remains ready for the next command.

## 6. Verification and result

- The freestanding kernel builds without warnings.
- The ISO boots in a 32-bit BIOS VirtualBox VM at 1024×768.
- The terminal and OS Lab expose each major subsystem through live state.
- The global stop leaves the terminal responsive.
- The narrated demonstration is between two and five minutes and uses genuine
  JAS OS captures.

## 7. Submission links

- GitHub repository: <https://github.com/Jubayed-Sahel/JAS-OS>
- Bootable ISO: [`jas-os.iso`](../jas-os.iso)
- Narrated demonstration: [`JAS-OS-demo.mp4`](JAS-OS-demo.mp4)
- Demonstration script: [`DEMO_SCRIPT.md`](DEMO_SCRIPT.md)

## 8. Limitations and scope boundary

JAS OS is a course-project kernel, not a production operating system. Tasks are
cooperative, applications execute in kernel space, and MiniFS is RAM-backed.
Paging, disk, RAID, and I/O are transparent software models.
The project does not claim ring-3 isolation, production device drivers,
networking, persistence, or production security.
