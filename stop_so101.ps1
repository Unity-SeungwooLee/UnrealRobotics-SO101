<#
.SYNOPSIS
    SO101 Digital Twin - Stop all processes
.PARAMETER DetachUSB
    Also detach USB devices.
.EXAMPLE
    .\stop_so101.ps1
    .\stop_so101.ps1 -DetachUSB
#>

param(
    [switch]$DetachUSB,
    [string]$Distro        = 'Ubuntu-22.04',
    [string]$FollowerBusId = '1-7',
    [string]$LeaderBusId   = '3-2'
)

Write-Host ""
Write-Host "============================================" -ForegroundColor Yellow
Write-Host "  SO101 Digital Twin - Stopping all" -ForegroundColor Yellow
Write-Host "============================================" -ForegroundColor Yellow

$patterns = @(
    'lerobot_worker.py',
    'bridge_node',
    'rosbridge_websocket',
    'robot_state_publisher',
    'moveit_goal_node.py',
    'joint_trajectory_action_server.py',
    'move_group',
    'rviz2'
)

Write-Host "[Stop] Killing WSL2 processes..." -ForegroundColor Yellow
foreach ($p in $patterns) {
    $found = wsl -d $Distro -- bash -c "pgrep -f '$p' 2>/dev/null"
    if ($found) {
        wsl -d $Distro -- bash -c "pkill -f '$p' 2>/dev/null"
        Write-Host "  Killed: $p" -ForegroundColor Gray
    }
}

if ($DetachUSB) {
    Write-Host "[USB] Detaching devices..." -ForegroundColor Yellow
    & usbipd detach --busid $FollowerBusId 2>&1 | Out-Null
    & usbipd detach --busid $LeaderBusId   2>&1 | Out-Null
    Write-Host "  Detached Follower ($FollowerBusId), Leader ($LeaderBusId)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  All SO101 processes stopped." -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
