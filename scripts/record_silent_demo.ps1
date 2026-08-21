$ErrorActionPreference = 'Stop'

$vbox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
$vm = 'JAS OS'
$repo = Split-Path -Parent $PSScriptRoot
$output = Join-Path $repo 'docs\JAS-OS-Silent-Terminal-Demo.webm'
$screenOutput = Join-Path $repo 'docs\JAS-OS-Silent-Terminal-Demo-screen0.webm'
$scan = @{
    'a'='1e'; 'b'='30'; 'c'='2e'; 'd'='20'; 'e'='12'; 'f'='21'; 'g'='22';
    'h'='23'; 'i'='17'; 'j'='24'; 'k'='25'; 'l'='26'; 'm'='32'; 'n'='31';
    'o'='18'; 'p'='19'; 'q'='10'; 'r'='13'; 's'='1f'; 't'='14'; 'u'='16';
    'v'='2f'; 'w'='11'; 'x'='2d'; 'y'='15'; 'z'='2c'; ' '='39'; '/'='35';
    '1'='02'; '2'='03'; '3'='04'; '4'='05'; '5'='06'; '6'='07'; '7'='08';
    '8'='09'; '9'='0a'; '0'='0b'
}

function Send-Key([string]$make) {
    $value = [Convert]::ToInt32($make, 16)
    & $vbox controlvm $vm keyboardputscancode ('{0:x2}' -f $value) ('{0:x2}' -f ($value + 128)) | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Keyboard injection failed for scan code $make." }
}

function Type-Command([string]$text) {
    foreach ($char in $text.ToCharArray()) {
        $key = [string]$char
        if (-not $scan.ContainsKey($key)) { throw "No scan code is defined for '$key'." }
        Send-Key $scan[$key]
        Start-Sleep -Milliseconds 85
    }
    Send-Key '1c'
}

$running = & $vbox list runningvms
if ($running -match '"JAS OS"') {
    & $vbox controlvm $vm poweroff | Out-Null
    Start-Sleep -Seconds 2
}

foreach ($path in @($output, $screenOutput, $mp4Output)) {
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
}

& $vbox modifyvm $vm --recording off
& $vbox modifyvm $vm --recording-file $output
& $vbox modifyvm $vm --recording-screens 0
& $vbox modifyvm $vm --recording-video-res 1024x768
& $vbox modifyvm $vm --recording-video-rate 2200
& $vbox modifyvm $vm --recording-video-fps 30
& $vbox modifyvm $vm --recording-max-time 300
& $vbox modifyvm $vm --recording on

& $vbox startvm $vm --type gui | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Could not start the JAS OS VM.' }
Start-Sleep -Seconds 8
Start-Sleep -Seconds 3

$scenes = @(
    @('guide',     11, 'guide'),
    @('present 1', 11, 'files'),
    @('present 3', 11, 'terminal'),
    @('present 5', 11, 'guide'),
    @('present 6',  8, 'stop'),
    @('rw demo',    8, 'terminal'),
    @('stop',       9, 'stop'),
    @('present 7', 11, 'settings'),
    @('present 9', 11, 'terminal'),
    @('present 10',11, 'files'),
    @('present 12',11, 'oslab'),
    @('present 13',11, 'terminal'),
    @('dashboard', 10, 'oslab'),
    @('about',      9, 'notes'),
    @('help',      10, 'guide')
)

foreach ($scene in $scenes) {
    Type-Command "pointer $($scene[2])"
    Start-Sleep -Seconds 2
    Type-Command $scene[0]
    Start-Sleep -Seconds $scene[1]
}

Start-Sleep -Seconds 4
& $vbox controlvm $vm recording stop | Out-Null
& $vbox controlvm $vm poweroff | Out-Null
for ($attempt = 0; $attempt -lt 20; $attempt++) {
    if (-not ((& $vbox list runningvms) -match '"JAS OS"')) { break }
    Start-Sleep -Milliseconds 500
}
Start-Sleep -Seconds 2

for ($attempt = 0; $attempt -lt 60; $attempt++) {
    if (Test-Path -LiteralPath $screenOutput) {
        try {
            $stream = [System.IO.File]::Open($screenOutput, 'Open', 'ReadWrite', 'None')
            $stream.Close()
            break
        } catch {
            Start-Sleep -Milliseconds 500
        }
    } else {
        Start-Sleep -Milliseconds 500
    }
}

& $vbox modifyvm $vm --recording off | Out-Null

if (-not (Test-Path -LiteralPath $screenOutput)) { throw 'VirtualBox did not create the recording.' }
Move-Item -LiteralPath $screenOutput -Destination $output

Get-Item -LiteralPath $output -ErrorAction SilentlyContinue |
    Select-Object FullName, Length, LastWriteTime
