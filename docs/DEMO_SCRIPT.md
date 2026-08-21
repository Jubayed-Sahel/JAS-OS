# JAS OS 4:17 narrated demonstration script

The repository includes a [narrated 1080p MP4 demonstration](JAS-OS-demo.mp4)
and its [editable presentation source](JAS-OS-Demonstration.pptx). Every system
screen was captured from the real JAS OS ISO running in Oracle VirtualBox.

## 0:00-0:21 — Introduction

> Welcome to JAS OS, a bootable 32-bit x86 educational operating system created
> by Jubayed Ahmed Sahel for CSE 323. This demonstration uses the real ISO and
> covers the main concepts from chapters 1 through 13.

## 0:21-0:45 — Teacher demonstration mode

Command: `teacher`

> Teacher mode provides an ordered tour. `explain` describes a concept without
> changing state, while `present` runs its live kernel proof. The sidebar also
> provides Files, Notes, Task Manager, Calculator, Settings, Clock, and OS Lab.

## 0:45-1:08 — Kernel overview

Command: `present 1`

> JAS OS boots through its own loader into protected mode, then initializes
> interrupts, scheduling, memory, paging, MiniFS, storage, and I/O. The displayed
> values come from current kernel state rather than static labels.

## 1:08-1:29 — Processes and IPC

Command: `present 3`

> Task control blocks expose identifiers, priorities, and lifecycle states. A
> kernel mailbox performs explicit send and receive operations while the task
> table keeps the shell and demonstration jobs observable.

## 1:29-1:51 — CPU scheduling

Command: `present 5`

> JAS OS implements Round Robin, Priority, FCFS, and SJF. The scheduling policy
> can be changed at runtime, and the timeline records recent CPU slices.

## 1:51-2:10 — Synchronization

Command: `present 6`

> The readers-writers lab uses kernel semaphores. Multiple readers may enter
> together, while a writer receives exclusive access. Live counters show
> waiting, ownership, and progress.

## 2:10-2:29 — Global stop

Command: `stop`

> The stop command terminates every non-shell background task and stops terminal
> activity while leaving the shell responsive. The red STOP button, `stop all`,
> and `lab stop` invoke the same kernel action.

## 2:29-2:49 — Deadlock avoidance

Command: `present 7`

> Banker's algorithm displays allocation, maximum demand, need, and available
> resources. A request is granted only when a safe execution sequence remains.

## 2:49-3:10 — Virtual memory

Command: `present 9`

> Paging translates virtual addresses, allocates frames, tracks dirty pages, and
> counts hits and faults. FIFO replacement reports the selected victim page.

## 3:10-3:29 — Mass storage

Command: `present 12`

> FCFS, SSTF, SCAN, C-SCAN, and C-LOOK run on the same request queue and report
> total head movement for direct comparison.

## 3:29-3:52 — I/O systems

Command: `present 13`

> Polling, interrupts, and DMA are compared with live counters. Together with
> the earlier labs, these features implement the main course knowledge through
> chapter 13.

## 3:52-4:17 — Closing

> JAS OS is a real bootable freestanding educational kernel. Its simulations are
> labeled, its state is inspectable, and every major demonstration can be stopped
> safely. The source, ISO, report, and narrated demonstration are on GitHub.

## Useful answers during questions

- **Is it a real OS?** It is a real bootable freestanding kernel; educational
  paging, disk, RAID, and I/O models are clearly labeled as simulations.
- **Why cooperative tasks?** They make every transition bounded and inspectable,
  keeping the scheduling algorithms small enough to explain.
- **How do you stop output?** Use `stop` or the red STOP button. All non-shell
  background tasks are terminated and the shell remains responsive.
- **What did you exclude?** Chapter 14 onward, user-mode isolation, networking,
  and production disk/device support.
