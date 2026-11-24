# Headless hover test (Cosmic Hotshots, UE 5.7).
#
# Runs the test track without a window at a fixed frame rate, lets the vehicle
# drop and settle, and prints the vehicle state lines from the log. Run it at
# 30, 60 and 144 FPS and compare: the numbers must match, because physics runs
# on a fixed step.
#
# Usage:  .\Scripts\run_hover_test.ps1 -Fps 60 [-Seconds 6] [-EngineDir "F:\UE 5.7\UE_5.7"]
#
# Drive test (fixed input sequence: accelerate, steer right, steer left, coast):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 7.1 -LogInterval 0.5 -ExtraCmds "ch.Test.ScriptedInput 1"
# Drift/boost test (two-button drift, ability segment, hold-to-boost; needs a full segment at start):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 7.1 -LogInterval 0.5 -ExtraCmds "ch.Test.ScriptedInput 2,ch.Test.StartBoostCharge 0.4"
# Wall hit test (impact reaction and boost loss):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 7.1 -LogInterval 0.5 -ExtraCmds "ch.Test.ScriptedInput 3,ch.Test.StartBoostCharge 1"
# Close pass test (gap to the right wall shrinks, charging rises, then a touch locks it out):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 8.1 -LogInterval 0.25 -ExtraCmds "ch.Test.ScriptedInput 4,ch.Test.StartBoostCharge 0"
# The Unreal editor must be closed.

param(
    [int]$Fps = 60,
    [float]$Seconds = 6,
    [string]$EngineDir = "F:\UE 5.7\UE_5.7",
    [string]$Map = "/Game/Maps/TestTrack",
    [float]$LogInterval = 0.25,
    [string]$ExtraCmds = ""
)

$project = Join-Path (Split-Path $PSScriptRoot -Parent) "CosmicHotshotsGame.uproject"
$logDir = Join-Path (Split-Path $PSScriptRoot -Parent) "Saved\Logs"
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir "HoverTest_$Fps.log"
if (Test-Path $log) { Remove-Item $log }

# p.MaxPhysicsDeltaTime: the engine default (1/30 s) would slow the game down at exactly 30 FPS.
$cmds = "ch.Debug.VehicleLog $LogInterval,ch.Test.AutoQuitTime $Seconds,p.MaxPhysicsDeltaTime 0.05"
if ($ExtraCmds) { $cmds = "$cmds,$ExtraCmds" }

$exe = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$arguments = @(
    "`"$project`"", $Map, "-game", "-nullrhi", "-unattended", "-nosplash", "-nosound",
    "-benchmark", "-fps=$Fps", "-ExecCmds=`"$cmds`"", "-abslog=`"$log`""
)

$process = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(240000)) {
    $process.Kill()
    Write-Output "TIMEOUT: process killed"
}
else {
    Write-Output "Exit code: $($process.ExitCode)"
}

if (Test-Path $log) {
    Select-String -Path $log -Pattern "LogCosmicVehicle|Assertion failed|Ensure condition failed|Fatal error" |
        ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[\s*\d+\]', '' }
}
else {
    Write-Output "No log written: $log"
}
