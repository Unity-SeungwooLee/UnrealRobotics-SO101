<#
.SYNOPSIS
    SO101 Digital Twin - Launch script
.DESCRIPTION
    1. Wakes up the WSL2 distro (usbipd attach needs it running).
    2. Attaches USB devices (attach only - no detach).
       If a device is already attached, usbipd reports it and we continue.
    3. Writes each process's bash command into WSL2 as a .sh file.
    4. Opens Windows Terminal tabs that run those .sh files.

    For a clean detach + reattach cycle, use: .\reattach_usb.ps1
.PARAMETER Mode
    normal (default) - Worker + Bridge + rosbridge (3 tabs)
    moveit           - above 3 + RSP + MoveIt + GoalNode + ActionServer (7 tabs)
.PARAMETER SkipUSB
    Skip USB attach entirely.
.EXAMPLE
    .\launch_so101.ps1
    .\launch_so101.ps1 -Mode moveit
    .\launch_so101.ps1 -SkipUSB
#>

param(
    [ValidateSet('normal', 'moveit')]
    [string]$Mode = 'normal',

    [switch]$SkipUSB,

    [string]$Distro        = 'Ubuntu-22.04',
    [string]$FollowerBusId = '1-7',
    [string]$LeaderBusId   = '3-2'
)

$ErrorActionPreference = 'Stop'

# ----------------------------------------------
# 0. Pre-flight checks
# ----------------------------------------------
if (-not (Get-Command wt -ErrorAction SilentlyContinue)) {
    Write-Error "Windows Terminal (wt) not found. Install it from the Microsoft Store."
    exit 1
}
if (-not $SkipUSB -and -not (Get-Command usbipd -ErrorAction SilentlyContinue)) {
    Write-Error "usbipd-win not found. Install with: winget install usbipd"
    exit 1
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  SO101 Digital Twin Launcher  (Mode: $Mode)" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# ----------------------------------------------
# 1. Wake up the WSL2 distro
#    (usbipd attach fails if the target distro is not running)
# ----------------------------------------------
Write-Host "[WSL] Ensuring '$Distro' is running..." -ForegroundColor Yellow
# A trivial command boots the distro and keeps the VM alive
wsl -d $Distro -- true 2>$null
# Background keep-alive so the distro stays up during attach
Start-Process -WindowStyle Hidden wsl -ArgumentList @("-d", $Distro, "--", "sleep", "10")
Start-Sleep -Seconds 2

# ----------------------------------------------
# 2. USB attach (attach only)
# ----------------------------------------------
if (-not $SkipUSB) {
    Write-Host "[USB] Attaching devices..." -ForegroundColor Yellow

    # usbipd 5.x auto-selects the running distro; do not pass a distro name.
    # 'already attached' is reported as success-ish; we surface real errors only.
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'

    Write-Host "[USB] Attach Follower ($FollowerBusId)..."
    $outF = & usbipd attach --wsl --busid $FollowerBusId 2>&1
    if ($LASTEXITCODE -ne 0) {
        # An already-attached device returns nonzero with a benign message; show it but keep going
        Write-Host "    (usbipd) $outF" -ForegroundColor DarkYellow
    }
    Start-Sleep -Milliseconds 500

    Write-Host "[USB] Attach Leader ($LeaderBusId)..."
    $outL = & usbipd attach --wsl --busid $LeaderBusId 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "    (usbipd) $outL" -ForegroundColor DarkYellow
    }

    $ErrorActionPreference = $prevEAP
    Start-Sleep -Seconds 2

    # Verify the serial devices actually appeared in WSL2
    $tty = wsl -d $Distro -- bash -c "ls /dev/ttyACM* 2>/dev/null"
    if ($tty) {
        Write-Host "[USB] OK - WSL sees: $tty" -ForegroundColor Green
    } else {
        Write-Warning "[USB] No /dev/ttyACM* in WSL. Devices may not be attached."
        Write-Host "      Try: .\reattach_usb.ps1   (detach + reattach)" -ForegroundColor DarkYellow
    }
}

# ----------------------------------------------
# 3. Write each tab's bash script into WSL2
#    (single-quote here-strings = no PowerShell interpolation)
# ----------------------------------------------

$shCommon = @'
# ~/.so101_launch/_common.sh
ros2_env() {
    export PATH=$(echo $PATH | tr ':' '\n' | grep -v miniforge | tr '\n' ':' | sed 's/:$//')
    source /opt/ros/humble/setup.bash
    source ~/UnrealRobotics/install/setup.bash
    export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
    export ROS_LOCALHOST_ONLY=1
}
'@

$shWorker = @'
#!/usr/bin/env bash
echo "=== [Worker] LeRobot Worker ==="
source ~/miniforge3/etc/profile.d/conda.sh
conda activate lerobot
sudo chmod 666 /dev/ttyACM* 2>/dev/null
cd ~/UnrealRobotics/src/lerobot_ros2_bridge
echo "[Worker] python scripts/lerobot_worker.py --cmd-joints all --cmd-limit 120"
python scripts/lerobot_worker.py --cmd-joints all --cmd-limit 120
exec bash
'@

