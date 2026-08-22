param(
    [string]$VmName = 'JAS OS',
    [string]$VBoxManage = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$build = Join-Path $repo 'build'
$mainLog = Join-Path $build 'terminal-command-test.log'
$resultsPath = Join-Path $build 'terminal-command-results.csv'
$summaryPath = Join-Path $build 'terminal-command-summary.txt'
$iso = Join-Path $repo 'jas-os.iso'

if (-not (Test-Path -LiteralPath $VBoxManage)) { throw 'VBoxManage was not found.' }
if (-not (Test-Path -LiteralPath $iso)) { throw 'jas-os.iso was not found. Run build.ps1 first.' }
New-Item -ItemType Directory -Force $build | Out-Null

$scan = @{
    'a'='1e'; 'b'='30'; 'c'='2e'; 'd'='20'; 'e'='12'; 'f'='21'; 'g'='22';
    'h'='23'; 'i'='17'; 'j'='24'; 'k'='25'; 'l'='26'; 'm'='32'; 'n'='31';
    'o'='18'; 'p'='19'; 'q'='10'; 'r'='13'; 's'='1f'; 't'='14'; 'u'='16';
    'v'='2f'; 'w'='11'; 'x'='2d'; 'y'='15'; 'z'='2c'; ' '='39'; '/'='35';
    '1'='02'; '2'='03'; '3'='04'; '4'='05'; '5'='06'; '6'='07';
    '7'='08'; '8'='09'; '9'='0a'; '0'='0b'; '-'='0c'; '='='0d';
    '.'='34'; ','='33'; '*'='37'
}

function Send-ScanBatch(
    [System.Collections.Generic.List[string]]$batch,
    [bool]$allowVmTransition = $false
) {
    if ($batch.Count -eq 0) { return }
    $injectionOutput = & $VBoxManage controlvm $VmName keyboardputscancode @($batch) 2>&1
    if ($LASTEXITCODE -ne 0 -and -not $allowVmTransition) {
        throw "VirtualBox keyboard injection failed: $injectionOutput"
    }
    $batch.Clear()
    Start-Sleep -Milliseconds 35
}

function Add-Key([System.Collections.Generic.List[string]]$batch, [string]$make, [bool]$shift) {
    $value = [Convert]::ToInt32($make, 16)
    if ($shift) { $batch.Add('2a') }
    $batch.Add(('{0:x2}' -f $value))
    $batch.Add(('{0:x2}' -f ($value + 128)))
    if ($shift) { $batch.Add('aa') }
    if ($batch.Count -ge 20) { Send-ScanBatch $batch }
}

function Send-Command(
    [string]$command,
    [int]$delayMs = 220,
    [bool]$allowVmTransition = $false
) {
    $batch = [System.Collections.Generic.List[string]]::new()
    foreach ($character in $command.ToCharArray()) {
        $text = [string]$character
        $shift = $false
        $key = $text
        if ([char]::IsUpper($character)) {
            $key = $text.ToLowerInvariant()
            $shift = $true
        } elseif ($text -eq '_') {
            $key = '-'; $shift = $true
        } elseif ($text -eq '+') {
            $key = '='; $shift = $true
        } elseif ($text -eq '%') {
            $key = '5'; $shift = $true
        } elseif ($text -eq '?') {
            $key = '/'; $shift = $true
        }
        if (-not $scan.ContainsKey($key)) { throw "No scan code for '$text' in: $command" }
        Add-Key $batch $scan[$key] $shift
    }
    Add-Key $batch '1c' $false
    Send-ScanBatch $batch $allowVmTransition
    Start-Sleep -Milliseconds $delayMs
}

$tests = [System.Collections.Generic.List[object]]::new()
function Add-Test([string]$command, [string]$expect, [int]$delay = 220) {
    $tests.Add([PSCustomObject]@{ Command=$command; Expect=$expect; Delay=$delay })
}

# Discovery, aliases, GUI pointer targets, and terminal-only history.
Add-Test 'help' 'COMMAND CENTER'
Add-Test '?' 'COMMAND CENTER'
Add-Test 'commands' 'COMMAND CENTER'
Add-Test 'quickstart' 'QUICK START'
Add-Test 'start here' 'QUICK START'
Add-Test 'guide' 'FEATURE GUIDE'
Add-Test 'modules' 'KERNEL MODULE MAP'
Add-Test 'module map' 'KERNEL MODULE MAP'
Add-Test 'services' 'OPERATING-SYSTEM SERVICES'
Add-Test 'boot' 'X86 BOOT PATH'
Add-Test 'about' 'JAS OS - X86 KERNEL'
Add-Test 'sysinfo' 'SYSTEM INFORMATION'
Add-Test 'dashboard' 'LIVE KERNEL DASHBOARD'
Add-Test 'status' 'LIVE KERNEL DASHBOARD'
Add-Test 'home' 'LIVE KERNEL DASHBOARD'
Add-Test 'log' 'KERNEL EVENT LOG'
Add-Test 'log clear' 'Kernel event log cleared'
foreach ($target in 'files','notes','settings','clock','oslab','guide','stop','terminal') {
    Add-Test "pointer $target" "Moving to $target"
}
Add-Test 'pointer nowhere' 'Usage: pointer guide'
Add-Test 'history' 'RECENT COMMANDS'

# Scheduling and task lifecycle. The shell is ID 1; the first counter is ID 2.
Add-Test 'schedule status' 'CPU SCHEDULER'
Add-Test 'schedule rr' 'Scheduler policy changed'
Add-Test 'schedule roundrobin' 'Scheduler policy changed'
Add-Test 'schedule priority' 'Scheduler policy changed'
Add-Test 'schedule prio' 'Scheduler policy changed'
Add-Test 'schedule fcfs' 'Scheduler policy changed'
Add-Test 'schedule sjf' 'Scheduler policy changed'
Add-Test 'rr' 'Scheduler policy changed'
Add-Test 'prio' 'Scheduler policy changed'
Add-Test 'fcfs' 'Scheduler policy changed'
Add-Test 'sjf' 'Scheduler policy changed'
Add-Test 'create counter' 'Counter task 2 created'
Add-Test 'tasks' 'PROCESS MONITOR'
Add-Test 'ps' 'PROCESS MONITOR'
Add-Test 'taskmgr' 'PROCESS MONITOR'
Add-Test 'priority 2 9' 'Task priority updated'
Add-Test 'burst 2 1' 'Task burst estimate updated'
Add-Test 'burst 999 0' 'Usage: burst'
Add-Test 'suspend 2' 'Task moved to SUSPENDED'
Add-Test 'resume 2' 'Task moved to READY'
Add-Test 'ticks' 'Scheduler has completed'
Add-Test 'timeline' 'CPU timeline (last 24 slices):'
Add-Test 'pause' 'All application tasks are frozen'
Add-Test 'resume all' 'All paused application tasks may run again'
Add-Test 'pause all' 'All application tasks are frozen'
Add-Test 'resume all' 'All paused application tasks may run again'
Add-Test 'suspend 1' 'shell cannot suspend itself'
Add-Test 'kill 1' 'shell cannot terminate itself'
Add-Test 'kill 2' 'Task moved to FINISHED'
Add-Test 'suspend 999' 'Task not found'
Add-Test 'resume 999' 'Task is not suspended'
Add-Test 'kill 999' 'Task not found'

# IPC, threads, synchronization labs, and every stop path.
Add-Test 'ipc' 'IPC MAILBOX'
Add-Test 'ipc status' 'IPC MAILBOX'
Add-Test 'ipc send hello' 'Message placed in the kernel mailbox'
Add-Test 'ipc send second' 'mailbox full'
Add-Test 'ipc receive' '[RECV] hello'
Add-Test 'ipc receive' 'mailbox is empty'
Add-Test 'ipc demo' 'IPC MESSAGE-PASSING TRACE'
Add-Test 'ipc nonsense' 'Usage: ipc status'
Add-Test 'thread' 'THREAD LAB'
Add-Test 'thread demo' 'Two schedulable workers' 500
Add-Test 'thread status' 'THREAD LAB'
Add-Test 'thread stop' 'Thread workers stopped'
Add-Test 'thread nonsense' 'Usage: thread demo'
Add-Test 'thread start' 'Two schedulable workers' 500
Add-Test 'thread stop' 'Thread workers stopped'
Add-Test 'rw' 'READERS-WRITERS'
Add-Test 'rw demo' 'Two readers and one writer started' 500
Add-Test 'rw status' 'READERS-WRITERS'
Add-Test 'rw stop' 'Readers-writers tasks stopped'
Add-Test 'rw nonsense' 'Usage: rw demo'
Add-Test 'rw start' 'Two readers and one writer started' 500
Add-Test 'rw stop' 'Readers-writers tasks stopped'
Add-Test 'start' 'Producer-consumer pipeline started' 500
Add-Test 'demo status' 'SYNCHRONIZATION MONITOR'
Add-Test 'hold' 'Producer-consumer demo paused'
Add-Test 'continue' 'Producer-consumer demo resumed'
Add-Test 'demo stop' 'Producer-consumer demo stopped'
Add-Test 'demo start' 'Producer-consumer pipeline started' 500
Add-Test 'demo pause' 'Producer-consumer demo paused'
Add-Test 'demo resume' 'Producer-consumer demo resumed'
Add-Test 'demo stop' 'Producer-consumer demo stopped'
Add-Test 'demo producer' 'Producer-consumer pipeline started' 500
Add-Test 'demo stop' 'Producer-consumer demo stopped'
Add-Test 'stop' 'STOP complete'
Add-Test 'stop all' 'STOP complete'
Add-Test 'lab stop' 'STOP complete'

# Deadlock avoidance, memory, and paging.
Add-Test 'banker status' "BANKER'S ALGORITHM"
Add-Test 'banker' 'Usage: banker status'
Add-Test 'banker safe' 'SAFETY CHECK'
Add-Test 'banker request 1 1 0 2' 'Resource request granted'
Add-Test 'banker release 1 1 0 2' 'Resources released'
Add-Test 'banker request 9 0 0 0' 'Usage: banker request'
Add-Test 'banker release 1 9 9 9' 'Release denied'
Add-Test 'mem' 'CUSTOM KERNEL HEAP'
Add-Test 'mem test' 'Allocator test passed'
Add-Test 'fit demo' 'CONTIGUOUS-ALLOCATION FIT'
Add-Test 'replace demo' 'PAGE-REPLACEMENT COMPARISON'
Add-Test 'page status' 'VIRTUAL MEMORY PAGING'
Add-Test 'page' 'Usage: page status'
Add-Test 'page reset' 'Page tables, frames, and paging statistics reset'
Add-Test 'page read 0 42' 'ADDRESS TRANSLATION'
Add-Test 'page write 1 300' '[DIRTY]'
Add-Test 'page demo' 'FIFO PAGE-REFERENCE TRACE'
Add-Test 'page test' 'Paging test passed'
Add-Test 'page read 9 0' 'Invalid process/address'
Add-Test 'page nonsense' 'Usage: page status'

# MiniFS, Notes, and calculator paths including cleanup and validation.
Add-Test 'pwd' '/'
Add-Test 'ls' 'MiniFS v2 persistent directory'
Add-Test 'dir' 'MiniFS v2 persistent directory'
Add-Test 'mkdir demo' 'Directory created'
Add-Test 'cd demo' 'Current directory changed'
Add-Test 'pwd' '/demo'
Add-Test 'write proof.txt hello' 'File written'
Add-Test 'cat proof.txt' 'hello'
Add-Test 'ls .' 'proof.txt'
Add-Test 'fsinfo' 'MINIFS STORAGE'
Add-Test 'rmdir .' 'rmdir failed'
Add-Test 'rm proof.txt' 'File deleted'
Add-Test 'cd ..' 'Current directory changed'
Add-Test 'rmdir demo' 'Empty directory removed'
Add-Test 'cat missing' 'File not found'
Add-Test 'fsalloc demo' 'FILE-ALLOCATION METHODS'
Add-Test 'notes' 'NOTES'
Add-Test 'note' 'Usage: notes'
Add-Test 'note list' 'NOTES'
Add-Test 'note write viva first' 'Note saved'
Add-Test 'note append viva second' 'Note appended'
Add-Test 'note read viva' 'first'
Add-Test 'note delete viva' 'Note deleted'
Add-Test 'note read viva' 'Note not found'
Add-Test 'calc 25 * 4' '25 * 4 = 100'
Add-Test 'calc' 'Usage: calc'
Add-Test 'calc -15 + 8' '-15 + 8 = -7'
Add-Test 'calc 10 / 0' 'Division by zero'
Add-Test 'calc 7 % 3' '7 % 3 = 1'
Add-Test 'calc bad' 'Usage: calc'

# Storage and I/O command families.
Add-Test 'disk' 'MASS-STORAGE LAB'
Add-Test 'disk help' 'MASS-STORAGE LAB'
Add-Test 'disk demo' 'DISK-SCHEDULING COMPARISON'
Add-Test 'disk fcfs' 'DISK SCHEDULE'
Add-Test 'disk sstf' 'DISK SCHEDULE'
Add-Test 'disk scan' 'DISK SCHEDULE'
Add-Test 'disk scan down' 'Initial direction: toward cylinder 0'
Add-Test 'disk cscan up' 'DISK SCHEDULE'
Add-Test 'disk c-scan down' 'DISK SCHEDULE'
Add-Test 'disk clook up' 'DISK SCHEDULE'
Add-Test 'disk c-look down' 'DISK SCHEDULE'
Add-Test 'disk scan sideways' "Direction must be 'up' or 'down'"
Add-Test 'raid' 'RAID STRUCTURE'
Add-Test 'io' 'I/O SYSTEMS LAB'
Add-Test 'io help' 'I/O SYSTEMS LAB'
Add-Test 'io status' 'KERNEL I/O SUBSYSTEM'
Add-Test 'io reset' 'I/O counters reset'
Add-Test 'io modes' 'I/O COMPLETION MODES'
Add-Test 'io demo' '4096-BYTE I/O TRANSFER'
Add-Test 'io poll 64' '64-byte polling transfer completed'
Add-Test 'io interrupt 128' '128-byte interrupt transfer completed'
Add-Test 'io dma 256' '256-byte DMA transfer completed'
Add-Test 'io poll 0' 'Transfer size must be'
Add-Test 'io nonsense' 'Usage: io demo'

# Syscall-style aliases, including file and allocator round trips.
Add-Test 'syscall' 'SYSCALL ALIASES'
Add-Test 'syscall create' 'Counter task'
Add-Test 'syscall kill 999' 'Task not found'
Add-Test 'syscall schedule rr' 'Scheduler policy changed'
Add-Test 'syscall ps' 'PROCESS MONITOR'
Add-Test 'syscall tasks' 'PROCESS MONITOR'
Add-Test 'syscall timeline' 'CPU timeline (last 24 slices):'
Add-Test 'syscall mem' 'CUSTOM KERNEL HEAP'
Add-Test 'syscall malloc 64' 'malloc(64) -> slot 0'
Add-Test 'syscall free 0' 'freed slot 0'
Add-Test 'syscall malloc 0' 'Usage: syscall malloc'
Add-Test 'syscall free 3' 'Usage: syscall free'
Add-Test 'syscall banker_request 1 0 0 0' 'Resource request granted'
Add-Test 'syscall banker_release 1 0 0 0' 'Resources released'
Add-Test 'syscall sync' 'Producer-consumer pipeline started' 500
Add-Test 'stop' 'STOP complete'
Add-Test 'syscall page demo' 'FIFO PAGE-REFERENCE TRACE'
Add-Test 'syscall page' 'FIFO PAGE-REFERENCE TRACE'
Add-Test 'syscall disk' 'DISK-SCHEDULING COMPARISON'
Add-Test 'syscall io' '4096-BYTE I/O TRANSFER'
Add-Test 'syscall mkdir api' 'Directory created'
Add-Test 'syscall chdir api' 'Current directory changed'
Add-Test 'syscall write proof works' 'File written'
Add-Test 'syscall read proof' 'works'
Add-Test 'syscall open proof' 'works'
Add-Test 'syscall unlink proof' 'File deleted'
Add-Test 'syscall chdir ..' 'Current directory changed'
Add-Test 'rmdir api' 'Empty directory removed'
Add-Test 'syscall unknown' 'Unknown syscall'

# General validation, aliases, clear behavior, and the guarded format command.
Add-Test 'schedule nonsense' 'Usage: schedule rr'
Add-Test 'priority 999 20' 'Usage: priority'
Add-Test 'format yes' 'Type exactly: format YES'
Add-Test 'format YES' 'MiniFS formatted'
Add-Test 'unknowncommand' 'Command not found: unknowncommand'
Add-Test 'clear' ''
Add-Test 'cls' ''

function Set-SerialLog([string]$path) {
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
    & $VBoxManage modifyvm $VmName --uart1 0x3F8 4 --uartmode1 file $path | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not configure the VM serial log.' }
}

function Wait-ForPowerState([string]$state, [int]$seconds) {
    for ($i = 0; $i -lt $seconds * 4; ++$i) {
        $info = & $VBoxManage showvminfo $VmName --machinereadable
        if ($info -match "VMState=`"$state`"") { return $true }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

function Start-TestVm([string]$logPath) {
    if (-not (Wait-ForPowerState 'poweroff' 12)) {
        throw 'The test VM did not reach the powered-off state before startup.'
    }
    # VirtualBox can report poweroff slightly before releasing the prior direct
    # console session. Give that session time to close before reconfiguration.
    Start-Sleep -Seconds 1
    Set-SerialLog $logPath
    & $VBoxManage storageattach $VmName --storagectl IDE --port 0 --device 0 `
        --type dvddrive --medium $iso | Out-Null
    & $VBoxManage startvm $VmName --type headless | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not start the test VM.' }
    if (-not (Wait-ForPowerState 'running' 12)) { throw 'The test VM did not start.' }
    for ($i = 0; $i -lt 60; ++$i) {
        if (Test-Path -LiteralPath $logPath) {
            $bootOutput = Get-Content -LiteralPath $logPath -Raw
            if ($bootOutput -match 'Keys: Up/Down history.*Ctrl\+L clear') {
                # The guest discards the first editing-key make code after a fresh
                # boot. Two Backspaces on an empty prompt safely initialize that
                # path so the first command is not changed from "help" to "elp".
                & $VBoxManage controlvm $VmName keyboardputscancode 0e 8e 0e 8e | Out-Null
                if ($LASTEXITCODE -ne 0) { throw 'Could not warm the VM keyboard.' }
                Start-Sleep -Milliseconds 250
                return
            }
        }
        Start-Sleep -Milliseconds 250
    }
    throw 'JAS OS did not reach its terminal prompt within 15 seconds.'
}

function Stop-TestVm {
    $info = & $VBoxManage showvminfo $VmName --machinereadable
    if ($info -match 'VMState="running"') {
        & $VBoxManage controlvm $VmName poweroff | Out-Null
        [void](Wait-ForPowerState 'poweroff' 8)
    }
}

$results = [System.Collections.Generic.List[object]]::new()
try {
    Stop-TestVm
    Start-TestVm $mainLog
    foreach ($test in $tests) {
        Send-Command $test.Command $test.Delay
    }
    Start-Sleep -Seconds 2
    Stop-TestVm

    $text = (Get-Content -LiteralPath $mainLog -Raw).Replace("`r`n", "`n")
    $matches = [regex]::Matches($text, '(?m)^[^\n]* \$ (.+)$')
    $cursor = 0
    foreach ($test in $tests) {
        $found = -1
        for ($i = $cursor; $i -lt $matches.Count; ++$i) {
            if ($matches[$i].Groups[1].Value.TrimEnd("`r") -ceq $test.Command) {
                $found = $i
                break
            }
        }
        if ($found -lt 0) {
            $results.Add([PSCustomObject]@{Command=$test.Command;Passed=$false;Expected=$test.Expect;Observed='prompt marker missing'})
            continue
        }
        $end = if ($found + 1 -lt $matches.Count) { $matches[$found + 1].Index } else { $text.Length }
        $segment = $text.Substring($matches[$found].Index, $end - $matches[$found].Index)
        $passed = -not $test.Expect -or $segment.IndexOf($test.Expect, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        $observed = ($segment -replace '\s+', ' ').Trim()
        if ($observed.Length -gt 180) { $observed = $observed.Substring(0, 180) }
        $results.Add([PSCustomObject]@{Command=$test.Command;Passed=$passed;Expected=$test.Expect;Observed=$observed})
        $cursor = $found + 1
    }

    # Power controls run in isolated boots because each intentionally resets/stops the VM.
    $rebootLog = Join-Path $build 'terminal-reboot-test.log'
    Start-TestVm $rebootLog
    Send-Command 'reboot' 100 $true
    $rebooted = $false
    for ($i = 0; $i -lt 60; ++$i) {
        if (Test-Path -LiteralPath $rebootLog) {
            $currentRebootText = Get-Content -LiteralPath $rebootLog -Raw
            if (([regex]::Matches($currentRebootText, 'JAS OS x86 booting')).Count -ge 2) {
                $rebooted = $true
                break
            }
        }
        Start-Sleep -Milliseconds 250
    }
    Stop-TestVm
    $rebootText = if (Test-Path $rebootLog) { Get-Content $rebootLog -Raw } else { '' }
    $bootCount = ([regex]::Matches($rebootText, 'JAS OS x86 booting')).Count
    $results.Add([PSCustomObject]@{Command='reboot';Passed=($rebooted -and $bootCount -ge 2);Expected='second kernel boot';Observed="boot count=$bootCount"})

    foreach ($powerCommand in 'shutdown','poweroff') {
        $powerLog = Join-Path $build "terminal-$powerCommand-test.log"
        Start-TestVm $powerLog
        Send-Command $powerCommand 100 $true
        $poweredOff = Wait-ForPowerState 'poweroff' 10
        $powerText = if (Test-Path $powerLog) { Get-Content $powerLog -Raw } else { '' }
        $results.Add([PSCustomObject]@{Command=$powerCommand;Passed=($poweredOff -and $powerText -match 'Shutting down');Expected='VM poweroff';Observed="state poweroff=$poweredOff"})
        Stop-TestVm
    }
} finally {
    Stop-TestVm
    & $VBoxManage modifyvm $VmName --uart1 off | Out-Null
}

$results | Export-Csv -LiteralPath $resultsPath -NoTypeInformation
$passedCount = @($results | Where-Object Passed).Count
$failed = @($results | Where-Object { -not $_.Passed })
$lines = @(
    'JAS OS terminal command regression suite',
    "Passed: $passedCount",
    "Failed: $($failed.Count)",
    "Total:  $($results.Count)",
    ''
)
if ($failed.Count) {
    $lines += 'Failures:'
    foreach ($failure in $failed) {
        $lines += "- $($failure.Command) | expected: $($failure.Expected) | observed: $($failure.Observed)"
    }
} else {
    $lines += 'All command and power-control checks passed.'
}
$lines | Set-Content -LiteralPath $summaryPath
$lines | ForEach-Object { Write-Host $_ }
if ($failed.Count) { exit 1 }
