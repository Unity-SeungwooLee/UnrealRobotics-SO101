# UnrealRobotics-SO101

Unreal Engine 5.4 기반 **SO-ARM-101 로봇 디지털 트윈** 클라이언트.  
실물 로봇 ↔ Unreal 양방향 Sim-to-Real 디지털 트윈 (개인 프로젝트, 진행 중)

https://github.com/user-attachments/assets/41278562-b763-4921-953c-f5606dfdc79b

Stack: ROS2 Humble · MoveIt 2 · Unreal Engine 5.4.4 (C++) · LeRobot · ZeroMQ · rosbridge · WSL2/Ubuntu

- **양방향 Sim-to-Real 파이프라인 구축**: 실물 SO-ARM-101(6축 서보 암)의 관절 상태를 30Hz로 Unreal 디지털 트윈에 실시간 반영하고, Unreal에서 지정한 목표 동작을 실물 로봇으로 하달하는 양방향 흐름을 처음부터 직접 구현.
- **MoveIt 2 모션 플래닝 통합**: Setup Assistant로 SRDF·kinematics·collision 설정 패키지를 구성하고, moveit_msgs/MoveGroup action client를 rclpy로 직접 작성. RViz·Unreal에서 IK/FK/named target으로 목표를 주면 플래닝→실물 실행까지 동작. 5DOF 로봇의 6DOF IK 실패를 진단하고 position-only goal로 우회.
- **모듈형 시스템 아키텍처 설계**: rclpy(Py3.10)와 LeRobot(Py3.12)의 ABI 비호환을 two-process + ZeroMQ IPC 구조로 해결. UE↔ROS2는 rosbridge WebSocket, ROS2 노드 간 통신은 CycloneDDS로 분리 구성. Worker / Bridge / Action server를 모듈로 분리하고 상태 머신(idle·sync·record·replay)으로 관리.
- **안정성·검증 로직**: 이중 관절 클램핑(session limit + 실측 physical limit), heartbeat 기반 자동 E-stop, Bridge/Worker 장애 분리 진단, USB 단선 자동 재연결 등 현장 운영을 가정한 안전 메커니즘 구현.
- **Record / Replay (Teach & Repeat)**: 관절 궤적 기록·재생 기능 구현 — 공장 반복 작업 자동화의 기본 단위. Cosine ease-in-out 보간으로 서보 충격 완화.
- **3D 에셋·좌표계 파이프라인**: URDF 메시(STL)를 Blender로 정리·FBX 변환, 7링크 6조인트를 Unreal SceneComponent 계층으로 구축. UE(cm·좌수계) ↔ ROS(m·우수계) 좌표·쿼터니언 변환 헬퍼 작성.

https://github.com/user-attachments/assets/0654abef-e979-4af7-bcd8-5db0cd8ebf9b

https://github.com/user-attachments/assets/ee9b3665-4707-4325-a5c6-124dd7ed0e11

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
