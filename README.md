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
┌──────────────────────────────────────────────┐     WebSocket ws://127.0.0.1:9090/?x=1
│  Windows 11                                  │ ◄──────────────────────────────────────►
│                                              │
│  Unreal Engine 5.4.4  (SO101_Twin)           │     ┌─────────────────────────────────────┐
│  · URosBridgeSubsystem  (WebSocket 소유)      │     │  WSL2 (Ubuntu-22.04)                │
│    - Subscribe / Advertise / Publish         │     │  ROS2 humble + CycloneDDS           │
│    - 자동 재접속 (지수 백오프 1s→30s)          │     │                                     │
│    - 재접속 시 구독·광고 자동 복원             │     │  1. lerobot_worker.py               │
│  · ARobotVisualizer  (URDF 관절 시각화)       │     │     leader/follower 텔레오프         │
│    - /joint_states 구독 → 관절 각도 적용      │     │  2. bridge_node  (ROS2 ↔ worker)    │
│    - MoveIt 명령 publish (named/joint/pose)  │     │  3. rosbridge_websocket :9090        │
│    - 녹화 / 재생 / 긴급 정지 제어             │     │  4. robot_state_publisher  (옵션)    │
│    - 연결 헬스 모니터링 (브리지·워커 heartbeat)│     │  5. MoveIt 스택  (옵션)              │
└──────────────────────────────────────────────┘     └─────────────────────────────────────┘
```

### 데이터 흐름

| 방향 | 토픽 | 타입 |
|---|---|---|
| ROS → UE | `/joint_states` | `sensor_msgs/JointState` |
| ROS → UE | `/robot_status` | `std_msgs/String` (JSON) |
| ROS → UE | `/bridge_heartbeat` | `std_msgs/String` |
| UE → ROS | `/robot_command` | `std_msgs/String` (JSON) |
| UE → ROS | `/moveit_goal_named` | `std_msgs/String` |
| UE → ROS | `/moveit_goal_joints` | `sensor_msgs/JointState` |
| UE → ROS | `/moveit_goal_pose` | `geometry_msgs/PoseStamped` |

---

## Prerequisites

### Windows side

| 항목 | 버전 |
|---|---|
| Unreal Engine | 5.4.4 (Epic Games Launcher) |
| Visual Studio | 2022, "Game development with C++" workload |
| OS | Windows 11 |
| Windows Terminal | Microsoft Store |
| usbipd-win | `winget install usbipd` |

### WSL2 side

| 항목 | 값 |
|---|---|
| Distro | Ubuntu-22.04 |
| ROS2 | humble + CycloneDDS (`rmw_cyclonedds_cpp`) |
| rosbridge | `ros-humble-rosbridge-suite` |
| LeRobot | `conda activate lerobot` |
| WSL2 networking | mirrored mode |

**WSL2 mirrored networking 설정** (`%UserProfile%\.wslconfig`):
```ini
[wsl2]
networkingMode=mirrored
```

---

## Setup (작성 중)

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

### 3. 시스템 실행

#### GUI 런처 (권장)

```powershell
powershell -ExecutionPolicy Bypass -File .\so101_launcher.ps1
```

WPF 버튼 패널이 열린다. Normal Mode 또는 MoveIt Mode 버튼으로 한 번에 실행.

#### CLI 런처

```powershell
# 일반 모드 (Worker + Bridge + rosbridge — Windows Terminal 3탭)
.\launch_so101.ps1

# MoveIt 모드 (위 3탭 + RSP + MoveIt + GoalNode + ActionServer — 7탭)
.\launch_so101.ps1 -Mode moveit

# USB 이미 연결된 경우 스킵
.\launch_so101.ps1 -SkipUSB
```

### 4. 연결 확인

```powershell
Test-NetConnection -ComputerName localhost -Port 9090
```

`TcpTestSucceeded : True`가 나오면 Unreal Editor에서 Play.

---

## PowerShell Scripts

| 스크립트 | 용도 |
|---|---|
| `so101_launcher.ps1` | WPF GUI — 모든 스크립트의 버튼 패널 |
| `launch_so101.ps1` | USB attach + WSL2 탭 실행 (`-Mode normal\|moveit`, `-SkipUSB`) |
| `stop_so101.ps1` | WSL2 프로세스 일괄 종료 (`-DetachUSB` 옵션) |
| `reattach_usb.ps1` | USB detach → reattach 강제 리셋 (포트 stuck 시) |

---

## Project Structure

```
SO101_Twin.uproject
Source/
  SO101_Twin/
    RosBridge/
      RosBridgeSubsystem.h/.cpp   # WebSocket 소유 GameInstanceSubsystem
      RosBridgeLog.h/.cpp         # 전용 로그 카테고리
      RosTestActor.h/.cpp         # 연결 테스트 액터
    Robot/
      RobotVisualizer.h/.cpp      # SO-ARM-101 URDF 시각화 + 제어
      RosCoordConv.h              # 좌표계 변환 헬퍼 (ROS ↔ UE)
    SO101_Twin.Build.cs
