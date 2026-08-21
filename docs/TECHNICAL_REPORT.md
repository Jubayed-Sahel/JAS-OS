# JAS OS technical report

**Student:** Jubayed Ahmed Sahel<br>
**Student ID:** 2221173642<br>
**Course:** CSE 323, Section 3<br>
**Version:** JAS OS v1.5.3<br>
**Repository:** <https://github.com/Jubayed-Sahel/JAS-OS>

The formatted submission report is available as [PDF](JAS-OS-Technical-Report.pdf)
and [editable DOCX](JAS-OS-Technical-Report.docx).

## Project result

JAS OS is a bootable, freestanding 32-bit x86 educational operating system. It
implements the major concepts from chapters 1-13 as observable kernel modules
and runs in Oracle VirtualBox with a graphical desktop, terminal, Files, Notes,
Task Manager, Calculator, Settings, Clock, and OS Lab.

## STAR challenge 1: ESP32-S3 to x86

**Situation:** The earlier mini-kernel depended on an ESP32-S3 runtime, while the
submission needed to boot as a PC ISO.

**Task:** Preserve the course features without a host OS or standard library and
provide keyboard, mouse, and graphical output.

**Action:** I implemented a BIOS/El Torito loader, VESA framebuffer setup, A20,
GDT and protected-mode transition, then built freestanding IDT/PIC/PIT, serial,
PS/2 input, graphics, and runtime-library modules.

**Result:** JAS OS boots directly from a 1.44 MB ISO and presents an interactive
1024x768 desktop in VirtualBox. The cross-compiler build finishes with no warnings.

## STAR challenge 2: Making course concepts observable

**Situation:** Merely printing the names of algorithms would not prove an
operating-system implementation during a live demonstration.

**Task:** Connect each chapter to stateful code and visible evidence.

**Action:** I separated tasks, scheduling, synchronization, memory, paging,
Banker's algorithm, files, storage, and I/O into kernel modules. The terminal
shows process states, CPU timelines, semaphore values, allocation statistics,
page faults, safe sequences, disk-head movement, and I/O counters.

**Result:** Every chapter from 1-13 has a direct command and OS Lab button backed
by live kernel state.

## STAR challenge 3: Explaining the terminal

**Situation:** The command set became too dense to remember during assessment,
and asynchronous output could hide the most relevant result.

**Task:** Make the project easy to present without weakening the implementation.

**Action:** I redesigned the terminal and added `teacher`, `explain CH`, and
`present CH`. Every OS Lab button now prints the concept, implementation, and
live proof before running the chapter demonstration.

**Result:** The project now provides a clear viva route and a readable live demo.
The submitted 4:17 narrated 1080p video follows this same workflow.

## STAR challenge 4: A dependable stop command

**Situation:** Producer-consumer, thread, readers-writers, and counter tasks can
all write asynchronous terminal output. The old `stop` only controlled one demo.

**Task:** Stop every background activity without killing the shell.

**Action:** The global stop handler first calls each lab's normal shutdown, scans
the task table, terminates every remaining non-shell task, and restores scheduler
state. Specific controls such as `demo stop` and `rw stop` remain available.

**Result:** `stop`, `stop all`, `lab stop`, and the red STOP button end background
terminal activity while the shell remains READY.

## Verified result

- Kernel: 92,384 bytes.
- ISO: 1,523,712 bytes.
- ISO SHA-256: `2B5A69F0D4CF0BC80321FCA6C608B1B73E16B44074966462E49EEB870043B6EF`.
- VM: 32-bit BIOS, 1024x768 framebuffer, PS/2 input.
- Demo: 4:17 narrated 1080p MP4 built from authentic Oracle VirtualBox captures.

## Limitations

JAS OS is an educational kernel. Tasks are cooperative, applications execute in
kernel space, MiniFS is RAM-backed, and paging/disk/RAID/I/O are transparent
teaching models. Chapter 14 onward is intentionally excluded.
