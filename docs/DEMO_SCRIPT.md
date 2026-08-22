# JAS OS 4:20 narrated demonstration script

The repository includes a [narrated 1080p MP4 demonstration](JAS-OS-demo.mp4)
and its [editable presentation source](JAS-OS-Demonstration.pptx). Every system
screen was captured from the real JAS OS ISO running in Oracle VirtualBox.

## 0:00-0:21 — Introduction

> Welcome to JAS OS, my bootable 32-bit x86 course-project operating system for
> CSE 323. It runs as a freestanding kernel and provides a graphical desktop,
> terminal, applications, and live views of its major subsystems.

## 0:21-0:47 — Feature guide

Command: `guide`

> The guide maps each kernel feature to direct commands and live state. The
> sidebar also provides Files, Notes, Task Manager, Calculator, Settings, Clock,
> and OS Lab.

## 0:47-1:10 — Kernel overview

Command: `status`

> JAS OS boots through its own loader into protected mode, then initializes
> interrupts, scheduling, memory, paging, MiniFS, storage, and I/O. The displayed
> values come from current kernel state rather than static labels.

## 1:10-1:31 — Processes and IPC

Command: `ipc demo`

> Task control blocks expose identifiers, priorities, and lifecycle states. A
> kernel mailbox performs explicit send and receive operations while the task
> table keeps the shell and demonstration jobs observable.

## 1:31-1:53 — CPU scheduling

Command: `schedule status`

> JAS OS implements Round Robin, Priority, FCFS, and SJF. The scheduling policy
> can be changed at runtime, and the timeline records recent CPU slices.

## 1:53-2:12 — Synchronization

Command: `rw demo`

> The readers-writers lab uses kernel semaphores. Multiple readers may enter
> together, while a writer receives exclusive access. Live counters show
> waiting, ownership, and progress.

## 2:12-2:31 — Global stop

Command: `stop`

> The stop command terminates every non-shell background task and stops terminal
> activity while leaving the shell responsive. The red STOP button, `stop all`,
> and `lab stop` invoke the same kernel action.

## 2:31-2:51 — Deadlock avoidance

Command: `banker status`

> Banker's algorithm displays allocation, maximum demand, need, and available
> resources. A request is granted only when a safe execution sequence remains.

## 2:51-3:12 — Virtual memory

Command: `page demo`

> Paging translates virtual addresses, allocates frames, tracks dirty pages, and
> counts hits and faults. FIFO replacement reports the selected victim page.

## 3:12-3:31 — Mass storage

Command: `disk demo`

> FCFS, SSTF, SCAN, C-SCAN, and C-LOOK run on the same request queue and report
> total head movement for direct comparison.

## 3:31-3:54 — I/O systems

Command: `io demo`

> Polling, interrupts, and DMA are compared with live counters. Together with
> the earlier labs, these commands show how each subsystem changes kernel state.

## 3:54-4:20 — Closing

> JAS OS is a real bootable freestanding course-project kernel. Its models are
> labeled, its state is inspectable, and every demonstration can be stopped
> safely. The source, ISO, STAR report, and narrated evidence are on GitHub.

## Useful answers during questions

- **Is it a real OS?** It is a real bootable freestanding kernel; course-concept
  paging, disk, RAID, and I/O models are clearly labeled as simulations.
- **Why cooperative tasks?** They make every transition bounded and inspectable,
  keeping the scheduling algorithms small enough to explain.
- **How do you stop output?** Use `stop` or the red STOP button. All non-shell
  background tasks are terminated and the shell remains responsive.
- **What did you exclude?** User-mode isolation, networking, and production
  disk or device support.
