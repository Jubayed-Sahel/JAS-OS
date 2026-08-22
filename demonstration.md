# JAS OS x86 Terminal Demonstration Guide

This document explains what each terminal command does, how it works inside
JAS OS, and what its output proves. It is intended as a practical guide for a
course demonstration or viva.

## 1. Using the terminal

1. Boot `jas-os.iso` in Oracle VirtualBox.
2. Open **Terminal** from the left sidebar.
3. Click inside the command box at the bottom of the terminal window.
4. Type a command and press **Enter**.
5. Use **Old**, **New**, and **Live** to navigate terminal output.
6. Use **Hist** or the Up/Down keys to recall earlier commands.
7. Use the green **Start** button for the recommended evaluation route.
8. Use the red **STOP** button whenever a background demonstration is running.

The prompt shows the current MiniFS directory:

```text
/ $ 
```

JAS OS uses these output markers:

| Marker | Meaning |
|---|---|
| `[OK]` | The requested operation completed successfully. |
| `[!]` | A warning, usage hint, or operation that cannot currently proceed. |
| `[X]` | An invalid operation or failed check. |

Useful keyboard controls:

| Key | Action |
|---|---|
| Up / Down | Recall command history. |
| Left / Right, Home / End | Edit the current command. |
| Tab | Complete common commands. |
| Page Up / Page Down | Scroll terminal output. |
| Ctrl+C | Stop all terminal background activity. |
| Ctrl+L | Clear terminal output. |
| Escape | Cancel the current input. |

## 2. Recommended short demonstration

Type `quickstart` or click **Start** to display the recommended route. The
following sequence presents the most important features without trying to run
every command:

```text
guide
status
boot
tasks
ipc demo
schedule status
timeline
rw demo
rw status
stop
banker status
mem test
page demo
fsinfo
disk demo
io demo
help
```

This sequence demonstrates OS organization, boot services, processes, IPC, CPU
scheduling, synchronization, global task stopping, deadlock avoidance, memory,
virtual memory, file systems, storage, and I/O.

## 3. Help and system overview

| Command | What it does | How it works and what it proves |
|---|---|---|
| `help` | Opens the command center. | Prints the supported command families from the kernel command dispatcher. Use this when a command is forgotten. |
| `guide` | Shows the shortest feature-to-command map. | Connects major CSE 323 concepts to commands backed by live kernel modules. |
| `modules` | Shows the complete kernel module map. | Identifies which command exposes each subsystem, including tasks, scheduling, memory, paging, files, storage, and I/O. |
| `status` | Opens the live kernel dashboard in the terminal. | Reads current state from the scheduler, memory manager, paging system, synchronization labs, storage model, and I/O counters. `dashboard` is an alias. |
| `dashboard` | Same as `status`. | Collects live values from multiple kernel modules rather than printing a static screenshot. |
| `about` | Shows the project identity and scope. | Explains that JAS OS is a freestanding x86 course kernel and distinguishes implemented models from production OS features. |
| `sysinfo` | Shows hardware and kernel information. | Reports the architecture, framebuffer, interrupt system, timer, input devices, uptime, and kernel version. |
| `services` | Lists operating-system services. | Maps user interface, execution, I/O, files, communication, error handling, allocation, and accounting to JAS OS implementations. |
| `boot` | Explains the x86 boot path. | Shows the BIOS/El Torito loader, VESA framebuffer, A20, GDT, protected-mode transition, interrupt initialization, and subsystem startup. |
| `log` | Displays the kernel event log. | Reads the rolling event history maintained by the kernel. The newest events appear first. |
| `log clear` | Clears the event log. | Resets the event-log buffer without restarting the OS. |
| `clear` | Clears the terminal output. | Resets the graphical terminal's stored line buffer and scroll position. |

## 4. Processes, tasks, and lifecycle control

| Command | What it does | How it works and what it proves |
|---|---|---|
| `tasks` | Displays the process/task table. | Reads task control blocks and shows task IDs, names, states, priorities, burst estimates, and execution information. `ps` is an alias. |
| `create counter` | Creates a background counter task. | Allocates a task-control-block entry and places the new task in the ready state for the cooperative scheduler. |
| `suspend ID` | Suspends one task. | Changes the selected task from a runnable state to `SUSPENDED`. The terminal refuses to suspend its own shell task. |
| `resume ID` | Resumes one suspended task. | Moves the selected task back to `READY` so the scheduler can run it again. |
| `kill ID` | Terminates one task. | Changes the selected task to `FINISHED`. The shell task is protected from terminating itself. |
| `pause all` | Pauses application tasks. | Freezes all non-shell application tasks while keeping the terminal responsive. `pause` is an alias. |
| `resume all` | Resumes paused tasks. | Restores paused application tasks to schedulable states. |
| `ticks` | Shows completed scheduler slices. | Reads the scheduler's live execution counter. |

