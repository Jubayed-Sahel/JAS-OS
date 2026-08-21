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
The motive is to implement, test, and prove **my own understanding** of the main
CSE 323 lecture concepts from chapters 1-13. The project is not intended to
teach other people. Instead, every feature gives me observable evidence that I
can connect a lecture concept to code, changing kernel state, and an algorithmic
result.

## 2. Project objective and scope

The earlier project was an ESP32-S3 mini-kernel. My objective was to transfer
its useful operating-system ideas to a PC-style x86 kernel that boots from an ISO
in Oracle VirtualBox. I included the main concepts from chapters 1-13 and
excluded chapter 14 onward as required.

The result includes a graphical desktop, terminal, Files, Notes, Task Manager,
Calculator, Settings, Clock, and OS Lab. These applications make the kernel
state easier for me to inspect during assessment; they are not presented as a
teaching platform.

## 3. Implemented lecture concepts

| Chapter | Concept | Evidence in JAS OS |
|---:|---|---|
| 1 | OS organization | Live subsystem dashboard |
| 2 | Services and boot | BIOS-to-protected-mode trace and syscall aliases |
| 3 | Processes and IPC | Task states and bounded mailbox send/receive |
| 4 | Threads | Shared counter workers |
| 5 | CPU scheduling | RR, Priority, FCFS, SJF, and timeline |
| 6 | Synchronization | Semaphores and readers-writers |
| 7 | Deadlocks | Banker's safety algorithm and safe sequence |
| 8 | Main memory | First-fit heap, splitting, and coalescing |
| 9 | Virtual memory | Paging, faults, frames, and FIFO replacement |
| 10 | File-system interface | MiniFS directories and file operations |
| 11 | File-system implementation | Allocation-method comparison |
| 12 | Mass storage | Five disk-scheduling algorithms and RAID model |
| 13 | I/O systems | Polling, interrupts, DMA, and live counters |

## 4. Evidence of my understanding

The `explain CH` command states the concept and identifies my implementation.
The `present CH` command then runs that implementation and prints live evidence.
For example, scheduling changes the CPU timeline, readers-writers changes
semaphore-protected state, Banker's algorithm calculates a safe sequence, and
paging changes frame ownership and fault counts. OS Lab calls the same path with
buttons. `stop` ends all non-shell activity while keeping the terminal usable.

## 5. Challenges using the STAR format

### Challenge 1 — Moving from ESP32-S3 to a bootable x86 ISO

**Situation:** The previous mini-kernel depended on an ESP32-S3 runtime, while
this project needed to boot as a PC operating system in Oracle VirtualBox.

**Task:** Preserve the relevant course concepts without a host operating system
or standard library and provide keyboard, mouse, and graphical output.

**Action:** I implemented a BIOS/El Torito loader, VESA framebuffer setup, A20,
GDT and protected-mode transition, then added IDT/PIC/PIT, serial, PS/2 input,
graphics, and freestanding runtime modules.

**Result:** JAS OS boots from its own ISO into an interactive 1024×768 desktop,
and the cross-compiler build completes without warnings.

### Challenge 2 — Proving understanding instead of printing labels

**Situation:** Printing algorithm names would not demonstrate that I understand
how the concepts operate or affect system state.

**Task:** Connect every included chapter to stateful code and observable results.

**Action:** I separated scheduling, tasks, synchronization, memory, paging,
Banker's algorithm, files, storage, and I/O into kernel modules. I exposed task
states, timelines, semaphore values, heap statistics, safe sequences, page
faults, disk-head movement, and I/O counters.

**Result:** Chapters 1-13 each have a command and OS Lab button backed by live
kernel state, allowing me to explain what changed and why.

### Challenge 3 — Organizing evidence during assessment

**Situation:** The command set became difficult to remember, and background
output could hide the result I needed to discuss.

**Task:** Make my implementation evidence easy to find without replacing the
underlying algorithms with static interface text.

**Action:** I added the understanding Guide, `explain CH`, `present CH`, chapter
map, readable terminal layout, scrolling controls, and OS Lab shortcuts. Each
path identifies the concept, my implementation, and its live proof.

**Result:** I can follow a consistent assessment route and connect each lecture
concept to the corresponding source-backed result.

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
- The terminal and OS Lab expose chapters 1-13 through live state.
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
Paging, disk, RAID, and I/O are transparent models used to test my understanding.
The project does not claim ring-3 isolation, production device drivers,
networking, persistence, or chapter 14+ protection and security.