Config/
  DefaultEngine.ini
  DefaultGame.ini
  DefaultInput.ini
  DefaultEditor.ini
```

---

## Key Components

### URosBridgeSubsystem

`UGameInstanceSubsystem`으로 WebSocket 연결을 단독 소유. 레벨 이동 시에도 연결이 유지된다.

```cpp
URosBridgeSubsystem* Ros = GetGameInstance()->GetSubsystem<URosBridgeSubsystem>();
Ros->OnConnected.AddDynamic(this, &AMyActor::OnRosConnected);
Ros->Connect(TEXT("ws://127.0.0.1:9090/?x=1"));

// OnRosConnected() 내부:
Ros->Subscribe(TEXT("/joint_states"), TEXT("sensor_msgs/JointState"));
Ros->Advertise(TEXT("/cmd_vel"), TEXT("geometry_msgs/Twist"));
Ros->Publish(TEXT("/cmd_vel"), TEXT("{\"linear\":{\"x\":0.5},\"angular\":{\"z\":0.0}}"));
```

- 연결 끊김 시 지수 백오프(1s → 30s)로 자동 재접속
- 재접속 후 Subscribe / Advertise 목록 자동 복원
- `OnConnected` / `OnDisconnected` / `OnTopicMessage` Blueprint 델리게이트 제공

### ARobotVisualizer

SO-ARM-101의 URDF 링크/조인트 구조를 UE 컴포넌트 계층으로 재현하여 `/joint_states`에 따라 실시간 시각화.

**MoveIt 명령 (Details 패널 > ROS|MoveIt):**
- `SendNamedTarget()` — "home", "ready" 등 명명된 자세 전송
- `SendJointGoal()` — 관절 각도(rad) 직접 지정
- `SendPoseGoal()` — UE 좌표(cm) → ROS 좌표(m) 자동 변환 후 위치 목표 전송

**녹화 / 재생 / 안전 (Details 패널 > ROS|Record · Replay · Safety):**
- `SyncOn()` / `SyncOff()` — 리더→팔로워 텔레오프 on/off
- `StartRecord()` / `StopRecord()` — 관절 궤적 녹화
- `StartReplay()` / `StopReplay()` — 녹화된 궤적 재생
- `EStop()` — 모든 모션 즉시 정지

**연결 헬스 모니터링:**
- `/bridge_heartbeat` (1 Hz) 수신 감시 — 4초 이상 없으면 경고
- `/joint_states` (30 Hz) 수신 감시 — 3초 이상 없으면 워커 오프라인 경고
- 팔로워/리더 장치 오류 (`device_error`) 감지

### RosCoordConv

```cpp
// ROS → UE
FVector uePos = RosCoordConv::RosToUePosition(x_m, y_m, z_m);
FQuat   ueQuat = RosCoordConv::RosQuatToUe(qx, qy, qz, qw);
float   ueDeg = RosCoordConv::RosJointAngleToUeDegrees(angle_rad);

// UE → ROS
double rx, ry, rz;
RosCoordConv::UeToRosPosition(uePos, rx, ry, rz);
```

---

## Coordinate Conversion

| | Unit | Handedness | Up | Forward |
|---|---|---|---|---|
| Unreal | cm | Left-handed | Z | X |
| ROS | m | Right-handed | Z | X |

변환 규칙: 위치는 ×100 후 Y 반전, 쿼터니언은 Y·W 부호 반전, 관절 각도는 부호 반전.  
씬이 "좌우 반전"처럼 보이면 좌표 변환 누락을 먼저 의심할 것.

---

## USB Management (usbipd-win)

SO-ARM-101의 리더/팔로워 암은 USB-Serial로 연결된다. usbipd로 WSL2에 전달해야 한다.

| 버스 ID | 역할 |
|---|---|
| `1-7` | Follower arm |
| `3-2` | Leader arm |

```powershell
# 현재 상태 확인
usbipd list

# 수동 attach
usbipd attach --wsl --busid 1-7
usbipd attach --wsl --busid 3-2

# WSL2에서 장치 확인
wsl -d Ubuntu-22.04 -- ls /dev/ttyACM*
```

장치가 stuck 상태이면 `.\reattach_usb.ps1`로 강제 리셋.

---

## License

This project is for research and educational purposes.