Suggested proof:

```text
create counter
tasks
suspend 2
tasks
resume 2
kill 2
tasks
```

Use the actual ID printed by `create counter`; it may not always be `2`.

## 5. CPU scheduling

| Command | What it does | How it works and what it proves |
|---|---|---|
| `schedule status` | Displays the active scheduling policy. | Reads the scheduler configuration and execution counters. |
| `schedule rr` | Selects Round Robin. | Ready tasks receive CPU slices in cyclic order. `rr` is a short alias. |
| `schedule priority` | Selects priority scheduling. | The scheduler prefers the runnable task with the highest configured priority. `prio` is a short alias. |
| `schedule fcfs` | Selects First-Come, First-Served. | Runnable tasks are selected according to arrival order. `fcfs` is a short alias. |
| `schedule sjf` | Selects Shortest Job First. | The scheduler prefers the runnable task with the smallest burst estimate. `sjf` is a short alias. |
| `priority ID 0..9` | Changes a task priority. | Updates the priority stored in the task control block; `9` is the highest value. |
| `burst ID TICKS` | Changes an SJF burst estimate. | Updates the estimated CPU burst used when SJF compares runnable tasks. |
| `timeline` | Shows recent CPU scheduling decisions. | Reads the rolling scheduler timeline so the execution order can be compared after changing policies. |

Suggested comparison:

```text
create counter
create counter
schedule rr
timeline
schedule priority
priority 2 9
timeline
schedule sjf
burst 2 1
timeline
```

Replace task IDs with the IDs visible in `tasks`.

## 6. IPC and shared execution

| Command | What it does | How it works and what it proves |
|---|---|---|
| `ipc status` | Shows the kernel mailbox state. | Reports whether the one-message mailbox is empty or full and displays sent/received counters. `ipc` is an alias. |
| `ipc send TEXT` | Sends one message. | Copies text into a bounded kernel mailbox. A second send is rejected while the mailbox is full. |
| `ipc receive` | Receives one message. | Copies the mailbox message to the terminal and makes the mailbox empty. An empty receive reports that it would block. |
| `ipc demo` | Runs a complete send/receive trace. | Demonstrates a successful send, a rejected second send, and a receive through explicit message passing. |
| `thread demo` | Starts two worker tasks sharing one counter. | Creates independently scheduled workers that access shared state protected by a semaphore. `thread start` is an alias. |
| `thread status` | Shows worker IDs and the shared counter. | Reads the live thread-lab state and demonstrates that both workers change the same resource. `thread` is an alias. |
| `thread stop` | Stops the thread lab. | Terminates both worker tasks safely. |

Suggested IPC proof:

```text
ipc status
ipc send hello
ipc status
ipc send second
ipc receive
ipc status
```

## 7. Synchronization

### Producer-consumer bounded buffer

| Command | What it does | How it works and what it proves |
|---|---|---|
| `start` | Starts producer and consumer tasks. | Uses custom semaphores for empty slots, full slots, and mutual exclusion around a bounded buffer. `demo start` and `demo producer` are aliases. |
| `demo status` | Displays the synchronization state. | Shows whether the pipeline is running, the buffer contents, item count, and semaphore values. |
| `hold` | Pauses the pipeline. | Suspends producer-consumer progress without deleting the lab state. `demo pause` is an alias. |
| `continue` | Continues a paused pipeline. | Restores producer and consumer progress. `demo resume` is an alias. |
| `demo stop` | Stops only the producer-consumer lab. | Terminates its tasks and releases its lab state. |

### Readers-writers

| Command | What it does | How it works and what it proves |
|---|---|---|
| `rw demo` | Starts two readers and one writer. | Multiple readers may inspect shared data together, while the writer receives exclusive access through semaphores. `rw start` is an alias. |
| `rw status` | Shows live readers-writers state. | Displays task IDs, active-reader count, and the changing shared value. `rw` is an alias. |
| `rw stop` | Stops the readers-writers lab. | Terminates the three demonstration tasks safely. |

