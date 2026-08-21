# JAS OS 4:20 narrated demonstration script

The repository includes a [narrated 1080p MP4 demonstration](JAS-OS-demo.mp4)
and its [editable presentation source](JAS-OS-Demonstration.pptx). Every system
screen was captured from the real JAS OS ISO running in Oracle VirtualBox.

## 0:00-0:21 — Introduction

> Welcome to JAS OS, my bootable 32-bit x86 course-project operating system for
> CSE 323. The motive is to implement, test, and prove my own understanding of
> the lecture concepts from chapters 1 through 13, not to teach other people.

## 0:21-0:47 — Understanding evidence mode

Command: `guide`

> The guide maps each lecture concept to my implementation and live evidence.
> `explain` states what I understand without changing state, while `present`
> runs the corresponding kernel proof. The sidebar also provides Files, Notes,
> Task Manager, Calculator, Settings, Clock, and OS Lab.

## 0:47-1:10 — Kernel overview

Command: `present 1`

> JAS OS boots through its own loader into protected mode, then initializes
> interrupts, scheduling, memory, paging, MiniFS, storage, and I/O. The displayed
> values come from current kernel state rather than static labels.

## 1:10-1:31 — Processes and IPC

Command: `present 3`

> Task control blocks expose identifiers, priorities, and lifecycle states. A
> kernel mailbox performs explicit send and receive operations while the task
> table keeps the shell and demonstration jobs observable.

## 1:31-1:53 — CPU scheduling

Command: `present 5`

> JAS OS implements Round Robin, Priority, FCFS, and SJF. The scheduling policy
> can be changed at runtime, and the timeline records recent CPU slices.

## 1:53-2:12 — Synchronization

Command: `present 6`

> The readers-writers lab uses kernel semaphores. Multiple readers may enter
> together, while a writer receives exclusive access. Live counters show
> waiting, ownership, and progress.

## 2:12-2:31 — Global stop

Command: `stop`

> The stop command terminates every non-shell background task and stops terminal
> activity while leaving the shell responsive. The red STOP button, `stop all`,
> and `lab stop` invoke the same kernel action.

## 2:31-2:51 — Deadlock avoidance

Command: `present 7`

> Banker's algorithm displays allocation, maximum demand, need, and available
> resources. A request is granted only when a safe execution sequence remains.

## 2:51-3:12 — Virtual memory

Command: `present 9`

> Paging translates virtual addresses, allocates frames, tracks dirty pages, and
> counts hits and faults. FIFO replacement reports the selected victim page.

## 3:12-3:31 — Mass storage

Command: `present 12`

> FCFS, SSTF, SCAN, C-SCAN, and C-LOOK run on the same request queue and report
> total head movement for direct comparison.

## 3:31-3:54 — I/O systems

Command: `present 13`

> Polling, interrupts, and DMA are compared with live counters. Together with
> the earlier labs, these features implement the main course knowledge through
> chapter 13.

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
- **What did you exclude?** Chapter 14 onward, user-mode isolation, networking,
  and production disk/device support.
