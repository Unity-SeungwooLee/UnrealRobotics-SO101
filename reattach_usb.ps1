<#
.SYNOPSIS
    SO101 - USB detach + reattach cycle
.DESCRIPTION
    Forces a clean detach then reattach of the Follower/Leader USB devices.
    Use this when devices are in a stuck state (e.g. worker shows
    'could not open port /dev/ttyACM0' even though usbipd says attached).

    This is the heavy-handed reset. Normal launch (.\launch_so101.ps1) only
    attaches; it does not detach. Run this first if attach alone is not working.
.EXAMPLE
    .\reattach_usb.ps1
#>

param(
    [string]$Distro        = 'Ubuntu-22.04',
    [string]$FollowerBusId = '1-7',
    [string]$LeaderBusId   = '3-2'
)

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  SO101 - USB Detach + Reattach" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

if (-not (Get-Command usbipd -ErrorAction SilentlyContinue)) {
    Write-Error "usbipd-win not found. Install with: winget install usbipd"
    exit 1
}

# 1. Make sure the distro is running (attach needs it)
Write-Host "[WSL] Ensuring '$Distro' is running..." -ForegroundColor Yellow
wsl -d $Distro -- true 2>$null
Start-Process -WindowStyle Hidden wsl -ArgumentList @("-d", $Distro, "--", "sleep", "15")
Start-Sleep -Seconds 2

# 2. Detach (ignore 'already not attached')
Write-Host "[USB] Detaching..." -ForegroundColor Yellow
& usbipd detach --busid $FollowerBusId 2>&1 | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
& usbipd detach --busid $LeaderBusId   2>&1 | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
Start-Sleep -Seconds 2

# 3. Reattach
Write-Host "[USB] Reattaching Follower ($FollowerBusId)..." -ForegroundColor Yellow
& usbipd attach --wsl --busid $FollowerBusId 2>&1 | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
Start-Sleep -Milliseconds 500

Write-Host "[USB] Reattaching Leader ($LeaderBusId)..." -ForegroundColor Yellow
& usbipd attach --wsl --busid $LeaderBusId 2>&1 | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
Start-Sleep -Seconds 2

# 4. Verify
$tty = wsl -d $Distro -- bash -c "ls /dev/ttyACM* 2>/dev/null"
Write-Host ""
if ($tty) {
    Write-Host "[USB] OK - WSL sees: $tty" -ForegroundColor Green
} else {
    Write-Warning "[USB] Still no /dev/ttyACM* in WSL."
    Write-Host "      Check 'usbipd list' (STATE should be Shared) and physical USB connection." -ForegroundColor DarkYellow
}
Write-Host ""