### Global stop

| Command | What it does | How it works and what it proves |
|---|---|---|
| `stop` | Stops everything started from the terminal. | Calls each named lab shutdown routine, scans the task table, terminates every remaining non-shell task, resumes scheduler state, and leaves the terminal ready. `stop all` and `lab stop` are aliases. |

The red **STOP** toolbar button calls the same global stop action.

## 8. Deadlock avoidance with Banker's algorithm

| Command | What it does | How it works and what it proves |
|---|---|---|
| `banker status` | Displays allocation, maximum, need, and available resources. | Reads the live resource-allocation matrices and calculates a safe sequence. |
| `banker safe` | Runs only the safety test. | Simulates process completion using the available-work vector and reports a complete safe sequence or an unsafe state. |
| `banker request P A B C` | Requests resources for process `P`. | Temporarily applies the request and grants it only if it is valid, available, within remaining need, and leaves the system safe. |
| `banker release P A B C` | Releases held resources. | Reduces the process allocation and returns those units to the available vector. |

Safe demonstration:

```text
banker status
banker safe
banker request 1 1 0 2
banker status
```

If a request is rejected, that is useful evidence: the kernel prevented an
invalid, unavailable, or unsafe allocation.

## 9. Main memory

| Command | What it does | How it works and what it proves |
|---|---|---|
| `mem` | Shows heap statistics. | Reads the custom first-fit allocator and reports allocated space, free space, blocks, and the largest free block. |
| `mem test` | Runs the allocator self-test. | Allocates blocks, frees them, and verifies coalescing before printing the resulting heap state. |
| `fit demo` | Compares First Fit, Best Fit, and Worst Fit. | Applies the same request to a fixed list of free holes and reports which hole each policy selects. |
| `syscall malloc N` | Allocates `N` bytes through a syscall-style alias. | Calls the real kernel allocator and stores the returned pointer in one of four visible demo slots. |
| `syscall free SLOT` | Frees a demonstration allocation. | Passes the selected pointer back to the allocator so the free list can reclaim and coalesce it. |

## 10. Virtual memory and paging

| Command | What it does | How it works and what it proves |
|---|---|---|
| `page status` | Shows page tables, frames, and counters. | Reports mappings, hits, faults, evictions, dirty write-backs, and frame ownership from the paging model. |
| `page demo` | Runs a FIFO page-reference trace. | Translates a sequence of reads and writes, loads missing pages, and replaces the oldest page when frames are full. |
| `page read P ADDRESS` | Simulates a process read. | Splits the virtual address into a page number and offset, then maps it to a physical frame. |
| `page write P ADDRESS` | Simulates a process write. | Performs translation and marks the mapped page dirty. |
| `page test` | Runs the paging self-test. | Checks hits, faults, FIFO eviction, and dirty write-back behavior. |
| `page reset` | Resets paging state. | Clears mappings, frames, and paging statistics. |
| `replace demo` | Compares FIFO, LRU, and Optimal replacement. | Runs the same reference string with three frames and reports the fault count for each algorithm. |

Processes are `0..2`, and valid demonstration addresses are `0..2047`.

## 11. MiniFS file system

MiniFS is a small kernel-managed, RAM-backed file system used by both the
terminal and graphical Files/Notes applications.

| Command | What it does | How it works and what it proves |
|---|---|---|
| `pwd` | Prints the current directory. | Reads the shell's current working-directory path. |
| `ls` | Lists the current directory. | Reads MiniFS directory entries and prints type, name, and size. `ls PATH` lists another directory. |
| `mkdir NAME` | Creates a directory. | Allocates a MiniFS directory entry after validating the path and parent directory. |
| `cd PATH` | Changes the current directory. | Resolves absolute/relative paths, including `.` and `..`, and updates the shell path only if the directory exists. |
| `rmdir PATH` | Removes an empty directory. | Refuses missing or non-empty directories. |
| `write NAME TEXT` | Creates or replaces a file. | Resolves the path and stores the text in a MiniFS file entry. |
| `cat NAME` | Reads a file. | Looks up the MiniFS entry and prints its stored contents. |
| `rm NAME` | Deletes a file. | Releases the selected MiniFS file entry. |
| `fsinfo` | Shows file-system statistics. | Reports used entries, files, directories, and commit count. |
| `fsalloc demo` | Explains file-allocation strategies. | Compares contiguous, linked, and indexed logical-to-physical block mapping. |
| `format YES` | Erases every MiniFS entry. | Reinitializes the file system. The exact uppercase confirmation is required because this is destructive. |

