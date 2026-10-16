param(
    [string]$Exe = "build-vk-on\Debug\dethrace.exe",
    [string]$GameDir = "D:\SteamLibrary\steamapps\common\Carmageddon1\CARMA",
    [string]$LogDir = "",
    [int]$MenuWaitSeconds = 12,
    [int]$RaceWaitSeconds = 20,
    [int]$ClickHoldMs = 500,
    [int]$ClickGapSeconds = 4,
    [switch]$DryRun
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir

function Resolve-ProbePath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return (Join-Path $repoRoot $Path)
}

$exePath = Resolve-ProbePath -Path $Exe
$gameDirPath = Resolve-ProbePath -Path $GameDir
if ($LogDir -eq "") {
    $logDirPath = Split-Path -Parent $exePath
} else {
    $logDirPath = Resolve-ProbePath -Path $LogDir
}

$inputErrors = @()

if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    $inputErrors += "Exe not found: $exePath"
} else {
    $exePath = (Resolve-Path -LiteralPath $exePath).Path
}

if (-not (Test-Path -LiteralPath $gameDirPath -PathType Container)) {
    $inputErrors += "Game directory not found: $gameDirPath"
} else {
    $gameDirPath = (Resolve-Path -LiteralPath $gameDirPath).Path
}

if (-not (Test-Path -LiteralPath $logDirPath -PathType Container)) {
    $inputErrors += "Log directory not found: $logDirPath"
} else {
    $logDirPath = (Resolve-Path -LiteralPath $logDirPath).Path
}

if ($inputErrors.Count -gt 0) {
    foreach ($message in $inputErrors) {
        Write-Host "ERROR: $message"
    }
    exit 2
}

$outLogPath = Join-Path $logDirPath "vkrend.log"
$errLogPath = Join-Path $logDirPath "vkrend.err"

$clickPoints = @(
    [pscustomobject]@{ X = 422; Y = 159 },
    [pscustomobject]@{ X = 217; Y = 340 },
    [pscustomobject]@{ X = 535; Y = 402 }
)

$existingProcesses = @(Get-Process -Name dethrace -ErrorAction SilentlyContinue)

