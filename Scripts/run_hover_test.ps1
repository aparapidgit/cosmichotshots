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
# Overtake test (straight past a dummy 30 m ahead, 5 m to the left: exactly one OVERTAKE line):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 7.1 -LogInterval 0.5 -ExtraCmds "ch.Test.ScriptedInput 5,ch.Test.SpawnDummyAhead 3000,ch.Test.DummyLateralOffset -500"
# Lap test (test autopilot drives one lap: checkpoints, LAP, FINISH, RACE_FINISHED):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 90 -LogInterval 5 -TimeoutSeconds 900 -ExtraCmds "ch.Test.ScriptedInput 6,ch.Race.Laps 1"
# Race with test bots (player on autopilot + 7 bots, each with its own line and throttle; overtakes, contact, results):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 200 -LogInterval 30 -TimeoutSeconds 900 -ExtraCmds "ch.Test.ScriptedInput 6,ch.Race.Laps 2,ch.Test.Bots 7"
#   IMPACT lines are written once per frame, so their ORDER among the other lines differs between frame rates;
#   compare sorted logs. The lines themselves must match.
# Modifier stack on its own (max speed x1.2 at 2 s: 1 s fade in, 2 s hold, 1 s fade out; MODS lines):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 7.1 -LogInterval 0.5 -ExtraCmds "ch.Test.ScriptedInput 5,ch.Test.SpeedModifier 1.2"
# Spark passive (roster index 2) from the last grid slot against 3 bots (PASSIVE lines: on while last, off after an overtake):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 110 -LogInterval 10 -TimeoutSeconds 900 -ExtraCmds "ch.Test.ScriptedInput 6,ch.Race.Laps 1,ch.Test.Bots 3,ch.Test.PlayerCharacter 2,ch.Test.PlayerGridSlot 7"
# Sulgwen passive (roster index 3) from the last grid slot against 5 bots (chain level rises with overtakes, ends after 4 s):
#         .\Scripts\run_hover_test.ps1 -Fps 60 -Seconds 110 -LogInterval 10 -TimeoutSeconds 900 -ExtraCmds "ch.Test.ScriptedInput 6,ch.Race.Laps 1,ch.Test.Bots 5,ch.Test.PlayerCharacter 3,ch.Test.PlayerGridSlot 7"
#   Add "ch.Passives.Enabled 0" to switch every passive off.
# Roster indices: 0 TestLight, 1 TestHeavy, 2 Spark, 3 Sulgwen, 4 Freya, 5 Killjoke, 6 Glick, 7 Wilqis, 8 Slot7, 9 Slot8.
# Other passives ($L = "ch.Test.ScriptedInput 6,ch.Race.Laps 1"; use -Seconds 100 -LogInterval 10 -TimeoutSeconds 900):
#   Wilqis   (gap to the car ahead):      "$L,ch.Test.Bots 3,ch.Test.BotCharacter 0,ch.Test.PlayerGridSlot 7,ch.Test.PlayerCharacter 7"
#   Killjoke (leader pulls away, as bots): "$L,ch.Test.Bots 7,ch.Test.BotCharacter 5,ch.Test.PlayerCharacter 0"
#   Freya    (close overtakes, as bots):   "$L,ch.Test.Bots 7,ch.Test.BotCharacter 4,ch.Test.PlayerCharacter 0"   (MODS maxSpeed=0.93 on the overtaken car)
#   Slot8    (top 3 of 6, from pole):      "$L,ch.Test.Bots 5,ch.Test.BotCharacter 0,ch.Test.PlayerCharacter 9"
# Side hit into a dummy turned side on (-Seconds 5 -LogInterval 0.5); compare the IMPACT speedChange with "ch.Passives.Enabled 0":
#   Glick as the dummy:      "ch.Test.ScriptedInput 5,ch.Test.SpawnDummyAhead 2500,ch.Test.DummyYawOffset 90,ch.Test.PlayerCharacter 0,ch.Test.DummyCharacter 6"
#   Slot8 2nd of 2 drivers:  "ch.Test.ScriptedInput 5,ch.Test.SpawnDummyAhead 4000,ch.Test.DummyYawOffset 90,ch.Test.PlayerCharacter 9,ch.Test.DummyCharacter 0"
# Slot7 drift chain (one counted drift, then a wall hit resets it; -Seconds 45 -LogInterval 1):
#   "ch.Test.ScriptedInput 8,ch.Test.PlayerCharacter 8"
# Computer drivers (ARaceAIController; difficulty 0 Easy, 1 Normal, 2 Hard; ch.AI.RubberBanding 0 switches rubber banding off):
#   Full grid, 3 laps:   -Seconds 200 -LogInterval 20 -TimeoutSeconds 900 -ExtraCmds "ch.Test.ScriptedInput 6,ch.Race.Laps 3,ch.Race.AIOpponents 7,ch.Race.AIDifficulty 1"
#   Compare difficulties: -Seconds 80 -LogInterval 10 -ExtraCmds "ch.Test.ScriptedInput 6,ch.Race.Laps 1,ch.Race.AIOpponents 1,ch.Race.AIDifficulty 2,ch.AI.RubberBanding 0"
# Item boxes (placeholder item "Empty": ITEM PICKUP / ROLL_DONE / USE / REPLACE / RESPAWN lines; scripted mode 9 presses use every 8 s):
#   -Seconds 135 -LogInterval 20 -TimeoutSeconds 900 -ExtraCmds "ch.Test.ScriptedInput 9,ch.Race.Laps 2,ch.Race.AIOpponents 7,ch.Test.PlayerGridSlot 7"
#   "ch.Race.ItemBoxes 0" removes the boxes. Loot table check: -ExecOnly "ch.Item.PrintLoot 8"
# Race lines (LogCosmicRace) carry the race time of the physics step and must match at every frame rate too.
# The Unreal editor must be closed.