Suggested file proof:

```text
mkdir demo
cd demo
write proof.txt JAS OS file system works
ls
cat proof.txt
fsinfo
cd ..
```

## 12. Notes and calculator

| Command | What it does | How it works and what it proves |
|---|---|---|
| `notes` | Lists saved notes. | Ensures `/notes` exists and lists the MiniFS files shared with the graphical Notes application. `note list` is an alias. |
| `note write NAME TEXT` | Creates or replaces a note. | Stores the text as `/notes/NAME` in MiniFS. |
| `note read NAME` | Reads a note. | Loads the same MiniFS file used by the graphical editor. |
| `note append NAME TEXT` | Adds a new line to a note. | Reads the existing contents, appends text, and writes the combined result within the 1024-byte capacity. |
| `note delete NAME` | Deletes a note. | Removes the corresponding MiniFS file. |
| `calc A OP B` | Performs signed integer arithmetic. | Parses two signed 32-bit integers and supports `+`, `-`, `*`, `/`, and `%`, including division-by-zero and overflow warnings. |

Examples:

```text
note write viva My operating system notes
note append viva Scheduling and paging are implemented
note read viva
calc 25 * 4
calc -15 + 8
```

## 13. Mass storage

| Command | What it does | How it works and what it proves |
|---|---|---|
| `disk` | Opens disk-lab help. | Shows the shared request queue and available policies. `disk help` is an alias. |
| `disk demo` | Compares all disk schedulers. | Runs FCFS, SSTF, SCAN, C-SCAN, and C-LOOK with the same queue and starting head, then reports service order and total head movement. |
| `disk fcfs` | Runs FCFS. | Services requests in arrival order. |
| `disk sstf` | Runs SSTF. | Repeatedly selects the request closest to the current head. |
| `disk scan up` | Runs SCAN. | Moves toward one disk end while servicing requests, then reverses direction. `down` changes the initial direction. |
| `disk cscan up` | Runs C-SCAN. | Services in one direction and wraps to the other end for more uniform waiting time. |
| `disk clook up` | Runs C-LOOK. | Uses circular service but wraps between the last and first pending requests instead of physical disk ends. |
| `raid` | Compares RAID levels. | Explains striping, mirroring, parity, minimum disk counts, and failure tolerance for RAID 0, 1, 5, 6, and 10. |

The disk and RAID features are transparent course models; they do not claim to
be production hardware drivers.

## 14. I/O systems

| Command | What it does | How it works and what it proves |
|---|---|---|
| `io` | Opens I/O-lab help. | Lists the available device and transfer-mode demonstrations. `io help` is an alias. |
| `io status` | Shows devices and transfer counters. | Reports requests, bytes, polling checks, interrupts, and DMA transfers. |
| `io demo` | Compares polling, interrupts, and DMA. | Simulates the same 4096-byte payload with each mode and shows their different CPU involvement. |
| `io poll N` | Simulates an `N`-byte polling transfer. | The CPU repeatedly checks readiness while the transfer progresses. |
| `io interrupt N` | Simulates an interrupt-driven transfer. | Device events notify the CPU through interrupt-style completion. |
| `io dma N` | Simulates an `N`-byte DMA transfer. | Models a programmed bulk transfer plus a completion interrupt instead of per-byte CPU copying. |
| `io modes` | Explains blocking modes. | Compares blocking, nonblocking, and asynchronous I/O completion behavior. |
| `io reset` | Clears I/O statistics. | Resets the I/O model counters for a fresh comparison. |

The keyboard and PS/2 mouse use real IRQ input in the VM. The polling,
interrupt-transfer, and DMA comparison is an inspectable teaching model.

## 15. Syscall-style aliases

Typing `syscall` prints the complete alias list. These commands do not create a
user-mode ABI; they provide familiar syscall names that call the existing
kernel modules directly.

