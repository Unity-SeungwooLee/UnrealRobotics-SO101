# UnrealRobotics-SO101

Unreal Engine 5.4 기반 **SO-ARM-101 로봇 디지털 트윈** 클라이언트.  
물리 로봇이 실행하는 ROS2와 WebSocket으로 연결하여 실시간 시각화 및 제어를 제공한다.

---

## Architecture

```
┌─────────────────────────────┐        WebSocket (ws://localhost:9090)
│  Windows 11                 │ ◄──────────────────────────────────────►
│                             │
│  Unreal Engine 5.4.4        │        ┌─────────────────────────────┐
│  SO101_Twin                 │        │  WSL2 (Ubuntu)              │
│  · URosBridgeSubsystem      │        │  ROS2 humble                │
│  · Robot visualization      │        │  SO-ARM-101 drivers         │
│  · rosbridge v2 client      │        │  rosbridge_suite            │
└─────────────────────────────┘        └─────────────────────────────┘
```

- **UE → ROS2**: 제어 명령 publish (`/cmd_vel` 등)
- **ROS2 → UE**: 센서 데이터 / 관절 상태 subscribe
- **좌표계 변환**: UE (cm, 좌수계) ↔ ROS (m, 우수계), Y축 반전

---

## Prerequisites

### Windows side

| 항목 | 버전 |
|---|---|
| Unreal Engine | 5.4.4 (Epic Games Launcher) |
| Visual Studio | 2022, "Game development with C++" workload |
| OS | Windows 11 |

### WSL2 side

| 항목 | 값 |
|---|---|
| Distro | Ubuntu |
| ROS2 | humble |
| rosbridge | `ros-humble-rosbridge-suite` |
| WSL2 networking | mirrored mode |

**WSL2 mirrored networking 설정** (`%UserProfile%\.wslconfig`):
```ini
[wsl2]
networkingMode=mirrored
```

---

## Setup

### 1. Clone & open in Unreal

```powershell
git clone https://github.com/Unity-SeungwooLee/UnrealRobotics-SO101.git
```

`SO101_Twin.uproject`를 우클릭 → **Generate Visual Studio project files** 실행 후 Unreal Editor에서 열기.

### 2. Build (command line)

```powershell
& "C:\Program Files (x86)\UE_5.4\Engine\Build\BatchFiles\Build.bat" `
  SO101_TwinEditor Win64 Development `
  -Project="$PWD\SO101_Twin.uproject" -WaitMutex
```

### 3. WSL2에서 rosbridge 실행

```bash
# WSL2 터미널
source /opt/ros/humble/setup.bash
ros2 launch rosbridge_server rosbridge_websocket_launch.xml
```

### 4. 연결 확인 (Windows PowerShell)

```powershell
Test-NetConnection -ComputerName localhost -Port 9090
```

`TcpTestSucceeded : True`가 나오면 Unreal Editor에서 Play.

---

## Project Structure

```
SO101_Twin.uproject
Source/
  SO101_Twin/
    RosBridge/          # rosbridge WebSocket 클라이언트 (URosBridgeSubsystem)
    Robot/              # 로봇 시각화 액터 및 컴포넌트
    SO101_Twin.Build.cs
Config/
  DefaultEngine.ini
  DefaultGame.ini
  DefaultInput.ini
  DefaultEditor.ini
```

---

## Coordinate Conversion

| | Unit | Handedness | Up | Forward |
|---|---|---|---|---|
| Unreal | cm | Left-handed | Z | X |
| ROS | m | Right-handed | Z | X |

변환 규칙: 위치는 ×100 후 Y 반전, 쿼터니언은 Y·W 부호 반전.  
씬이 "좌우 반전"처럼 보이면 좌표 변환 누락을 먼저 의심할 것.

---

## License

This project is for research and educational purposes.
