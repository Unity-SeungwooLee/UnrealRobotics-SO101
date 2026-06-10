<#
.SYNOPSIS
    SO101 Digital Twin - GUI Launcher (WPF)
.DESCRIPTION
    A small button panel that calls the existing launch/stop/reattach scripts.
    No installation required: uses .NET WPF built into Windows PowerShell.

    Buttons:
      Normal Mode   -> launch_so101.ps1
      MoveIt Mode   -> launch_so101.ps1 -Mode moveit
      Reattach USB  -> reattach_usb.ps1
      Stop All      -> stop_so101.ps1
      Check USB     -> usbipd list (parsed for Follower/Leader)

    The .ps1 scripts must sit in the SAME folder as this file.
.EXAMPLE
    Right-click -> Run with PowerShell
    or:  powershell -ExecutionPolicy Bypass -File .\so101_launcher.ps1
#>

# WPF must run in STA (Single-Threaded Apartment).
# If launched in MTA, relaunch self in STA.
if ([System.Threading.Thread]::CurrentThread.GetApartmentState() -ne 'STA') {
    Start-Process powershell -ArgumentList @(
        '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass',
        '-File', "`"$PSCommandPath`""
    )
    return
}

Add-Type -AssemblyName PresentationFramework
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase

# Folder where this script (and the .ps1 scripts) live
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

$FollowerBusId = '1-7'
$LeaderBusId   = '3-2'

# ----------------------------------------------
# XAML UI definition
# ----------------------------------------------
[xml]$xaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="SO101 Launcher" Height="430" Width="360"
        WindowStartupLocation="CenterScreen" ResizeMode="CanMinimize"
        Background="#1E1F24" FontFamily="Segoe UI">
    <Grid Margin="18">
        <Grid.RowDefinitions>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="*"/>
            <RowDefinition Height="Auto"/>
        </Grid.RowDefinitions>

        <TextBlock Grid.Row="0" Text="SO101 Digital Twin"
                   Foreground="#E8E8EC" FontSize="18" FontWeight="SemiBold"
                   Margin="0,0,0,2"/>
        <TextBlock Grid.Row="1" Text="Launch control"
                   Foreground="#8A8B93" FontSize="12" Margin="0,0,0,16"/>

        <Button x:Name="BtnNormal" Grid.Row="2" Content="Start Normal Mode  (3 tabs)"
                Height="44" Margin="0,0,0,8" FontSize="13"
                Background="#3B82F6" Foreground="White" BorderThickness="0"
                Cursor="Hand"/>
        <Button x:Name="BtnMoveIt" Grid.Row="3" Content="Start MoveIt Mode  (7 tabs)"
                Height="44" Margin="0,0,0,8" FontSize="13"
                Background="#2563EB" Foreground="White" BorderThickness="0"
                Cursor="Hand"/>
        <Button x:Name="BtnReattach" Grid.Row="4" Content="Reattach USB"
                Height="38" Margin="0,0,0,8" FontSize="13"
                Background="#374151" Foreground="#E8E8EC" BorderThickness="0"
                Cursor="Hand"/>

        <Grid Grid.Row="5" Margin="0,0,0,8">
            <Grid.ColumnDefinitions>
                <ColumnDefinition Width="*"/>
                <ColumnDefinition Width="8"/>
                <ColumnDefinition Width="*"/>
            </Grid.ColumnDefinitions>
            <Button x:Name="BtnCheck" Grid.Column="0" Content="Check USB"
                    Height="38" FontSize="13"
                    Background="#374151" Foreground="#E8E8EC" BorderThickness="0"
                    Cursor="Hand"/>
            <Button x:Name="BtnStop" Grid.Column="2" Content="Stop All"
                    Height="38" FontSize="13"
                    Background="#7F1D1D" Foreground="White" BorderThickness="0"
                    Cursor="Hand"/>
        </Grid>

        <Border Grid.Row="6" Background="#15161A" CornerRadius="6"
                Margin="0,8,0,0" Padding="10">
            <TextBlock x:Name="LogBox" Text="Ready."
                       Foreground="#A7F3D0" FontFamily="Consolas" FontSize="12"
                       TextWrapping="Wrap" VerticalAlignment="Top"/>
        </Border>

        <TextBlock x:Name="StatusBar" Grid.Row="7" Text="Idle"
                   Foreground="#8A8B93" FontSize="11" Margin="0,8,0,0"/>
    </Grid>
</Window>
'@

# ----------------------------------------------
# Load XAML
# ----------------------------------------------
$reader = New-Object System.Xml.XmlNodeReader $xaml
$window = [Windows.Markup.XamlReader]::Load($reader)

$BtnNormal   = $window.FindName('BtnNormal')
$BtnMoveIt   = $window.FindName('BtnMoveIt')
$BtnReattach = $window.FindName('BtnReattach')
$BtnCheck    = $window.FindName('BtnCheck')
$BtnStop     = $window.FindName('BtnStop')
$LogBox      = $window.FindName('LogBox')
$StatusBar   = $window.FindName('StatusBar')

# ----------------------------------------------
# Helpers
# ----------------------------------------------
function Set-Log {
    param([string]$Text, [string]$Color = '#A7F3D0')
    $LogBox.Text = $Text
    $LogBox.Foreground = [System.Windows.Media.BrushConverter]::new().ConvertFromString($Color)
}
function Set-Status { param([string]$Text) $StatusBar.Text = $Text }

# Launch a .ps1 in a separate PowerShell window (so its own logs/tabs show normally)
function Invoke-Script {
    param([string]$ScriptName, [string]$Args = '')
    $path = Join-Path $ScriptDir $ScriptName
    if (-not (Test-Path $path)) {
        Set-Log "Script not found:`n$ScriptName`n(must be in same folder)" '#FCA5A5'
        Set-Status "Error: missing $ScriptName"
        return
    }
    $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$path`" $Args"
    Start-Process powershell -ArgumentList $argList
    Set-Status "Launched: $ScriptName $Args"
}

# Run usbipd list and report Follower/Leader state into the log box
function Show-UsbStatus {
    Set-Log "Checking USB..." '#FDE68A'
    Set-Status "Running usbipd list..."
    try {
        $lines = & usbipd list 2>&1
    } catch {
        Set-Log "usbipd not found.`nInstall: winget install usbipd" '#FCA5A5'
        Set-Status "Error: usbipd missing"
        return
    }
    $report = @()
    foreach ($id in @($FollowerBusId, $LeaderBusId)) {
        $role = if ($id -eq $FollowerBusId) { 'Follower' } else { 'Leader' }
        $row  = $lines | Where-Object { $_ -match "^\s*$([regex]::Escape($id))\s" }
        if ($row) {
            if ($row -match 'Attached') {
                $report += "$role ($id): ATTACHED"
            } elseif ($row -match 'Shared') {
                $report += "$role ($id): shared (not attached)"
            } else {
                $report += "$role ($id): not shared"
            }
        } else {
            $report += "$role ($id): NOT FOUND"
        }
    }
    # Also confirm what WSL sees
    $tty = wsl -d Ubuntu-22.04 -- bash -c "ls /dev/ttyACM* 2>/dev/null"
    if ($tty) { $report += "WSL: $tty" } else { $report += "WSL: no /dev/ttyACM*" }

    $allAttached = ($report[0] -match 'ATTACHED') -and ($report[1] -match 'ATTACHED')
    $color = if ($allAttached) { '#A7F3D0' } else { '#FDE68A' }
    Set-Log ($report -join "`n") $color
    Set-Status "USB check done"
}

# ----------------------------------------------
# Button handlers
# ----------------------------------------------
$BtnNormal.Add_Click({
    Set-Log "Starting NORMAL mode...`nAttaching USB, opening 3 tabs." '#BFDBFE'
    Invoke-Script 'launch_so101.ps1'
})
$BtnMoveIt.Add_Click({
    Set-Log "Starting MOVEIT mode...`nAttaching USB, opening 7 tabs." '#BFDBFE'
    Invoke-Script 'launch_so101.ps1' '-Mode moveit'
})
$BtnReattach.Add_Click({
    Set-Log "Reattaching USB (detach + attach)..." '#FDE68A'
    Invoke-Script 'reattach_usb.ps1'
})
$BtnStop.Add_Click({
    Set-Log "Stopping all SO101 processes..." '#FCA5A5'
    Invoke-Script 'stop_so101.ps1'
})
$BtnCheck.Add_Click({ Show-UsbStatus })

# ----------------------------------------------
# Show window
# ----------------------------------------------
$window.ShowDialog() | Out-Null