param(
    [int]$Fps = 60,
    [float]$Seconds = 6,
    [string]$EngineDir = "F:\UE 5.7\UE_5.7",
    [string]$Map = "/Game/Maps/TestTrack",
    [float]$LogInterval = 0.25,
    [string]$ExtraCmds = "",
    # Console COMMANDS (not variables) to run once the game is up, e.g. "ch.Item.PrintLoot 8". Not passed through -DPCVars.
    [string]$ExecOnly = "",
    # Wall-clock limit for the whole run; a full-lap test needs more than the default.
    [int]$TimeoutSeconds = 240
)

$project = Join-Path (Split-Path $PSScriptRoot -Parent) "CosmicHotshotsGame.uproject"
$logDir = Join-Path (Split-Path $PSScriptRoot -Parent) "Saved\Logs"
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir "HoverTest_$Fps.log"
if (Test-Path $log) { Remove-Item $log }

# p.MaxPhysicsDeltaTime: the engine default (1/30 s) would slow the game down at exactly 30 FPS.
$cmds = "ch.Debug.VehicleLog $LogInterval,ch.Test.AutoQuitTime $Seconds,p.MaxPhysicsDeltaTime 0.05"
if ($ExtraCmds) { $cmds = "$cmds,$ExtraCmds" }
if ($ExecOnly) { $cmds = "$cmds,$ExecOnly" }

$exe = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$arguments = @(
    "`"$project`"", $Map, "-game", "-nullrhi", "-unattended", "-nosplash", "-nosound",
    "-benchmark", "-fps=$Fps", "-ExecCmds=`"$cmds`"", "-abslog=`"$log`""
)
# -ExecCmds only run at the END of the first frame, after that frame's physics steps (how many
# depends on the frame rate). The test console variables are also set at startup through
# -DPCVars, so helpers that act in the first frame (test dummy) see them at every frame rate.
if ($ExtraCmds) {
    $startupCvars = ($ExtraCmds -split ',' | ForEach-Object { $_.Trim() -replace '\s+', '=' }) -join ','
    $arguments += "-DPCVars=`"$startupCvars`""
}

$process = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    $process.Kill()
    Write-Output "TIMEOUT: process killed"
}
else {
    Write-Output "Exit code: $($process.ExitCode)"
}

if (Test-Path $log) {
    Select-String -Path $log -Pattern "LogCosmicVehicle|LogCosmicRace|Assertion failed|Ensure condition failed|Fatal error" |
        ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[\s*\d+\]', '' }
}
else {
    Write-Output "No log written: $log"
}