| Example | Kernel action |
|---|---|
| `syscall create` | Creates a counter task. |
| `syscall kill ID` | Terminates a task. |
| `syscall schedule rr` | Changes the scheduling policy. |
| `syscall ps` | Displays tasks. |
| `syscall timeline` | Displays recent CPU slices. |
| `syscall malloc N` | Allocates kernel heap memory. |
| `syscall free SLOT` | Frees one visible demo allocation. |
| `syscall mem` | Displays heap state. |
| `syscall sync` | Starts producer-consumer synchronization. |
| `syscall page demo` | Runs the paging trace. |
| `syscall disk` | Runs the disk demonstration. |
| `syscall io` | Runs the I/O demonstration. |
| `syscall open PATH` | Reads a MiniFS file. |
| `syscall write PATH TEXT` | Writes a MiniFS file. |
| `syscall unlink PATH` | Deletes a MiniFS file. |
| `syscall mkdir PATH` | Creates a MiniFS directory. |
| `syscall chdir PATH` | Changes the current directory. |

## 16. GUI demonstration pointer

The `pointer` command moves the guest OS cursor to a useful interface target.
It is intended for repeatable demonstrations; physical mouse input cancels the
automatic movement.

```text
pointer files
pointer notes
pointer settings
pointer clock
pointer oslab
pointer guide
pointer stop
pointer terminal
```

The command changes only the guest cursor position. It does not automatically
click the selected application or button.

## 17. Power controls

| Command | What it does | Important note |
|---|---|---|
| `reboot` | Restarts the virtual machine through the keyboard-controller reset path. | Save any demonstration notes you need before running it. |
| `shutdown` | Sends an ACPI-style power-off request supported by VirtualBox/QEMU. | The VM stops immediately. `poweroff` is an alias. |

## 18. How terminal commands work internally

The terminal is not a host program running on Windows. It is part of the JAS OS
kernel and graphical shell:

1. The PS/2 keyboard driver receives scan codes through IRQ 1.
2. The input module converts scan codes into characters and places them in a
   kernel keyboard queue.
3. The GUI collects characters in the command input buffer.
4. Pressing Enter sends the line to `commands_execute()` in
   `kernel/src/commands.c`.
5. The command dispatcher validates the text and calls the relevant kernel
   module, such as the scheduler, task manager, allocator, paging system,
   MiniFS, Banker's algorithm, storage model, or I/O model.
6. The module reads or changes kernel-managed state.
7. `kprintf()` sends formatted output to the terminal line buffer, and the GUI
   renders those lines in the terminal window.

This path is why the demonstrations are meaningful: the displayed IDs,
counters, task states, page faults, resource matrices, file entries, and
statistics come from the current kernel state.

## 19. Real implementation versus course model

During a viva, describe the scope accurately:

- **Real boot and hardware-facing code:** BIOS ISO boot, 16-bit loader,
  protected-mode transition, GDT, IDT, PIC, PIT, VESA framebuffer, PS/2 keyboard,
  PS/2 mouse, graphical shell, task table, allocator, and command interpreter.
- **Kernel implementations for course concepts:** cooperative scheduling,
  semaphores, IPC mailbox, Banker's safety algorithm, paging/frame model, and
  RAM-backed MiniFS.
- **Transparent teaching models:** disk-head scheduling, RAID comparison, and
  polling/interrupt/DMA transfer comparison.
- **Outside the project scope:** Chapter 14 onward, user-mode/ring-3 isolation,
  networking, production storage drivers, and claims of production security or
  persistence.

## 20. Troubleshooting during a demonstration

| Problem | Action |
|---|---|
| A background demo keeps printing | Type `stop` or click the red **STOP** button. |
| Output has moved off screen | Click **Up** or **Dn** in the terminal toolbar. |
| A task command fails | Run `tasks` and use the currently displayed task ID. |
| A command is rejected | Run `help`, then check the exact syntax and spaces. |
| The mailbox send fails | Run `ipc receive` first; the mailbox holds one message. |
| A Banker's request is denied | Explain that the request was invalid, unavailable, or would make the state unsafe. |
| A file cannot be found | Run `pwd` and `ls`, then use the correct relative or absolute path. |
| Too much output is visible | Run `clear`; this clears the display but does not reset kernel modules. |
| You need a clean subsystem state | Use `page reset`, `io reset`, `log clear`, or `stop`, depending on the subsystem. |

The best final proof is to run `status` again. Its changing values show that the
commands affected live kernel state rather than displaying prepared text.