if ($DryRun) {
    Write-Host "VKREND probe dry run"
    Write-Host ("Exe             : {0}" -f $exePath)
    Write-Host ("GameDir         : {0}" -f $gameDirPath)
    Write-Host ("LogDir          : {0}" -f $logDirPath)
    Write-Host ("Stdout log      : {0}" -f $outLogPath)
    Write-Host ("Stderr log      : {0}" -f $errLogPath)
    Write-Host ("MenuWaitSeconds : {0}" -f $MenuWaitSeconds)
    Write-Host ("RaceWaitSeconds : {0}" -f $RaceWaitSeconds)
    Write-Host ("ClickHoldMs     : {0}" -f $ClickHoldMs)
    Write-Host ("ClickGapSeconds : {0}" -f $ClickGapSeconds)
    Write-Host ""
    Write-Host "Planned steps:"
    Write-Host "   1. Refuse to start if another dethrace process is running (exit code 2)."
    Write-Host ("   2. Delete stale {0} and {1}." -f $outLogPath, $errLogPath)
    Write-Host ("   3. Start `"{0}`" --vkrend --window --dir `"{1}`" with working directory {1}." -f $exePath, $gameDirPath)
    Write-Host ("   4. Wait {0} seconds for the main menu." -f $MenuWaitSeconds)
    Write-Host ("   5. Click client point (422, 159), hold {0} ms, then wait {1} s." -f $ClickHoldMs, $ClickGapSeconds)
    Write-Host ("   6. Click client point (217, 340), hold {0} ms, then wait {1} s." -f $ClickHoldMs, $ClickGapSeconds)
    Write-Host ("   7. Click client point (535, 402), hold {0} ms, then wait {1} s." -f $ClickHoldMs, $ClickGapSeconds)
    Write-Host ("   8. Wait {0} seconds for the race." -f $RaceWaitSeconds)
    Write-Host "   9. Report process state, EXCEPTION_ACCESS_VIOLATION and VKREND markers across vkrend.log and vkrend.err."
    Write-Host "  10. Stop the dethrace process that was started."
    if ($existingProcesses.Count -gt 0) {
        $runningIds = ($existingProcesses | ForEach-Object { $_.Id }) -join ", "
        Write-Host ""
        Write-Host "NOTE: dethrace is already running (PID $runningIds); a real run would refuse to start with exit code 2."
    }
    Write-Host ""
    Write-Host "Inputs validated. Dry run complete; nothing was launched."
    exit 0
}

if ($existingProcesses.Count -gt 0) {
    $runningIds = ($existingProcesses | ForEach-Object { $_.Id }) -join ", "
    Write-Host "REFUSING TO START: another dethrace process is already running (PID $runningIds)."
    Write-Host "Close it before running the VKREND probe."
    exit 2
}

try {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace VkrendProbe
{
    public struct CPOINT
    {
        public int X;
        public int Y;
    }

    public static class Native
    {
        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool ClientToScreen(IntPtr hWnd, ref CPOINT lpPoint);

        [DllImport("user32.dll")]
        public static extern bool SetCursorPos(int x, int y);

        [DllImport("user32.dll")]
        public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
    }
}
"@
} catch {
    Write-Host "ERROR: failed to load user32 interop types: $($_.Exception.Message)"
    exit 2
}

function Invoke-ClientClick {
    param(
        [IntPtr]$WindowHandle,
        [int]$ClientX,
        [int]$ClientY,
        [int]$HoldMilliseconds
    )

    [VkrendProbe.Native]::ShowWindow($WindowHandle, 9) | Out-Null
    [VkrendProbe.Native]::SetForegroundWindow($WindowHandle) | Out-Null
    Start-Sleep -Milliseconds 300

    $point = New-Object VkrendProbe.CPOINT
    $point.X = $ClientX
    $point.Y = $ClientY
    [VkrendProbe.Native]::ClientToScreen($WindowHandle, [ref]$point) | Out-Null

    [VkrendProbe.Native]::SetCursorPos($point.X, $point.Y) | Out-Null
    Start-Sleep -Milliseconds 150

    [VkrendProbe.Native]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $HoldMilliseconds
    [VkrendProbe.Native]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}

Remove-Item -LiteralPath $outLogPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $errLogPath -Force -ErrorAction SilentlyContinue

$gameDirArg = $gameDirPath
while ($gameDirArg.Length -gt 3 -and $gameDirArg.EndsWith("\")) {
    $gameDirArg = $gameDirArg.Substring(0, $gameDirArg.Length - 1)
}
$startArguments = "--vkrend --window --dir `"$gameDirArg`""

Write-Host ("Starting `"{0}`" {1}" -f $exePath, $startArguments)
try {
    $proc = Start-Process -FilePath $exePath -ArgumentList $startArguments -WorkingDirectory $gameDirPath -PassThru -RedirectStandardOutput $outLogPath -RedirectStandardError $errLogPath
} catch {
    Write-Host "ERROR: failed to start $exePath : $($_.Exception.Message)"
    exit 2
}
Write-Host "Started dethrace (PID $($proc.Id)). Waiting $MenuWaitSeconds seconds for the main menu."

$pass = $false

try {
    Start-Sleep -Seconds $MenuWaitSeconds

    $proc.Refresh()
    $windowHandle = [IntPtr]::Zero
    $windowDeadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $windowDeadline) {
        $proc.Refresh()
        if ($proc.HasExited) {
            break
        }
        $candidateHandle = $proc.MainWindowHandle
        if ($candidateHandle -ne [IntPtr]::Zero) {
            $windowHandle = $candidateHandle
            break
        }
        Start-Sleep -Milliseconds 500
    }

    $clicksDelivered = 0
    if ($proc.HasExited) {
        Write-Host "WARNING: dethrace exited before the menu clicks."
    } elseif ($windowHandle -eq [IntPtr]::Zero) {
        Write-Host "WARNING: main window not found; menu clicks were not delivered."
    } else {
        foreach ($point in $clickPoints) {
            $proc.Refresh()
            if ($proc.HasExited) {
                break
            }
            Invoke-ClientClick -WindowHandle $windowHandle -ClientX $point.X -ClientY $point.Y -HoldMilliseconds $ClickHoldMs
            $clicksDelivered++
            Write-Host "Clicked client point ($($point.X), $($point.Y))."
            Start-Sleep -Seconds $ClickGapSeconds
        }
    }

    if (-not $proc.HasExited) {
        Write-Host "Waiting $RaceWaitSeconds seconds for the race."
        Start-Sleep -Seconds $RaceWaitSeconds
    }

    $proc.Refresh()
    $alive = -not $proc.HasExited

    if ($alive) {
        $stateText = "alive (PID $($proc.Id))"
    } else {
        try {
            $stateText = "exited (exit code $($proc.ExitCode))"
        } catch {
            $stateText = "exited (exit code unavailable)"
        }
    }

    $errText = ""
    if (Test-Path -LiteralPath $errLogPath) {
        $errText = [string](Get-Content -LiteralPath $errLogPath -Raw -ErrorAction SilentlyContinue)
    }
    $logText = ""
    if (Test-Path -LiteralPath $outLogPath) {
        $logText = [string](Get-Content -LiteralPath $outLogPath -Raw -ErrorAction SilentlyContinue)
    }

    $allText = $logText + [Environment]::NewLine + $errText
    $accessViolation = $errText.Contains("EXCEPTION_ACCESS_VIOLATION")
    $rendererMarker = $allText.Contains("VKREND: renderer facility in use")
    $offscreenMatch = $allText.Contains("VKREND: device offscreen match")

    $pass = $alive -and (-not $accessViolation)

    $avText = "no"
    if ($accessViolation) {
        $avText = "yes"
    }
    $markerText = "missing"
    if ($rendererMarker) {
        $markerText = "present"
    }
    $offscreenText = "absent"
    if ($offscreenMatch) {
        $offscreenText = "present"
    }
    $resultText = "FAIL"
    if ($pass) {
        $resultText = "PASS"
    }

    Write-Host ""
    Write-Host "==== VKREND PROBE SUMMARY ===="
    Write-Host ("Exe              : {0}" -f $exePath)
    Write-Host ("GameDir          : {0}" -f $gameDirPath)
    Write-Host ("LogDir           : {0}" -f $logDirPath)
    Write-Host ("Process          : {0}" -f $stateText)
    Write-Host ("Access violation : {0} (checked {1})" -f $avText, $errLogPath)
    Write-Host ("Renderer marker  : {0} ('VKREND: renderer facility in use' in the logs)" -f $markerText)
    Write-Host ("Offscreen match  : {0} ('VKREND: device offscreen match' in the logs)" -f $offscreenText)
    Write-Host ("Menu clicks      : {0}/3" -f $clicksDelivered)
    Write-Host ("Result           : {0}" -f $resultText)
    if (-not $rendererMarker) {
        Write-Host "NOTE: the logs do not contain 'VKREND: renderer facility in use'."
    }
    if (-not $offscreenMatch) {
        Write-Host "NOTE: the logs do not contain 'VKREND: device offscreen match' (acceptable in older builds)."
    }
    if ($clicksDelivered -lt 3) {
        Write-Host "WARNING: the probe did not deliver all menu clicks."
    }
    Write-Host "=============================="
} catch {
    Write-Host "ERROR: the probe aborted: $($_.Exception.Message)"
    $pass = $false
} finally {
    try {
        $proc.Refresh()
        if (-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force -ErrorAction Stop
            Start-Sleep -Milliseconds 750
            Write-Host "Stopped dethrace (PID $($proc.Id))."
        } else {
            Write-Host "dethrace had already exited; nothing to stop."
        }
    } catch {
        Write-Host "WARNING: failed to stop dethrace (PID $($proc.Id)): $($_.Exception.Message)"
    }
}

if ($pass) {
    exit 0
}
exit 1
