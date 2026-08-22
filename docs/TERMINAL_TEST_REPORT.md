# JAS OS Terminal Test Report

## Result

- Test date: 2026-08-22
- Environment: Oracle VirtualBox, VM `JAS OS`, booting the repository-root `jas-os.iso`
- Result: **208 passed, 0 failed**
- ISO SHA-256: `28A1E9BF74010670B09832022EF2FE6A84523097C372EA805F837D90E7E3BC58`

## What was tested

The suite entered commands through VirtualBox keyboard scan codes and checked
the kernel's COM1 output. This validates the booted ISO rather than a host-side
simulation. Coverage includes:

- command discovery, dashboard, system information, logs, history, and aliases;
- process creation and lifecycle, scheduler policies, priorities, burst times,
  CPU timeline, IPC, threads, semaphores, readers/writers, and producer/consumer;
- the global `stop` command and its aliases, while preserving the terminal shell;
- Banker's algorithm safe-state, request, release, and invalid-input paths;
- heap allocation, placement strategies, paging, page replacement, and errors;
- MiniFS navigation and file round trips, allocation display, format guard, and
  confirmed format;
- notes and calculator success/error paths;
- disk scheduling, RAID, polling, interrupts, DMA, and I/O validation;
- syscall dispatcher commands and unknown-syscall handling;
- isolated reboot, shutdown, and poweroff behavior.

Expected error cases were treated as passes only when JAS OS returned the
appropriate usage or validation message. This ensures invalid commands do not
silently appear successful.

## Reproduce the test

1. Build JAS OS with `powershell -ExecutionPolicy Bypass -File .\build.ps1`.
2. Ensure a powered-off VirtualBox VM named `JAS OS` exists.
3. Run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\test_terminal_commands.ps1
   ```

4. Read `build/terminal-command-summary.txt` and
   `build/terminal-command-results.csv`.

The script restores COM1 to disabled and powers off the test VM in its cleanup
path, including after a failed assertion.