$shBridge = @'
#!/usr/bin/env bash
source ~/.so101_launch/_common.sh
echo "=== [Bridge] ROS2 Bridge Node ==="
ros2_env
ros2 run lerobot_ros2_bridge bridge_node
exec bash
'@

$shRosbridge = @'
#!/usr/bin/env bash
source ~/.so101_launch/_common.sh
echo "=== [rosbridge] WebSocket Server :9090 ==="
ros2_env
ros2 launch rosbridge_server rosbridge_websocket_launch.xml
exec bash
'@

$shRSP = @'
#!/usr/bin/env bash
source ~/.so101_launch/_common.sh
echo "=== [RSP] robot_state_publisher ==="
ros2_env
xacro ~/UnrealRobotics/src/so101_description/urdf/so101_arm.urdf.xacro variant:=follower > /tmp/so101_follower.urdf
python3 -c "
import yaml
with open('/tmp/so101_follower.urdf') as f: urdf = f.read()
params = {'robot_state_publisher': {'ros__parameters': {'robot_description': urdf}}}
with open('/tmp/rsp_params.yaml', 'w') as f: yaml.dump(params, f, default_flow_style=False)
"
ros2 run robot_state_publisher robot_state_publisher --ros-args --params-file /tmp/rsp_params.yaml
exec bash
'@

$shMoveIt = @'
#!/usr/bin/env bash
source ~/.so101_launch/_common.sh
echo "=== [MoveIt] demo.launch.py ==="
ros2_env
ros2 launch so101_moveit_config demo.launch.py
exec bash
'@

$shGoalNode = @'
#!/usr/bin/env bash
source ~/.so101_launch/_common.sh
echo "=== [GoalNode] moveit_goal_node.py ==="
ros2_env
python3 ~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/moveit_goal_node.py
exec bash
'@

$shActionSrv = @'
#!/usr/bin/env bash
source ~/.so101_launch/_common.sh
echo "=== [ActionSrv] joint_trajectory_action_server.py ==="
ros2_env
python3 ~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/joint_trajectory_action_server.py
exec bash
'@

# Helper: write a script into WSL2 via base64 (safe for any special chars)
function Write-WslScript {
    param([string]$Name, [string]$Content)
    $lf = $Content -replace "`r`n", "`n"
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($lf)
    $b64   = [System.Convert]::ToBase64String($bytes)
    $target = "~/.so101_launch/$Name"
    wsl -d $Distro -- bash -c "mkdir -p ~/.so101_launch && echo $b64 | base64 -d > $target && chmod +x $target"
}

Write-Host "[Setup] Writing launch scripts into WSL2 (~/.so101_launch/)..." -ForegroundColor Yellow
Write-WslScript "_common.sh"     $shCommon
Write-WslScript "worker.sh"      $shWorker
Write-WslScript "bridge.sh"      $shBridge
Write-WslScript "rosbridge.sh"   $shRosbridge
if ($Mode -eq 'moveit') {
    Write-WslScript "rsp.sh"        $shRSP
    Write-WslScript "moveit.sh"     $shMoveIt
    Write-WslScript "goalnode.sh"   $shGoalNode
    Write-WslScript "actionsrv.sh"  $shActionSrv
}

# ----------------------------------------------
# 4. Build Windows Terminal tabs
# ----------------------------------------------
function New-Tab {
    param([string]$Title, [string]$ScriptName)
    return @("--title", $Title, "wsl", "-d", $Distro, "--", "bash", "~/.so101_launch/$ScriptName")
}

$tabs = @()
$tabs += ,@(New-Tab "1-Worker"    "worker.sh")
$tabs += ,@(New-Tab "2-Bridge"    "bridge.sh")
$tabs += ,@(New-Tab "3-rosbridge" "rosbridge.sh")
if ($Mode -eq 'moveit') {
    $tabs += ,@(New-Tab "4-RSP"       "rsp.sh")
    $tabs += ,@(New-Tab "5-MoveIt"    "moveit.sh")
    $tabs += ,@(New-Tab "6-GoalNode"  "goalnode.sh")
    $tabs += ,@(New-Tab "7-ActionSrv" "actionsrv.sh")
}

# Assemble wt args: first tab + (; new-tab ...) repeated
$wtArgs = @()
for ($i = 0; $i -lt $tabs.Count; $i++) {
    if ($i -gt 0) { $wtArgs += ";"; $wtArgs += "new-tab" }
    $wtArgs += $tabs[$i]
}

Write-Host "[Launch] Opening $($tabs.Count) WSL2 tabs ($Mode mode)..." -ForegroundColor Cyan
Start-Process wt -ArgumentList $wtArgs

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  $($tabs.Count) terminals launched!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host "  1. Wait for all tabs to initialize (~5s)"  -ForegroundColor Gray
Write-Host "  2. Open Unreal Editor -> PIE Play"         -ForegroundColor Gray
if ($Mode -eq 'moveit') {
    Write-Host "  3. RobotVisualizer Details > ROS|MoveIt" -ForegroundColor Gray
} else {
    Write-Host "  3. RobotVisualizer Details > ROS|Sync/Record/Replay" -ForegroundColor Gray
}
Write-Host ""
