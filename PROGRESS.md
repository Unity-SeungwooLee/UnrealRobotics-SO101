# SO101_Twin — Project Progress & Roadmap

디지털 트윈 프로젝트: **SO-ARM-101 실물 로봇 ↔ Unreal Engine 5.4.4 양방향 연동**

## Project Overview

- **Goal**: SO-ARM-101 로봇의 디지털 트윈 구축. 실물 로봇의 관절 상태를 Unreal에 실시간 반영하고, Unreal에서 계획한 동작을 실물 로봇에 전달.
- **Hardware**: SO-ARM-101 (Leader arm + Follower arm, 각 6x Feetech STS3215 servos, Waveshare bus servo driver board)
  - **Follower**: STS3215 **12V** 모터 x6 (1:345 all joints) + Waveshare 보드 + **12V** 어댑터
  - **Leader**: STS3215-C046 **7.4V** 모터 x6 (**1:147 all joints** — 업체 커스텀 구성) + Waveshare 보드 + **5V** 어댑터
    - 공식 SO-ARM101 Leader 사양은 C044×2(1:191) + C001×1(1:345) + C046×3(1:147)이지만, 업체가 C046×6 구성으로 판매 중. Trade-off: shoulder_lift(L2)에서 중력 영향이 공식 구성보다 크게 나타남. Gripper 기어 제거는 수행하지 않음.
- **Software stack**:
  - Windows 11 + Unreal Engine 5.4.4 (C++ 프로젝트 `SO101_Twin`)
  - WSL2 (Ubuntu 22.04) + ROS2 Humble
  - WSL2 (Ubuntu 22.04) + conda env `lerobot` (Python 3.12) + LeRobot editable install
  - 통신: rosbridge_suite (WebSocket, port 9090)
  - **LeRobot ↔ ROS2 bridge**: two-process ZeroMQ IPC (Python 버전 비호환 때문 — 섹션 3.3 참조)
  - **MoveIt 2**: 모션 플래닝 + FollowJointTrajectory action server (ZMQ 경유, Phase 7)
- **Project root**: `C:\UnrealProjects\UnrealRobotics-SO101\`
- **ROS2 workspace**: `~/UnrealRobotics/` (WSL2 경로: `/home/tmddn/UnrealRobotics/`)
  - `src/so101_description/` — URDF, 메시, launch 파일 (legalaspro에서 가져옴)
  - `src/lerobot_ros2_bridge/` — ZMQ ↔ ROS2 bridge 노드 (Phase 3.3에서 생성)
  - `src/so101_moveit_config/` — MoveIt 설정 패키지 (Phase 7에서 생성)
- **Related docs**:
  - `CLAUDE.md` — 프로젝트 메타 정보, 역할 분담, 빌드 명령
  - `SKILL.md` — Unreal C++ + ROS 통합 규칙과 known quirks

---

## Legend

- ✅ **Done** — 완료. 검증까지 끝남.
- 🔄 **In progress** — 현재 작업 중.
- ⏳ **Next up** — 바로 다음에 할 것.
- 📋 **Planned** — 로드맵에 있지만 아직 착수 전.
- ❓ **Open question** — 결정 필요.

---

# Phase 0 — Environment Setup

## ✅ 0.1 WSL2 & ROS2 Humble 환경 구성
- WSL2에 Ubuntu 22.04 설치 및 ROS2 Humble 설치
- `source /opt/ros/humble/setup.bash`를 `~/.bashrc`에 추가

## ✅ 0.2 WSL2 mirrored networking 설정
- `%UserProfile%\.wslconfig`에 다음 추가:
  ```ini
  [wsl2]
  networkingMode=mirrored
  ```
- `wsl --shutdown` 후 재시작으로 적용
- `ip addr show`에서 `loopback0` 인터페이스와 실제 LAN IP 확인으로 검증

## ✅ 0.3 Windows ↔ WSL2 TCP 통신 검증
- `python3 -m http.server 9090` → `Test-NetConnection -ComputerName localhost -Port 9090`이 `TcpTestSucceeded : True` 반환

## ✅ 0.4 rosbridge_suite 설치
```bash
sudo apt install -y ros-humble-rosbridge-suite
```

## ✅ 0.5 rosbridge 실행 검증
- `ros2 launch rosbridge_server rosbridge_websocket_launch.xml`로 기동
- PowerShell의 `System.Net.WebSockets.ClientWebSocket`로 수동 WebSocket 핸드셰이크 성공 확인
- `/chatter` 토픽 구독 → `demo_nodes_cpp talker`의 메시지 수신 성공

## ⚠️ 발견된 quirk (SKILL.md 섹션 7에 기록됨)
- `ros2 topic pub` + `ros2 topic echo` CLI 조합이 WSL2에서 잘 동작하지 않음. 우회: `demo_nodes_cpp talker`/`listener` 사용.
- `ROS_LOCALHOST_ONLY=1`나 CycloneDDS 전환은 **rosbridge 경로에서는** 불필요함이 확인됨. 기본 FastDDS로 rosbridge 경로는 정상 동작.
- ⚠️ 단, **ROS2 노드 간 DDS discovery에는 CycloneDDS가 필수** — Phase 3.3에서 발견. SKILL.md 섹션 7.4 참조.

---

# Phase 1 — Unreal Client (ROS → Unreal 수신 파이프)

## ✅ 1.1 Unreal 프로젝트 초기 설정
- UE 5.4.4로 C++ 프로젝트 `SO101_Twin` 생성
- `Source/SO101_Twin/SO101_Twin.Build.cs`에 의존성 추가:
  - `WebSockets`, `Json`, `JsonUtilities` (PrivateDependencyModuleNames)

## ✅ 1.2 RosBridge 모듈 구조 생성
- `Source/SO101_Twin/RosBridge/` 폴더 생성
- 로그 카테고리 파일: `RosBridgeLog.h`, `RosBridgeLog.cpp`
  - `DECLARE_LOG_CATEGORY_EXTERN(LogRosBridge, Log, All)` + `DEFINE_LOG_CATEGORY`

## ✅ 1.3 URosBridgeSubsystem 구현
- `UGameInstanceSubsystem` 기반 (레벨 변경에도 연결 유지)
- 기능 범위:
  - `Connect(Url)` / `Disconnect()` / `IsConnected()`
  - `Subscribe(Topic, Type)` — rosbridge v2 JSON 프로토콜
  - `FOnRosTopicMessage` 델리게이트 (토픽 메시지 수신 브로드캐스트)
  - 스레드 마샬링: WebSocket 콜백 → `AsyncTask(ENamedThreads::GameThread, ...)` → UObject 접근
  - `TWeakObjectPtr` 사용으로 콜백 생존 안전성 확보

## ✅ 1.4 ARosTestActor 테스트 액터 구현
- `BeginPlay`에서 Subsystem 가져와 Connect → 타이머로 지연 후 Subscribe
- `EndPlay`에서 델리게이트 해제 + 타이머 클리어
- `OnRosMessage` 핸들러: `UE_LOG` + `AddOnScreenDebugMessage`로 뷰포트 표시
- Details 패널에서 URL/Topic/Type 수정 가능 (`UPROPERTY(EditAnywhere)`)

## ✅ 1.5 첫 빌드 성공
- Development Editor / Win64 구성
- 49개 액션 성공, 0개 실패
- `UnrealEditor-SO101_Twin.dll` 생성 확인

## ✅ 1.6 End-to-end 메시지 수신 성공
- WSL2: `rosbridge_server` + `demo_nodes_cpp talker` 기동
- Unreal PIE에서 `/chatter` 구독 → Output Log와 뷰포트에 `Hello World: N` 실시간 표시
- rosbridge 측 로그에 `Client connected` + `Subscribed to /chatter` 확인

## ⚠️ 해결된 hard-won quirks (SKILL.md 섹션 7에 상세 기록)
- **libwebsockets path mangling**: `ws://host:9090` 또는 `ws://host:9090/`에 대해 `GET //` (double slash)로 HTTP 요청이 나가서 rosbridge가 404 반환. **해결**: URL에 `?x=1` 같은 더미 쿼리 스트링을 추가하면 정상 `GET /?x=1`로 나감.
- **localhost vs 127.0.0.1**: Unreal IWebSocket은 `localhost`로는 outbound 요청 자체를 안 보냄 (IPv6 `::1` 선시도 후 fallback 실패 추정). **해결**: 항상 `127.0.0.1` 하드코딩.
- **최종 정상 URL**: `ws://127.0.0.1:9090/?x=1`

---

# Phase 2 — Hardware Verification ✅ (완료)

## ✅ 2.1 STS3215 모터 개별 검증 및 세팅

### Hardware 구성 확정
- **Follower**: Waveshare bus servo driver + 12V 어댑터 + STS3215 12V 모터 x6 (공식 사양)
- **Leader**: Waveshare bus servo driver + 5V 어댑터 + **STS3215-C046 7.4V 모터 x6 (1:147 all joints — 업체 커스텀)**
- 양쪽 Waveshare 보드 모두 점퍼 2개를 **B 채널(USB)**에 설정
- ⚠️ **전원/모터 교차 연결 절대 금지** — 7.4V 모터에 12V 어댑터를 잘못 연결하면 모터 손상 위험

### WSL2 USB 포워딩 셋업
- `usbipd-win`으로 Waveshare 보드를 WSL2에 포워딩
- PowerShell 프로필에 헬퍼 함수 등록: `Attach-Follower`, `Attach-Leader`, `Attach-Both`
- 현재 USB 포트 기준: Follower=`1-7` (→ `/dev/ttyACM0`), Leader=`3-2` (→ `/dev/ttyACM1`)
- ⚠️ WSL 재시작/셧다운 후 usbipd attach 풀림 → 헬퍼 함수로 재연결. Attach 상태 불일치 시 `usbipd detach --busid X-Y` 후 재attach.

### WSL2 Toolchain 설치
- Miniforge 설치 (conda 26.1.1)
- conda env `lerobot` 생성 (Python 3.12.13)
- `conda install ffmpeg -c conda-forge` (ffmpeg 8.0.1 with libsvtav1)
- LeRobot editable 설치:
  ```bash
  cd ~ && git clone https://github.com/huggingface/lerobot.git
  cd lerobot && pip install -e ".[feetech]"
  ```
- ⚠️ 매 WSL 세션에서 `conda activate lerobot` 필요

### 권한 설정
- `sudo usermod -aG dialout $USER` → `/dev/ttyACM*` 접근 권한 부여
- 그룹 변경 반영을 위해 `wsl --shutdown` 후 재시작 필요
- 세션별로 `sudo chmod 666 /dev/ttyACM*` 필요할 수 있음

### 모터 ID 할당 (양쪽 arm 완료)
- `lerobot-find-port`로 각 보드의 포트 확정
- 6개 모터 세팅 순서: gripper(6) → wrist_roll(5) → wrist_flex(4) → elbow_flex(3) → shoulder_lift(2) → shoulder_pan(1)
- 총 12개 모터 전부 ID + baudrate (1,000,000) 할당 완료

### Sanity check
- `robot.bus.sync_read("Present_Position", normalize=False)`로 raw encoder 값 읽기
- Follower 6개, Leader 6개 모두 정상 범위(0~4095) 내 응답 확인

## ✅ 2.2 SO-ARM-101 조립
- Leader arm + Follower arm 양쪽 모두 조립 완료
- **Leader gripper 모터 내부 기어 제거 작업은 수행하지 않음** (업체 영상에서 제시되었으나 C046의 1:147 감속비로도 충분히 passive 동작 가능하다고 판단)
- Leader L2(shoulder_lift)는 업체 커스텀 C046×6 구성의 특성상 중력으로 떨어지는 경향이 공식 구성보다 강함 (예상된 trade-off)

## ✅ 2.3 조립 후 스윕 테스트
- Follower, Leader 양쪽 12개 모터 전부 데이지 체인 응답 확인
- 안전 범위 테스트 동작은 2.4 calibration 후 degrees 단위로 확인하는 쪽이 안전해서 생략 (PROGRESS.md의 기존 step은 건너뛰고 바로 2.4로 진행)

## ✅ 2.4 LeRobot Calibration

### Follower calibration
```bash
lerobot-calibrate \
    --robot.type=so101_follower \
    --robot.port=/dev/ttyACM0 \
    --robot.id=so101_twin_follower
```
- 성공. 파일: `~/.cache/huggingface/lerobot/calibration/robots/so101_follower/so101_twin_follower.json`

### Leader calibration
```bash
lerobot-calibrate \
    --teleop.type=so101_leader \
    --teleop.port=/dev/ttyACM1 \
    --teleop.id=so101_twin_leader
```
- **첫 시도 실패**: `ValueError: Magnitude 3188 exceeds 2047` (set_half_turn_homings에서 homing offset이 12-bit signed 범위 초과)
- **해결**: Leader 전원 재인가(5V 어댑터 detach/reattach) + usbipd 재attach(`usbipd detach --busid 3-2` → `Attach-Leader`) 후 재시도 → 성공
- 파일: `~/.cache/huggingface/lerobot/calibration/teleoperators/so101_leader/so101_twin_leader.json`

### Degrees 단위 값 확인 (검증 성공)

**Import 경로 주의**: LeRobot 버전에 따라 경로가 다름. 현재 설치 환경 기준:
- `from lerobot.robots.so_follower import SO101Follower, SO101FollowerConfig`
- `from lerobot.teleoperators.so_leader import SOLeader, SO101LeaderConfig`
- Follower는 `SO101FollowerConfig`가 `id` 파라미터를 받지만, Leader는 base `SOLeaderConfig`에 `id`가 없고 **`SO101LeaderConfig`** (= `SOLeaderTeleopConfig`, `TeleoperatorConfig` 상속)를 써야 함

**Follower 각도 읽기 예시**:
```python
from lerobot.robots.so_follower import SO101Follower, SO101FollowerConfig
cfg = SO101FollowerConfig(port="/dev/ttyACM0", id="so101_twin_follower")
robot = SO101Follower(cfg)
robot.connect()  # calibration 파일 자동 로드
obs = robot.get_observation()
# obs: {"shoulder_pan.pos": -5.89, "shoulder_lift.pos": -104.26, ...}
robot.disconnect()
```

**Leader 각도 읽기 예시**:
```python
from lerobot.teleoperators.so_leader import SOLeader, SO101LeaderConfig
cfg = SO101LeaderConfig(port="/dev/ttyACM1", id="so101_twin_leader")
leader = SOLeader(cfg)
leader.connect()
obs = leader.get_action()  # Leader는 get_observation 대신 get_action
leader.disconnect()
```

### Open questions 정리 (모두 closed)
- ~~드라이버 보드 종류~~: **Waveshare** ✅
- ~~전원 어댑터 사양~~: Follower **12V**, Leader **5V** ✅
- ~~Leader 모터 구성~~: C046×6 업체 커스텀으로 확정, 공식 사양과 다른 점 인지하고 진행 ✅
- ~~Gripper 기어 제거~~: 수행하지 않음 ✅

---

# Phase 3 — SO-ARM-101 → ROS2 Humble 통합 ✅ (완료)

## ✅ 3.1 ROS2 드라이버 선택: **옵션 A 확정 (LeRobot Python API 래핑)**

### 검토한 옵션들

**옵션 A (채택)**: LeRobot의 `SO101Follower`/`SOLeader` 클래스를 rclpy ROS2 노드로 얇게 래핑.
- `/joint_states` 퍼블리시 + `/joint_commands` 구독하는 100~200줄짜리 어댑터 노드

**옵션 B (기각)**: 커뮤니티 ROS2 드라이버
- `legalaspro/so101-ros-physical-ai`: 완성도 최상 (ros2_control + MoveIt2 + teleop 통합), 단 **Ubuntu 24.04 + ROS 2 Jazzy 전용**. 우리 환경(Ubuntu 22.04 + Humble)과 비호환. 포팅 비용이 프로젝트 scope 벗어남.
- `msf4-0/so101_ros2`: Humble 기반으로 추정, WSL2 전제, 가볍지만 **ros2_control 없음** (MoveIt 통합 미비), 자체 calibration 사용으로 Phase 2 성과 재사용 어려움.
- `brukg/SO-100-arm`: SO-100 전용, SO-101 leader arm 미지원.
- `JafarAbdi/feetech_ros2_driver`: 범용 feetech 드라이버로 SO-101 전용 로직 없음 → 사실상 옵션 C에 가까워짐.

**옵션 C (기각)**: scservo_sdk + rclcpp로 처음부터 직접 작성. 작업량 최대, Phase 5(Unreal) 진입 지연.

### 옵션 A 선정 이유
1. **환경 호환성 리스크 제로**. Ubuntu 22.04 + ROS2 Humble + LeRobot + WSL2 스택 위에서 바로 동작.
2. **Phase 2 성과 100% 재사용**. Calibration JSON, `get_observation()`/`get_action()`/`send_action()` API 그대로 사용. Degrees 단위 값이 이미 검증됨.
3. **프로젝트 scope 준수**. 우리 목표는 "Unreal 디지털 트윈"이고 ROS2 드라이버는 얇은 bridge 역할이면 충분.
4. **성능 충분**. SO-ARM-101 수준에서 Python rclpy 30~60Hz 루프로 충분. 100Hz+ 필요하면 나중에 옵션 C로 교체 가능.

### ⚠️ 옵션 A 설계 변경: Two-Process + ZMQ IPC (Phase 3.3에서 결정)

**원래 계획**: 단일 Python 프로세스에서 `rclpy` + LeRobot 동시 사용.

**변경 이유**: LeRobot 0.5.2는 `requires-python >= 3.12`를 요구하고, `ros-humble-rclpy`는 CPython 3.10 ABI 바이너리로 lock-in. **한 프로세스에서 공존 불가능**.

구 LeRobot 버전(0.3.4, Python 3.10 호환)으로 다운그레이드하는 방안도 검토했으나, SO101 지원 여부 불확실 + Phase 2 calibration JSON 포맷 호환성 리스크 + API 경로 변경 가능성 때문에 기각.

**최종 아키텍처**: Two-process + ZeroMQ IPC
```
┌─────────────────────────────────┐       ┌──────────────────────────────────┐
│ lerobot_worker.py               │       │ ros_bridge_node.py               │
│ (conda env 'lerobot', Py 3.12) │       │ (system Py 3.10 + rclpy)        │
│ - LeRobot API (SO101Follower,   │ ZMQ   │ - rclpy publish/subscribe       │
│   SOLeader)                     │◄─────►│ - /joint_states publisher        │
│ - /dev/ttyACM* 독점 점유        │ IPC   │ - /leader_joint_states publisher │
│ - ZMQ PUB :5555 (joint states) │       │ - /follower_joint_commands sub   │
│ - ZMQ REP :5556 (commands)     │       │ - ZMQ SUB :5555 / REQ :5556     │
└─────────────────────────────────┘       └──────────────────────────────────┘
```

이 설계의 부수적 이점:
- STS3215 half-duplex 제약 (Appendix A #11)이 자연스럽게 해결됨 — tty를 점유하는 프로세스가 worker 하나로 명확히 격리
- Worker crash 시에도 bridge node는 살아있어 ROS2 쪽 graceful degradation 가능
- LeRobot의 heavyweight dependency (torch 등)가 ROS2 노드와 분리되어 startup 빠름

### 참고: legalaspro 레포는 자료로 활용
- 레포 전체는 포팅 비용이 크지만, **URDF/STL 메시는 ROS 버전 독립적**이라 파일 단위로 가져다 쓸 수 있음. Phase 3.2에서 `so101_description` 패키지 전체를 가져옴.

## ✅ 3.2 URDF 확보

### URDF 소스 조사 및 선정

두 후보를 비교 조사:

**후보 A: `legalaspro/so101-ros-physical-ai`의 `so101_description/` 패키지** (채택)
- ROS2 xacro 구조 (leader/follower variant 분리)
- 완전한 ROS2 패키지 (package.xml + CMakeLists.txt + launch + RViz config)
- 16개 STL 메시 포함
- ros2_control, 카메라 xacro 옵션 포함 (on/off 가능)
- 라이선스: Apache 2.0

**후보 B: `TheRobotStudio/SO-ARM100`의 `Simulation/SO101/`** (참고용)
- 평문 URDF 2종 (new_calib / old_calib) + MuJoCo XML
- ROS2 패키지 구조 없음 (package.xml, CMakeLists.txt, launch 파일 없음)
- 라이선스: Apache 2.0

**핵심 발견**: legalaspro의 URDF는 TheRobotStudio 공식 URDF를 기반으로 ROS2용 xacro로 리팩토링한 파생물. Joint 이름, limit 값, 메시 파일명이 전부 정확히 일치.

### Joint 이름 검증 결과 — LeRobot calibration과 100% 일치
| # | URDF joint name | LeRobot calibration name | 일치 |
|---|---|---|---|
| 1 | `shoulder_pan` | `shoulder_pan` | ✅ |
| 2 | `shoulder_lift` | `shoulder_lift` | ✅ |
| 3 | `elbow_flex` | `elbow_flex` | ✅ |
| 4 | `wrist_flex` | `wrist_flex` | ✅ |
| 5 | `wrist_roll` | `wrist_roll` | ✅ |
| 6 | `gripper` | `gripper` | ✅ |

### Joint limit (URDF 기준, radians)
| Joint | Lower | Upper |
|---|---|---|
| shoulder_pan | -1.91986 | 1.91986 |
| shoulder_lift | -1.74533 | 1.74533 |
| elbow_flex | -1.69 | 1.69 |
| wrist_flex | -1.65806 | 1.65806 |
| wrist_roll | -2.74385 | 2.84121 |
| gripper | -0.174533 | 1.74533 |

⚠️ 이 limit은 공식 SO-ARM101 기준. Leader 업체 커스텀(C046×6)의 기어비 차이는 URDF가 아닌 LeRobot calibration에서 이미 흡수됨 (Phase 2.4에서 degrees 값 정상 읽기로 검증). 따라서 URDF limit은 그대로 사용. MoveIt 동역학 계산 시 effort/velocity limit 조정 여지는 있음 (Phase 7에서 필요 시 처리).

### 설치 과정

1. `legalaspro/so101-ros-physical-ai`에서 `so101_description/` 폴더만 sparse-checkout으로 가져와 `~/UnrealRobotics/src/so101_description/`에 배치
2. ⚠️ **conda/miniforge PATH 충돌 해결 필요** — `conda deactivate`만으로 부족. Miniforge가 `~/.bashrc`에서 PATH에 `/home/tmddn/miniforge3/bin`을 주입하여 `colcon build` 시 시스템 Python 대신 miniforge Python이 사용됨 → `catkin_pkg` 못 찾는 에러 발생. 해결: `export PATH=$(echo $PATH | tr ':' '\n' | grep -v miniforge | tr '\n' ':' | sed 's/:$//')` 후 빌드 (Appendix A #12 참조)
3. `colcon build --packages-select so101_description --symlink-install` 성공
4. xacro → URDF 변환 검증: Follower 356줄, Leader 369줄 정상 생성

### launch 파일 수정
- `display.launch.py`가 존재하지 않는 `so101_new_calib.urdf`를 참조하고 있었음 (TheRobotStudio 공식 레포의 plain URDF). legalaspro 패키지에는 이 파일이 없어서 launch 실패.
- **수정**: xacro 기반으로 변경하고 `variant` 인자 (`leader`/`follower`) 지원하도록 `OpaqueFunction` 패턴 적용. 원본은 `display.launch.py.bak`으로 백업.
- 수정 후 `ros2 launch so101_description display.launch.py variant:=follower` 정상 동작 확인.

### RViz 시각화 검증
- WSLg를 통해 RViz2 창이 Windows 데스크톱에 정상 표시
- `joint_state_publisher_gui` 슬라이더 6개 (shoulder_pan, shoulder_lift, elbow_flex, wrist_flex, gripper, wrist_roll) 표시
- 슬라이더 조작 시 3D 로봇 모델 관절이 실시간으로 따라 움직임 확인
- Global Status: Ok, RobotModel Status: Ok
- TF 프레임 라벨 정상 표시 (upper_arm_link, moving_jaw_so101_v1_link, gripper_frame_link 등)

## ✅ 3.3 LeRobot ↔ ROS2 Bridge 구현 및 검증

### 환경 제약 조사 결과
- 시스템 Python: 3.10.12 (Ubuntu 22.04 기본)
- `ros-humble-rclpy`: apt 설치, `_rclpy_pybind11.cpython-310-*.so` — **CPython 3.10 ABI lock-in**
- conda env `lerobot`: Python 3.12.13, LeRobot 0.5.2 (`requires-python >= 3.12`)
- **결론**: rclpy(3.10)와 lerobot(≥3.12)은 한 프로세스에 공존 불가능
- 구 버전 다운그레이드(0.3.4)는 SO101 지원 여부 불확실 + calibration 호환성 리스크로 기각

### Two-Process + ZMQ IPC 아키텍처 채택
- **lerobot_worker.py** (conda env, Python 3.12): LeRobot API로 실물 로봇 관절 읽기/쓰기, ZMQ PUB(5555)/REP(5556) 서버
- **ros_bridge_node.py** (system Python 3.10 + rclpy): ZMQ SUB/REQ 클라이언트, ROS2 토픽 발행/구독
- pyzmq: 양쪽 환경에 설치 (conda: `pip install pyzmq`, system: `sudo pip3 install pyzmq numpy`)

### CycloneDDS 필수 발견
- FastDDS는 WSL2 mirrored mode에서 ROS2 노드 간 DDS discovery 실패 (loopback 인터페이스가 multicast 미지원)
- `sudo apt install -y ros-humble-rmw-cyclonedds-cpp` 설치
- **모든 ROS2 터미널에 필수 환경변수**:
  ```bash
  export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
  export ROS_LOCALHOST_ONLY=1
  ```
- `ros2 daemon stop` → `ros2 daemon start` (한 줄씩 실행) 후 정상 동작

### 패키지 구조
```
~/UnrealRobotics/src/lerobot_ros2_bridge/
├── package.xml                            # ament_python 패키지
├── setup.py / setup.cfg
├── resource/lerobot_ros2_bridge           # ament marker
├── lerobot_ros2_bridge/
│   ├── __init__.py
│   └── ros_bridge_node.py                 # rclpy 노드 (system py3.10에서 실행)
└── scripts/
    ├── lerobot_worker.py                  # LeRobot + ZMQ (conda env에서 실행)
    └── joint_trajectory_action_server.py  # MoveIt FollowJointTrajectory (Phase 7에서 추가)
```

### End-to-end 검증 성공 (dummy 데이터)
- dummy worker (sine wave ±30° at 30Hz) → ZMQ PUB → bridge node → `/joint_states` (radians) → `robot_state_publisher` → TF → RViz
- RViz에서 SO-ARM-101 3D 모델이 sine wave에 따라 실시간으로 움직이는 것 확인
- 6개 joint 이름 정확, radians 변환 정상
- `ros2 topic echo /joint_states --once`로 데이터 흐름 검증

### RViz 연동 시 주의사항
- `display.launch.py`는 `joint_state_publisher` 또는 `joint_state_publisher_gui`를 함께 띄우므로, bridge node와 `/joint_states` 토픽이 충돌함
- **해결**: launch 파일 대신 `robot_state_publisher`를 직접 실행:
  ```bash
  xacro .../so101_arm.urdf.xacro variant:=follower > /tmp/so101_follower.urdf
  # URDF가 길어서 CLI 인자 전달 불가 — YAML 파일로 파라미터 전달
  python3 -c "
  import yaml
  with open('/tmp/so101_follower.urdf') as f: urdf = f.read()
  params = {'robot_state_publisher': {'ros__parameters': {'robot_description': urdf}}}
  with open('/tmp/rsp_params.yaml', 'w') as f: yaml.dump(params, f, default_flow_style=False)
  "
  ros2 run robot_state_publisher robot_state_publisher --ros-args --params-file /tmp/rsp_params.yaml
  ```
- RViz에서 RobotModel 추가 후 **Description Topic**을 `/robot_description`으로 명시적 설정 필요 (자동 감지 안 됨)
- RViz 터미널에서도 `source ~/UnrealRobotics/install/setup.bash` 필요 (STL 메시 경로가 `package://so101_description/...`이므로)

## ✅ 3.4 실물 로봇 연결 검증 및 텔레옵 구현

### 실물 연결 검증 성공
- dummy worker 대신 `lerobot_worker.py`를 실물 follower + leader에 연결하여 실행
- Worker가 30Hz로 양쪽 arm의 관절 데이터를 ZMQ로 발행 확인
- Bridge node가 ZMQ 메시지를 수신하여 `/joint_states` (follower) + `/leader_joint_states` (leader) 토픽으로 발행
- Follower arm을 살짝 움직였을 때 RViz 3D 모델이 실시간으로 따라 움직이는 것 확인
- Leader 데이터는 `/leader_joint_states` 별도 토픽으로 정상 발행 (RViz는 `/joint_states`=follower만 구독)

### 텔레옵 모드 구현 (`--teleop` 플래그)
- `lerobot_worker.py`에 `--teleop` 플래그 추가
- 텔레옵 모드 활성화 시: 매 루프마다 leader의 관절값을 읽어 follower에 `send_action()`으로 전달
- 텔레옵 시작 전 Enter 키 대기 — leader를 안전한 시작 위치로 잡을 시간 제공
- 텔레옵 후 follower를 다시 읽어 실제 반영된 위치를 ZMQ로 발행
- **End-to-end 성공**: Leader arm 움직이면 → follower arm이 따라 움직이고 → 동시에 RViz 3D 모델도 실시간 업데이트

### 텔레옵 실행 방법
```bash
# 터미널 1 (conda env)
conda activate lerobot
sudo chmod 666 /dev/ttyACM*
cd ~/UnrealRobotics/src/lerobot_ros2_bridge
python scripts/lerobot_worker.py --teleop
# "Move leader arm to starting position, then press Enter to begin..." 에서 Enter
```

### 데이터 흐름 (텔레옵 모드)
```
Leader arm (손 조작)
    ↓ get_action()
lerobot_worker.py (conda, Py3.12)
    ├─→ send_action() → Follower arm (실물 따라 움직임)
    ├─→ get_observation() → follower 현재 위치 재읽기
    └─→ ZMQ PUB (follower + leader 데이터)
         ↓
ros_bridge_node.py (rclpy)
    ├─→ /joint_states (follower) → robot_state_publisher → TF → RViz
    └─→ /leader_joint_states (leader) → (향후 Unreal에서 활용 가능)
```

## ✅ 3.5 Joint Command 구독 검증

### Command 경로 구현 확인
- `ros_bridge_node.py`에 `/follower_joint_commands` 구독 + ZMQ REQ 전달 (`on_joint_command()`)이 이미 구현되어 있음 확인
- `lerobot_worker.py`에 ZMQ REP `send_follower_action` 핸들러가 이미 구현되어 있음 확인
- 코드 변경은 안전 메커니즘 추가에 집중

### Safety Clamp 구현
- **Baseline clamp**: worker 시작 시 follower 초기 위치를 기록, 명령은 baseline ± `--cmd-limit`(기본 5°) 범위로 클램핑
- **Physical limit clamp**: calibration range_min/range_max에서 실측한 LeRobot degrees 기준 절대 한계 적용
- 이중 보호: effective range = baseline clamp ∩ physical limits
- `--cmd-joints` 인자로 명령 허용 관절 제한 (기본: shoulder_pan만, `all`로 전체 허용)
- 클램핑 발생 시 로그 출력 (`[BASELINE LIMIT]` / `[PHYSICAL LIMIT]` 구분)

### URDF limit vs LeRobot degrees 좌표계 불일치 발견 및 해결
- **발견**: URDF 0° (home pose) ≠ LeRobot 0° (calibration midpoint). 서로 다른 좌표계라서 URDF limit을 LeRobot degrees에 직접 적용하면 잘못된 결과 발생 (예: shoulder_lift rest=-103.47°인데 URDF limit -100°가 적용되어 6.5° 범위만 허용)
- **해결**: URDF limit 대신 **실측 물리적 한계**를 사용. 토크 해제 후 두 포인트 측정 → 선형 보간으로 calibration range_min/max를 LeRobot degrees로 변환
- **실측 결과** (follower, `JOINT_LIMITS_DEG`):
  ```
  shoulder_pan:  -119.91° ~ +119.91°
  shoulder_lift: -104.62° ~ +104.62°
  elbow_flex:     -97.01° ~  +97.01°
  wrist_flex:    -102.68° ~ +102.68°
  wrist_roll:    -180.00° ~ +180.00°
  gripper:         +0.14° ~  +99.45°
  ```

### Calibration JSON 경로 수정
- 실제 경로: `~/.cache/huggingface/lerobot/calibration/robots/so_follower/` (기존 코드의 `so101_follower`에서 수정)
- Leader: `~/.cache/huggingface/lerobot/calibration/teleoperators/so_leader/`

### 테스트 스크립트 (`test_joint_cmd.py`) 작성
- `/follower_joint_commands` 토픽에 JointState 명령 발행
- 모드: `--absolute`, `--offset`, `--sweep` (단일 관절), `--sweep-all` (전 관절 순차)
- sweep은 `/joint_states`를 구독하여 **현재 위치 기준**(baseline-relative)으로 사인파 왕복
- `--sweep-rate` 인자로 전송 주파수 조정 (기본 30Hz, 10Hz에서는 떨림 발생)

### End-to-end 검증 성공
1. **단발 명령**: `--absolute -2.0` → shoulder_pan 실물 동작 확인 ✅
2. **연속 sweep**: `--sweep --sweep-amp 3.0` → 부드러운 왕복 ✅
3. **클램핑 동작**: 범위 초과 값 전송 시 worker에서 정상 클램핑 ✅
4. **전 관절 순차 테스트**: `--sweep-all --sweep-amp 5.0` → 6개 관절 모두 정상 동작 ✅
5. **±10° 확장 테스트**: `--sweep-all --sweep-amp 10.0` → physical limit과 baseline clamp 이중 보호 동작 확인 ✅

### 데이터 흐름 (command 경로)
```
test_joint_cmd.py (ROS2 터미널, Py3.10)
    ↓ /follower_joint_commands (JointState, radians)
ros_bridge_node.py (rclpy)
    ↓ ZMQ REQ (degrees)
lerobot_worker.py (conda, Py3.12)
    ├─→ safety clamp (baseline ± limit ∩ physical limits)
    ├─→ send_action() → Follower arm (실물 동작)
    └─→ get_observation() → ZMQ PUB → /joint_states → RViz 피드백
```

---

# Phase 4 — RViz 시각화 ✅ (완료)

## ✅ 4.1 RViz2 기본 셋업 (Phase 3.3에서 조기 달성)
- RViz2 실행, URDF 로드 (`robot_state_publisher` 직접 실행)
- `/joint_states` 구독 → 3D 모델 실시간 업데이트 확인 (dummy 데이터)
- TF 트리 정상 (`/tf`, `/tf_static` 토픽 확인)

## ✅ 4.2 실물 ↔ RViz 동기화 검증 (Phase 3.4에서 달성)
- Follower arm 움직임 → RViz 모델 실시간 반영 확인
- 텔레옵 모드에서 Leader arm 조작 → Follower + RViz 동시 동기화 확인

---

# Phase 5 — Unreal에 3D 로봇 파트 올리기 ✅ (완료)

## ✅ 5.1 Mesh 준비

### STL 파일 현황
- 소스: `legalaspro/so101-ros-physical-ai` 레포의 `so101_description/meshes/` (16개 STL)
- Follower에 사용되는 고유 메시: **13개** (Leader 전용 3개 제외: `handle_so101_v1`, `trigger_so101_v1`, `wrist_roll_so101_v1`)
- STL 단위: **미터(m)** (base_so101_v2 기준 약 11cm × 7cm × 8.7cm)

### 블렌더 변환 (STL → FBX)
- Blender 5.1.1 사용
- **Import 설정**: Forward/Up 축은 기본값 그대로 (URDF mesh offset이 원본 원점 기준이므로 축 변경 금지)
- **Export 설정**:
  - Forward: `-Y Forward` (블렌더 5.x 기본값 `-Z Forward`에서 변경 필요)
  - Up: `Z Up`
  - Apply Transform: 체크
  - Apply Scalings: `FBX All`
- 스케일 변경 없음 — 블렌더가 FBX 헤더에 cm 단위 정보를 자동 포함

### 메시 불량 삼각형(degenerate triangle) 문제 발견 및 해결
- Onshape → STL 변환 시 곡면 부분에서 면적 0에 가까운 불량 삼각형 다수 발생
- Unreal에서 메시가 구멍 뚫린 것처럼 렌더링되는 현상
- **해결**: Blender에서 Decimate (ratio 0.1~0.2) + Auto Smooth + 수동 메시 수정. 13개 전부 처리.

## ✅ 5.2 Unreal에 임포트
- 13개 FBX를 `Content/Robot/Meshes/`에 임포트
- Import Scale: **1** (블렌더 FBX가 이미 cm 단위 포함하므로 Fbx File Information의 File Units가 centimeter로 표시됨)
- Generate Collision: 해제 (시각화 목적)
- 에셋 이름: 원본 STL 이름 그대로 (예: `base_so101_v2`, `sts3215_03a_v1`)
- 에셋 경로: `/Game/Robot/Meshes/<이름>`

## ✅ 5.3 로봇 액터 구현

### 신규 파일 3개
```
Source/SO101_Twin/Robot/
├── RosCoordConv.h         # 좌표 변환 유틸리티 (Phase 5.4)
├── RobotVisualizer.h      # 로봇 액터 헤더
└── RobotVisualizer.cpp    # 로봇 액터 구현
```

### 컴포넌트 계층 구조 (URDF 기반)
```
RootComponent (Scene)
└── BaseLink (Scene) ← StaticMeshComponent ×4
    └── ShoulderPanJoint (Scene) ← joint angle 적용 (Z축 회전)
        └── ShoulderLink (Scene) ← StaticMeshComponent ×3
            └── ShoulderLiftJoint (Scene)
                └── UpperArmLink (Scene) ← StaticMeshComponent ×2
                    └── ElbowFlexJoint (Scene)
                        └── LowerArmLink (Scene) ← StaticMeshComponent ×3
                            └── WristFlexJoint (Scene)
                                └── WristLink (Scene) ← StaticMeshComponent ×2
                                    └── WristRollJoint (Scene)
                                        └── GripperLink (Scene) ← StaticMeshComponent ×2
                                            └── GripperJoint (Scene)
                                                └── MovingJawLink (Scene) ← StaticMeshComponent ×1
```

- 7개 Link = USceneComponent (각 링크에 속하는 메시들의 부모)
- 6개 Joint = USceneComponent (ROS joint angle을 로컬 Z축 회전으로 적용)
- 17개 StaticMeshComponent (13개 고유 메시, `sts3215_03a_v1`은 4회 재사용)
- `JointComponentMap` (TMap<FName, USceneComponent*>)으로 ROS joint 이름 → Joint 컴포넌트 매핑
- 메시 로딩은 `BeginPlay`에서 `StaticLoadObject`로 수행 (에디터에서는 빈 액터, Play 시 메시 표시)

### .Build.cs 수정
- `PublicIncludePaths`에 `RosBridge/`와 `Robot/` 폴더 추가 (하위 폴더 간 include 해결)
```csharp
PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "RosBridge"));
PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Robot"));
```

### Joint 각도 적용 방식
- URDF joint origin의 RPY가 "기본 회전(base rotation)"
- ROS `/joint_states`의 position 값이 "관절 각도(joint angle)"
- 최종 회전 = `BaseQuat * JointQuat` (쿼터니언 합성)
- JointQuat는 로컬 Z축(UpVector) 기준 회전 (URDF axis="0 0 1")

## ✅ 5.4 좌표 변환 헬퍼 구현
- `RosCoordConv.h` — namespace에 FORCEINLINE 함수들:
  - `RosToUePosition`: m→cm, Y 반전
  - `RosRpyToUeRotator`: rad→deg, pitch/yaw 부호 반전
  - `RosQuatToUe`: Y/W 부호 반전
  - `RosJointAngleToUeDegrees`: 단일 관절 각도 변환
  - `UeToRosPosition`: 역변환

## ✅ 5.5 /joint_states 구독 → 로봇 포즈 반영
- `ARobotVisualizer`가 `BeginPlay`에서 `URosBridgeSubsystem`에 연결 → `/joint_states` 구독
- `OnRosMessage` → `ParseAndApplyJointStates`에서 `sensor_msgs/JointState` JSON 파싱
- joint name 배열 + position(radians) 배열에서 `JointComponentMap`으로 매칭 → 각 Joint 컴포넌트에 회전 적용

### End-to-end 검증 성공
- 실물 follower arm 손으로 움직이기 → Unreal 모델 실시간 반영 ✅
- 텔레옵 (leader → follower → Unreal) 실시간 동기화 ✅

### 알려진 비주얼 이슈 (미래 작업으로 보류)
- **그리퍼 닫힌 정도 차이**: URDF home pose(0°)와 LeRobot calibration zero point 사이의 오프셋 차이. 실물과 Unreal의 gripper 위치가 살짝 다름.
- **rest 포즈에서 lower_arm ↔ upper_arm 부품 겹침**: 동일 원인 (URDF/LeRobot zero point 차이).
- **해결 방안**: Unreal 측에서 joint별 zero point 오프셋 보정값을 `UPROPERTY(EditAnywhere)`로 추가하여 에디터에서 미세 조정. Calibration JSON은 건드리지 않음 (실물 동작에 영향).

### 실행 방법 (Unreal 연동 전체)
```
⚠️ PowerShell에서 먼저: Attach-Both (USB 연결)

터미널 1 (conda): lerobot_worker.py [--teleop]
터미널 2 (ROS2): ros2 run lerobot_ros2_bridge bridge_node
터미널 3 (ROS2): ros2 launch rosbridge_server rosbridge_websocket_launch.xml
Unreal Editor: Play → RobotVisualizer가 자동 연결 및 구독
```

---

# Phase 6 — Unreal → ROS 송신 파이프 확장 ✅ (완료)

## ✅ 6.1 Publish API 추가
- `URosBridgeSubsystem::Advertise(Topic, Type)` 구현 — rosbridge v2 `{"op":"advertise"}` JSON 전송
- `URosBridgeSubsystem::Publish(Topic, MsgJson)` 구현 — MsgJson을 JSON 객체로 파싱 후 `{"op":"publish","topic":"...","msg":{...}}` 형태로 전송. 미리 Advertise하지 않은 토픽에 publish하면 경고 후 드롭.
- `AdvertisedTopics` TMap으로 트래킹 (재연결 시 `RestoreAdvertisements()`로 복원)
- SKILL.md 섹션 3의 "advertise 먼저, 그 다음 publish" 원칙 준수

## ✅ 6.2 Connected 델리게이트 추가
- `FOnRosBridgeConnected` / `FOnRosBridgeDisconnected` 델리게이트 추가
- `RobotVisualizer`, `RosTestActor` 모두 `OnConnected.AddDynamic`으로 연결 이벤트 수신
- 기존 타이머 폴링 방식 (`DoSubscribe()` + `SubscribeDelaySeconds`) 완전 제거
- Subscribe/Advertise를 BeginPlay에서 즉시 큐잉 — 연결 전 호출 시 맵에 저장, 연결 시 자동 전송

## ✅ 6.3 자동 재연결 로직
- `HandleConnectionError`/`HandleClosed`에서 `ScheduleReconnect()` 호출
- 지수 백오프: 1s → 2s → 4s → 8s → 16s → 30s (cap)
- 재연결 성공 시 `RestoreSubscriptions()` + `RestoreAdvertisements()` 자동 복원
- 명시적 `Disconnect()` 호출 시에만 자동 재연결 비활성화
- `HandleConnected`/`HandleConnectionError`/`HandleClosed` 모두 `AsyncTask(GameThread, ...)` 마샬링 추가 (기존에는 HandleConnected만 로그, 마샬링 없었음)

## ✅ 6.4 송신 검증
- `ARosTestActor`에 Publish 테스트 기능 추가: 연결 시 `/unreal_test` (std_msgs/String) Advertise → 1초 간격 반복 타이머로 `"Hello from Unreal #N"` Publish
- WSL2에서 `ros2 topic echo /unreal_test std_msgs/msg/String`으로 수신 확인 성공

### 수정된 파일 목록
```
Source/SO101_Twin/RosBridge/
├── RosBridgeSubsystem.h      # 델리게이트 2개, Advertise/Publish API, 재연결 멤버 추가
├── RosBridgeSubsystem.cpp    # 전체 구현 (큐잉, 복원, 백오프, 스레드 마샬링)
├── RosTestActor.h            # Publish 테스트 설정, OnConnected 콜백, 타이머 폴링 제거
└── RosTestActor.cpp          # Publish 테스트 구현, 델리게이트 기반 전환
Source/SO101_Twin/Robot/
├── RobotVisualizer.h         # OnConnected 콜백, 타이머 폴링 제거
└── RobotVisualizer.cpp       # 델리게이트 기반 전환, Subscribe 큐잉
```

### ⚠️ 발견된 이슈: rosbridge도 CycloneDDS 필수
- rosbridge를 FastDDS(기본값)로 띄우고, `ros2 topic echo`를 CycloneDDS로 띄우면 DDS 도메인이 달라서 토픽이 안 보임
- **해결**: rosbridge 터미널에서도 `export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` + `export ROS_LOCALHOST_ONLY=1` 필수
- Appendix A #14의 "모든 ROS2 터미널" 원칙을 rosbridge에도 반드시 적용해야 함 — 이전까지는 rosbridge가 WebSocket 경로라 FastDDS에서도 동작했기 때문에 누락하기 쉬웠음
- ⚠️ rosbridge 자체의 WebSocket 수신/발신은 FastDDS에서도 작동하지만, rosbridge가 **ROS2 토픽으로 재발행**할 때 DDS를 사용하므로, 다른 노드/CLI와 같은 RMW를 써야 토픽이 보임

---

# Phase 7 — MoveIt 통합 ✅ (완료)

## ✅ 7.1 MoveIt 2 설치
- `sudo apt install -y ros-humble-moveit` 설치
- `ros2 pkg list | grep moveit`로 25개 패키지 확인

## ✅ 7.2 MoveIt Setup Assistant로 설정
- WSLg를 통해 Setup Assistant GUI 실행
- **URDF**: `so101_description` 패키지의 `urdf/legacy/so101_follower.urdf` 사용
- **Self-collision matrix**: Generate Collision Matrix로 자동 생성
- **Planning groups**:
  - `arm`: shoulder_pan, shoulder_lift, elbow_flex, wrist_flex, wrist_roll (KDL kinematics solver)
  - `gripper`: gripper (solver 없음)
- **Robot poses**: `home` (전부 0°), `ready` (shoulder_lift=-0.5, elbow_flex=0.5)
- **End effector**: `gripper` (parent_link=gripper_link, parent_group=arm)
- **패키지 생성**: `~/UnrealRobotics/src/so101_moveit_config/`

### 설정 수정 사항
- `package.xml`에 author/maintainer email이 비어있어 빌드 실패 → `user@example.com`으로 채움
- `joint_limits.yaml`에서 `max_velocity: 10`, `max_acceleration: 0`이 정수(int)로 저장되어 MoveIt이 double 타입 에러로 crash → `10.0`, `0.0`으로 수정
- `so101_follower.srdf`에 `shoulder_link` ↔ `lower_arm_link` self-collision disable 추가 — URDF/LeRobot zero point 오프셋으로 rest pose에서 메시 겹침이 false positive로 감지됨 (실물은 충돌하지 않음)

## ✅ 7.3 Controller 인터페이스 결정 및 구현

### ros2_control vs FollowJointTrajectory action server 비교
- **옵션 A (채택)**: 기존 ZMQ bridge에 `FollowJointTrajectory` action server 추가. 구현량 작고 기존 아키텍처 유지.
- **옵션 B (기각)**: ros2_control hardware_interface로 리팩터링. 표준적이지만 구현량 크고 아키텍처 변경 필요.
- SO-ARM-101 수준에서는 옵션 A로 충분. 정밀도가 필요하면 Phase 8 이후 옵션 B로 마이그레이션 가능.

### 신규/수정 파일
```
~/UnrealRobotics/src/so101_moveit_config/
├── config/
│   ├── moveit_controllers.yaml   # (신규) arm_controller: FollowJointTrajectory
│   ├── joint_limits.yaml         # (수정) int→double
│   ├── so101_follower.srdf       # (수정) shoulder_link↔lower_arm_link collision disable
│   └── so101_follower.urdf       # (신규, 복사) legacy URDF
├── launch/
│   ├── demo.launch.py            # (수정) custom controller config + robot_state_publisher 분리
│   └── demo.launch.py.bak        # 원본 백업
~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/
└── joint_trajectory_action_server.py  # (신규) FollowJointTrajectory → ZMQ → worker
```

### moveit_controllers.yaml
```yaml
moveit_simple_controller_manager:
  controller_names:
    - arm_controller
  arm_controller:
    type: FollowJointTrajectory
    action_ns: follow_joint_trajectory
    default: true
    joints: [shoulder_pan, shoulder_lift, elbow_flex, wrist_flex, wrist_roll]
```

### joint_trajectory_action_server.py
- `FollowJointTrajectory` action server (`/arm_controller/follow_joint_trajectory`)
- MoveIt이 보낸 trajectory의 각 waypoint를 시간 순서대로 ZMQ REQ로 worker에 전달
- 기존 ZMQ 프로토콜 그대로 사용 (`{"cmd": "send_follower_action", "args": {...}}`)
- `/joint_states` 구독하여 feedback 발행
- Worker의 `JOINT_LIMITS_DEG` physical limit 보호는 그대로 유지

### demo.launch.py 수정
- `moveit_simple_controller_manager` 사용하도록 파라미터 추가
- `robot_state_publisher`는 launch에서 제외 (별도 터미널에서 실행 — bridge node의 `/joint_states`와 충돌 방지)
- `move_group` + `rviz2` 두 노드만 launch

## ✅ 7.4 MoveIt 데모 동작 검증

### End-to-end 성공
- RViz MotionPlanning 패널에서 Goal State → `ready` → **Plan** → 경로 시각화 확인 → **Execute** → 실물 follower arm 동작 확인
- `home` 포즈로 복귀도 정상 동작
- Interactive marker(IK)로 end effector 자유 목표 설정 + Plan + Execute 성공
- Joints 탭 슬라이더(FK)로 개별 관절 목표 설정도 가능

### 알려진 이슈: trajectory 실행 시 떨림
- MoveIt trajectory의 waypoint를 하나씩 개별 ZMQ 명령으로 보내는 방식 때문에 stop-and-go 패턴 발생
- STS3215 서보가 자체 보간을 하지 않아 떨림이 두드러짐
- **완화**: Velocity Scaling을 0.10에서 0.30~0.50으로 올리면 감소
- **근본 해결**: trajectory 다운샘플링 또는 서보 속도/가속도 프로파일 활용 (Phase 8 이후)

## ✅ 7.5 Unreal에서 MoveIt 호출 방법 조사

### rosbridge를 통한 MoveIt 호출 — 세 가지 방법 비교
Phase 8에서 Unreal → MoveIt 연동 시 사용할 방법을 조사:

**방법 A — rosbridge의 action 호출**:
- rosbridge v2는 `call_service`는 지원하지만 action 호출은 공식 지원이 제한적
- MoveIt의 `/move_action`은 action이라 직접 호출이 어려움

**방법 B — 서비스 우회**:
- MoveIt `move_group`에 서비스 기반 인터페이스가 있는지 확인 필요

**방법 C — 중간 ROS2 노드 (추천)**:
- Unreal이 rosbridge로 간단한 토픽(예: `/moveit_goal_pose`)에 목표 publish
- 중간 Python 노드가 MoveIt Python API(`MoveGroupInterface`)로 플래닝+실행
- 가장 안정적이고 유연한 방법

**결정**: Phase 8 시작 시 방법 C를 우선 채택 예정. A/B도 재검토 가능.

### 실행 방법 (MoveIt 연동 전체 — 6개 터미널)
```
⚠️ PowerShell에서 먼저: Attach-Both (USB 연결)

터미널 1 (conda): python scripts/lerobot_worker.py --cmd-joints all --cmd-limit 120
터미널 2 (ROS2): ros2 run lerobot_ros2_bridge bridge_node
터미널 3 (ROS2): python3 ~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/joint_trajectory_action_server.py
터미널 4 (ROS2): ros2 launch so101_moveit_config demo.launch.py
터미널 5 (ROS2): robot_state_publisher (YAML 파라미터로 URDF 전달)
(선택) 터미널 6 (ROS2): ros2 launch rosbridge_server rosbridge_websocket_launch.xml (Unreal 연동 시)
```

⚠️ MoveIt 사용 시 worker는 `--cmd-joints all --cmd-limit 120`으로 실행해야 함. 기본 `--cmd-limit 5`는 baseline ±5°만 허용하여 MoveIt trajectory가 클램핑됨. `--cmd-limit 120`은 baseline clamp를 사실상 비활성화하고 `JOINT_LIMITS_DEG` physical limit만 남김.

---


# Phase 8 — 최종 통합: 양방향 디지털 트윈

## ✅ 8.1 Unreal에서 MoveIt에 목표 전달

### 방법 결정: 방법 C (중간 ROS2 Python 노드) 확정
- Phase 7.5에서 조사한 3가지 방법 중 **방법 C** 채택
- Unreal이 rosbridge로 간단한 토픽에 목표 publish → 중간 Python 노드가 MoveGroup action으로 전달
- `moveit_commander`는 ROS2 Humble에서 미제공 (ROS 1 전용). `MoveItPy`도 소스 빌드 필요
- **최종 방식**: `moveit_msgs/action/MoveGroup` action client를 rclpy로 직접 작성 — 추가 설치 없이 기존 환경에서 동작

### 5DOF 제약 발견 및 해결
- SO-ARM-101은 5DOF arm이므로 임의의 6DOF Cartesian pose (position + orientation)를 동시에 만족시킬 수 없음
- 첫 시도: PoseStamped로 position + orientation constraint 전송 → MoveIt `Unable to sample any valid states for goal tree` 에러 (IK 해 없음)
- **해결**: orientation constraint 제거하고 position-only로 전송. Joint space goal을 기본으로 사용.

### 중간 노드 구현: `moveit_goal_node.py`
- 파일 위치: `~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/moveit_goal_node.py`
- 3가지 토픽 구독:
  1. `/moveit_goal_named` (std_msgs/String) → named target ("home", "ready" 등)
  2. `/moveit_goal_joints` (sensor_msgs/JointState) → joint space goal (radians)
  3. `/moveit_goal_pose` (geometry_msgs/PoseStamped) → Cartesian position-only goal
- `MoveGroup.Goal` 조립 후 `/move_action` action server에 전송
- `MultiThreadedExecutor` + `ReentrantCallbackGroup`으로 동시 콜백 처리
- 실행 중 새 goal 수신 시 무시 (이전 동작 완료 대기)
- 에러코드 → 사람이 읽을 수 있는 문자열 변환

### Named target 내장 (SRDF 기반)
```python
NAMED_TARGETS = {
    'home':  {'shoulder_pan': 0.0, 'shoulder_lift': 0.0, 'elbow_flex': 0.0, 'wrist_flex': 0.0, 'wrist_roll': 0.0},
    'ready': {'shoulder_pan': 0.0, 'shoulder_lift': -0.5, 'elbow_flex': 0.5, 'wrist_flex': 0.0, 'wrist_roll': 0.0},
}
```

### ROS2 측 End-to-end 검증 성공
- `ros2 topic pub --once /moveit_goal_named std_msgs/msg/String '{data: "ready"}'` → 실물 동작 ✅
- `ros2 topic pub --once /moveit_goal_named std_msgs/msg/String '{data: "home"}'` → 복귀 ✅
- `ros2 topic pub --once /moveit_goal_joints sensor_msgs/msg/JointState '{...}'` → 임의 관절 목표 ✅

### Unreal C++ 수정: `ARobotVisualizer`에 MoveIt 명령 기능 추가

**수정된 파일**:
```
Source/SO101_Twin/Robot/
├── RobotVisualizer.h      # MoveIt 명령 UPROPERTY/UFUNCTION 추가
└── RobotVisualizer.cpp    # MoveIt 토픽 advertise + publish 구현
```

**추가된 기능**:
- `AdvertiseMoveItTopics()`: BeginPlay에서 3개 MoveIt 토픽 자동 advertise
  - `/moveit_goal_named` (std_msgs/String)
  - `/moveit_goal_joints` (sensor_msgs/JointState)
  - `/moveit_goal_pose` (geometry_msgs/PoseStamped)
- `SendNamedTarget()`: Details 패널에서 named target 문자열 입력 → publish (`CallInEditor` 버튼)
- `SendJointGoal()`: Details 패널에서 5개 관절 값(radians) 설정 → publish (`CallInEditor` 버튼)
- `SendPoseGoal()`: Details 패널에서 UE 좌표(cm) 입력 → ROS 좌표(m) 변환 → publish (`CallInEditor` 버튼)
- 기존 `/joint_states` 구독 + 3D 모델 실시간 업데이트는 변경 없이 유지

### Unreal End-to-end 검증 성공
- PIE에서 SendNamedTarget("ready") → 실물 follower arm 동작 ✅
- SendNamedTarget("home") → 복귀 ✅
- SendJointGoal (임의 관절 값) → 동작 ✅
- `/joint_states` 피드백으로 Unreal 3D 모델도 실시간 업데이트 ✅

### 데이터 흐름 (Phase 8.1 완성)
```
Unreal (C++, PIE)
    ↓ rosbridge publish (WebSocket, JSON)
        ↓ /moveit_goal_named 또는 /moveit_goal_joints
rosbridge_server (WSL2)
    ↓ ROS2 토픽 재발행
moveit_goal_node.py (rclpy)
    ↓ MoveGroup action goal 조립
        ↓ /move_action (action)
move_group (MoveIt 2)
    ↓ Plan (OMPL) + Execute
        ↓ /arm_controller/follow_joint_trajectory (action)
joint_trajectory_action_server.py
    ↓ ZMQ REQ (degrees)
lerobot_worker.py (conda, Py3.12)
    ├─→ safety clamp (physical limits)
    ├─→ send_action() → Follower arm (실물 동작)
    └─→ get_observation() → ZMQ PUB
         ↓
ros_bridge_node.py → /joint_states
    ↓ rosbridge WebSocket
Unreal (C++) → ParseAndApplyJointStates → 3D 모델 업데이트
```

### 실행 방법 (Phase 8.1 전체 — 8개 터미널)
```
⚠️ PowerShell에서 먼저: Attach-Both (USB 연결)

터미널 1 (conda): lerobot_worker.py --cmd-joints all --cmd-limit 120
터미널 2 (ROS2): ros2 run lerobot_ros2_bridge bridge_node
터미널 3 (ROS2): python3 ~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/joint_trajectory_action_server.py
터미널 4 (ROS2): ros2 launch so101_moveit_config demo.launch.py
터미널 5 (ROS2): robot_state_publisher (YAML 파라미터로 URDF 전달)
터미널 6 (ROS2): python3 ~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/moveit_goal_node.py
터미널 7 (ROS2): ros2 launch rosbridge_server rosbridge_websocket_launch.xml
Unreal Editor: PIE Play → RobotVisualizer Details 패널에서 MoveIt 명령
```

## 📋 8.1b Unreal 뷰포트 인터랙션 (우선순위 하향)
- [ ] 뷰포트 클릭/드래그로 3D 공간에 타겟 포즈 생성
- [ ] 좌표 변환 (UE → ROS) → publish
- ⚠️ 실용적 근거 약함 (5DOF IK 실패율 높음, 실제 공장에서 마우스로 로봇 조작하는 시나리오 거의 없음). 데모용으로만 의미 있어서 우선순위 하향.

## 📋 8.2 실시간 상태 피드백 루프
- [x] 실행 중 `/joint_states`를 Unreal이 구독 → 3D 모델 실시간 업데이트 (Phase 5에서 이미 구현됨)
- [ ] 계획된 경로(플래닝 결과)를 Unreal에 시각화 (선/웨이포인트)

## ⏳ 8.3 오류 및 안전 처리
- [x] E-stop 기능 (Phase 9에서 구현 완료)
- [x] Worker 상태 피드백 → Unreal 뷰포트 표시 (Phase 9에서 구현 완료)
- [ ] 연결 끊김 시 Unreal UI 경고 (OnDisconnected 델리게이트 활용)
- [ ] 플래닝 실패 시 사용자 피드백

---

# Phase 9 — Record / Replay / Sync ✅ (완료)

## ✅ 9.1 방향성 결정

### 디지털 트윈의 핵심 가치 정의
- 디지털 트윈의 본질적 가치는 **모니터링과 상태 파악**: 실물 로봇의 실시간 자세 표시, 관절 값 정상 범위 모니터링, 이력(히스토리) 시각화
- 뷰포트 클릭으로 포즈 지정(8.1b)은 데모용일 뿐 실용적 근거 약함 → 우선순위 하향
- 실제 공장 로봇은 미리 프로그래밍된 작업 시퀀스를 반복 실행 → **Record/Replay가 실용적 핵심 기능**

### MoveIt 경유 vs 직접 제어 비교
- MoveIt 경유: waypoint 개별 전송 → 서보 떨림(Appendix A #33), 시각적으로 좋지 않음
- 직접 제어 (LeRobot `send_action()`): 텔레옵처럼 부드러운 동작
- **결론**: Record/Replay는 MoveIt을 거치지 않고 worker → `send_action()` 직접 경로 사용

### LeRobot 기능 조사
- LeRobot은 `lerobot-record`, `lerobot-replay`, `lerobot-teleoperate` CLI 명령 기본 제공
- ACT, Diffusion Policy 등 imitation learning 학습 지원
- VLA 모델 (Pi0-FAST, GR00T N1.5 등) 지원
- Phone Teleop (스마트폰 → IK 기반 텔레옵) 지원
- **현재 Phase에서는** LeRobot 전체 파이프라인 대신 경량 관절 궤적 기록/재생을 자체 구현 (트랙 A)
- **트랙 B (미래)**: LeRobot imitation learning 학습 → 자율 동작 (카메라 + GPU 필요)

## ✅ 9.2 Worker Record/Replay 명령 구현

### 새 ZMQ 명령 7개 추가 (`lerobot_worker.py`)
- `start_teleop` — leader→follower sync 활성화 (approach 보간 포함)
- `stop_teleop` — sync 비활성화
- `start_record` — 텔레옵이 켜진 상태에서 관절 궤적 버퍼링 시작
- `stop_record` — 기록 중단 + JSON 파일 저장 (텔레옵 유지)
- `start_replay` — 저장된 파일 로드 + approach 보간 후 타이밍 맞춰 재생
- `stop_replay` — 재생 즉시 중단
- `estop` — 어떤 상태에서든 모든 동작 즉시 중단

### 상태 관리
- `self.state`: `"idle"` / `"syncing"` / `"recording"` / `"replaying"`
- `self.teleop`: 텔레옵(sync) 활성화 여부 (state와 독립)
- PUB 메시지에 `"state"`, `"teleop"` 필드 포함 → bridge node → Unreal까지 전파

### 기록 파일 형식
- 저장 경로: `~/recordings/recording_YYYYMMDD_HHMMSS.json`
- 형식:
  ```json
  {
    "version": 1,
    "recorded_at": "2026-05-20T18:10:19",
    "rate_hz": 30.0,
    "num_frames": 493,
    "duration_sec": 17.1,
    "joint_names": ["shoulder_pan", ...],
    "frames": [
      {"ts": 0.0, "joints": {"shoulder_pan": -5.89, ...}},
      ...
    ]
  }
  ```
- `start_replay`에 `filename` 미지정 시 가장 최근 파일 자동 선택
- `loop` 옵션으로 반복 재생 가능

### 안전 기능
- 기록 중 shutdown 시 자동 저장
- E-stop은 상태와 무관하게 즉시 모든 동작 중단 + teleop 해제
- 기존 safety clamp (`JOINT_LIMITS_DEG`) 보호는 replay에서도 그대로 유지

## ✅ 9.3 Approach 보간 (Smooth Transition) 구현

### 문제
- Replay 시작 시 현재 위치와 recording 첫 프레임 사이에 차이가 크면 서보가 최대 속도로 순간 이동 → 기어 충격 부하
- Loop 전환 시 마지막 프레임 → 첫 프레임 사이에도 동일 문제
- Sync On 시 leader와 follower 위치가 다르면 follower가 튀는 현상

### 해결: Cosine Ease-In-Out 보간
- 보간 커브: `factor = (1 - cos(t × π)) / 2` — 시작/끝이 부드럽게 감속/가속
- Duration = 최대 관절 각도 차이 ÷ approach speed (degrees/sec)
- 기본 속도: 45°/s (예: 90° 차이 → 2초, 10° 차이 → 0.3초)
- 최소 duration 0.3초 (너무 짧은 이동에서도 최소한의 부드러움 보장)
- 1° 미만 차이: approach 스킵, 즉시 시작

### 적용 지점 3곳
1. **Sync On (start_teleop)**: follower → leader 현재 위치까지 approach → 완료 후 teleop 자동 활성화 (`"syncing"` 상태)
2. **Start Replay**: follower → recording 첫 프레임까지 approach → 완료 후 재생 시작
3. **Loop 전환**: 마지막 프레임 → 첫 프레임까지 approach → 완료 후 재생 반복

### 설정 가능
- Worker CLI: `--approach-speed 45.0` (기본값)
- Unreal Details 패널: `ApproachSpeed` 슬라이더 (5~300°/s)
- 명령 단위 오버라이드: `"approach_speed": 30.0`

## ✅ 9.4 Bridge Node 명령 중계 구현

### 새 ROS2 토픽 2개 (`ros_bridge_node.py`)
- `/robot_command` (std_msgs/String, 구독) — Unreal에서 JSON 명령 수신 → ZMQ REQ로 worker에 전달
- `/robot_status` (std_msgs/String, 발행) — worker 응답 및 상태 변화를 Unreal에 전달

### 상태 전파
- Worker PUB 메시지의 `"state"`, `"teleop"` 필드를 읽어 상태 변화 시 `/robot_status`에 발행
- 명령 응답도 `/robot_status`에 발행 (성공/에러/타임아웃)

## ✅ 9.5 Unreal C++ 버튼 인터페이스 구현

### Details 패널 새 섹션 (`RobotVisualizer.h/.cpp`)
- **ROS|Sync**: `SyncOn`, `SyncOff` 버튼 + `bSyncActive` 상태 표시
- **ROS|Record**: `StartRecord`, `StopRecord` 버튼
- **ROS|Replay**: `ReplayFilename` 입력, `bReplayLoop` 체크박스, `ApproachSpeed` 슬라이더 (5~300°/s), `StartReplay`, `StopReplay` 버튼
- **ROS|Safety**: `EStop` 버튼
- **ROS|Status**: `WorkerState` (idle/syncing/recording/replaying 실시간 표시)

### 뷰포트 피드백
- 초록: recording 상태, Sync ON, 저장 완료
- 시안: replaying 상태
- 노랑: 중단됨, Sync OFF
- 빨강: E-stop, 에러, 연결 안 됨

### 명령 전달 경로
```
Unreal (Details 버튼 클릭)
    ↓ /robot_command (std_msgs/String, JSON)
rosbridge_server (WebSocket → ROS2)
    ↓ /robot_command
ros_bridge_node.py (rclpy)
    ↓ ZMQ REQ
lerobot_worker.py (conda, Py3.12)
    ├─→ 명령 실행 (teleop/record/replay/estop)
    └─→ ZMQ PUB (상태 + 관절 데이터)
         ↓
ros_bridge_node.py → /robot_status + /joint_states
    ↓ rosbridge WebSocket
Unreal (C++) → 상태 표시 + 3D 모델 업데이트
```

## ✅ 9.6 End-to-end 검증 성공

### 테스트 완료 항목
1. **Sync On** → leader 위치로 부드럽게 approach → teleop 활성화 ✅
2. **Start Record** → Sync가 켜진 상태에서 기록 시작, follower가 leader를 따라감 ✅
3. **Stop Record** → 파일 저장, Sync 유지 ✅
4. **Start Replay** → approach 보간 후 recording 재생, Unreal 3D 모델 실시간 반영 ✅
5. **Loop Replay** → 마지막→처음 approach 보간 후 반복 ✅
6. **Stop Replay** → 즉시 중단 ✅
7. **E-Stop** → 어떤 상태에서든 즉시 정지 ✅
8. **Sync Off** → teleop 해제 ✅

### 워크플로우 (의도된 사용 순서)
```
1. Sync On → leader와 follower 정렬 (보간 이동)
2. Leader arm을 원하는 시작 위치로 이동
3. Start Record → 텔레옵 상태에서 동작 기록
4. 원하는 동작 수행 (leader 조작)
5. Stop Record → 파일 저장
6. Sync Off → teleop 해제
7. Start Replay → 기록된 동작 재생 (반복 가능)
8. Stop Replay → 중단
9. Sync On → 다시 정렬 후 rest position으로 수동 복귀
```

### 수정된 파일 목록
```
WSL2:
  ~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/lerobot_worker.py  (480→978줄)
  ~/UnrealRobotics/src/lerobot_ros2_bridge/lerobot_ros2_bridge/ros_bridge_node.py  (200→318줄)

Windows:
  Source/SO101_Twin/Robot/RobotVisualizer.h   (187→258줄)
  Source/SO101_Twin/Robot/RobotVisualizer.cpp  (472→730줄)
```

### 실행 방법 (Record/Replay 전체 — 4개 터미널)
```
⚠️ PowerShell에서 먼저: Attach-Both (USB 연결)

터미널 1 (conda): lerobot_worker.py --cmd-joints all --cmd-limit 120
터미널 2 (ROS2): ros2 run lerobot_ros2_bridge bridge_node
터미널 3 (ROS2): ros2 launch rosbridge_server rosbridge_websocket_launch.xml
Unreal Editor: PIE Play → RobotVisualizer Details 패널에서 버튼 조작
```

---

# Phase 10 — UI 기획 및 구현 (📋 Planned)

## 📋 10.1 디지털 트윈 모니터링 UI
- [ ] 실시간 로봇 자세 표시 (3D 모델은 이미 구현, 2D 대시보드 추가)
- [ ] 관절 값 정상 범위 모니터링 (한계 근접 시 경고)
- [ ] 이력(히스토리) 시각화 (관절 값 변화 그래프)

## 📋 10.2 Record/Replay 조작 UI
- [ ] 뷰포트 내 조작 패널 (현재 Details 패널 → 독립 UI 위젯으로 승격)
- [ ] Recording 목록 표시 + 선택 UI
- [ ] Replay 진행률 표시 (프로그레스 바)
- [ ] Worker 상태 인디케이터 (idle/syncing/recording/replaying)

## 📋 10.3 안전 UI
- [ ] 연결 상태 인디케이터 (connected/disconnected/reconnecting)
- [ ] E-stop 버튼 (뷰포트 상단, 항상 접근 가능)
- [ ] 오류 알림 패널

---

# Future — LeRobot AI 통합 (트랙 B, 📋 Planned)

## 📋 F.1 Imitation Learning
- [ ] 카메라 추가 (USB 웹캠 → LeRobot observation)
- [ ] `lerobot-record`로 관절 + 카메라 데이터셋 수집
- [ ] ACT 또는 Diffusion Policy로 학습
- [ ] 학습된 정책 → worker에서 자율 실행

## 📋 F.2 VLA (Vision-Language-Action)
- [ ] Pi0-FAST 또는 GR00T N1.5 모델 통합
- [ ] 언어 지시("검은 블록을 집어") → 로봇 자율 동작

---

# Appendix A — Known Gotchas (SKILL.md 요약)

자세한 내용은 `SKILL.md` 섹션 7 참조.

| # | 증상 | 해결 |
|---|---|---|
| 1 | Unreal IWebSocket이 `localhost` URL로 아무 요청도 안 보냄 | `127.0.0.1` 하드코딩 |
| 2 | rosbridge가 `GET //`로 404 반환 | URL에 `?x=1` 더미 쿼리 추가 |
| 3 | `ros2 topic pub/echo` CLI 조합이 WSL2에서 먹통 | `demo_nodes_cpp talker/listener` 사용 |
| 4 | 좌표계 혼란 | UE는 cm/left-handed, ROS는 m/right-handed. Y축 뒤집기 + quaternion Y/W 부호 뒤집기 |
| 5 | WebSocket 콜백에서 UObject 직접 접근 → 크래시 | `AsyncTask(ENamedThreads::GameThread, ...)` 마샬링 필수 |
| 6 | `/dev/ttyACM0` PermissionError | `usermod -aG dialout $USER` + `wsl --shutdown` |
| 7 | usbipd `Attached` 상태인데 WSL2에 장치 없음 | `usbipd detach` 후 재attach |
| 8 | LeRobot `get_observation()` calibration 에러 (sanity check 단계) | `bus.sync_read(..., normalize=False)`로 raw 읽기 |
| 9 | Leader calibration `Magnitude NNNN exceeds 2047` | 전원 cycling + usbipd 재attach 후 재시도 |
| 10 | LeRobot import 경로 | Follower: `lerobot.robots.so_follower`, Leader: `lerobot.teleoperators.so_leader` (SO101LeaderConfig 사용) |
| 11 | STS3215 half-duplex 충돌 | 동일 tty에 두 프로세스 동시 접근 금지 (sanity check ↔ ROS2 bridge 노드 동시 실행 금지) |
| 12 | `colcon build`에서 `No module named 'catkin_pkg'` — miniforge Python 충돌 | `conda deactivate`만으로 부족. PATH에서 miniforge 경로를 수동 제거 필요: `export PATH=$(echo $PATH | tr ':' '\n' | grep -v miniforge | tr '\n' ':' | sed 's/:$//')`. ROS2 작업과 LeRobot(conda) 작업은 반드시 별도 터미널에서 수행. |
| 13 | `display.launch.py`에서 `so101_new_calib.urdf` not found | legalaspro launch 파일이 존재하지 않는 TheRobotStudio plain URDF를 참조. xacro 기반 + variant 인자 지원으로 수정 (원본 `.bak` 백업). |
| 14 | **FastDDS에서 ROS2 노드 간 discovery 실패** (WSL2 mirrored mode) | `sudo apt install ros-humble-rmw-cyclonedds-cpp` + 모든 ROS2 터미널에 `export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` + `export ROS_LOCALHOST_ONLY=1`. `ros2 daemon stop`/`start` (한 줄씩 실행). SKILL.md 섹션 7.4 참조. |
| 15 | **LeRobot ≥0.5.0이 Python ≥3.12 요구 → rclpy(3.10)와 공존 불가** | Two-process + ZMQ IPC 아키텍처 채택. 구 버전 다운그레이드 금지 (calibration 호환성 리스크). SKILL.md 섹션 7.5 참조. |
| 16 | `ros2 run`으로 bridge node 실행 시 `ModuleNotFoundError: No module named 'zmq'` | venv의 site-packages가 `ros2 run`에서 안 보임. **system Python에 직접 설치**: `sudo pip3 install pyzmq numpy`. |
| 17 | `robot_state_publisher`에 URDF CLI 인자 전달 시 parse 에러 | URDF가 길면 CLI 인자로 못 넘김. YAML 파일로 파라미터 전달: `--ros-args --params-file /tmp/rsp_params.yaml` |
| 18 | RViz에서 RobotModel 추가했는데 모델이 안 보임 (Status: Ok) | **Description Topic**을 `/robot_description`으로 명시 설정 필요. 또한 RViz 터미널에서 `source ~/UnrealRobotics/install/setup.bash` 필수 (STL 메시 `package://` 경로 해석에 필요). |
| 19 | `ros2 daemon stop`과 `start`를 한꺼번에 붙여넣으면 안 됨 | 한 줄씩 실행해야 함. 동시 붙여넣기 시 두 번째 명령이 씹힘. |
| 20 | **`ros2 daemon stop`이 첫 실행 시 무반응** — daemon이 없는 상태에서 stop하면 hang | `ros2 daemon start` 한 번 실행 후 `ros2 daemon stop` → `ros2 daemon start` 순서로 수행하면 정상 동작. |
| 21 | **URDF joint limits와 LeRobot degrees는 좌표계가 다름** — URDF 0°(home pose) ≠ LeRobot 0°(calibration midpoint). URDF limit을 LeRobot space에 직접 적용하면 rest position이 이미 limit 밖이 될 수 있음. | URDF limit 대신 calibration range_min/max를 두 포인트 측정으로 LeRobot degrees로 변환하여 사용. `JOINT_LIMITS_DEG` 참조. |
| 22 | **Calibration JSON 경로가 문서와 다름** — `so101_follower`가 아니라 `so_follower` | Follower: `~/.cache/huggingface/lerobot/calibration/robots/so_follower/`, Leader: `~/.cache/.../teleoperators/so_leader/` |
| 23 | **conda env에서 test_joint_cmd.py 실행 시 `rclpy` import 실패** | `test_joint_cmd.py`는 rclpy가 필요하므로 **ROS2 터미널**(system Py3.10)에서만 실행. conda env(Py3.12)에서는 실행 불가. |
| 24 | **STL 메시에 degenerate triangle (면적 0 삼각형)** — Onshape CAD→STL 변환 시 곡면에서 불량 삼각형 다수 발생. Unreal에서 구멍 뚫린 것처럼 렌더링됨. | Blender에서 Decimate (ratio 0.1~0.2) + Auto Smooth + 수동 수정. Remesh는 디테일 손실이 크므로 사용하지 않음. |
| 25 | **블렌더 5.x FBX Export 기본 축이 `-Z Forward, Y Up`으로 변경됨** — 이전 버전은 `-Y Forward, Z Up`이 기본이었으나 5.x에서 바뀜. Unreal FBX 임포터는 `-Y Forward, Z Up`을 기대함. | FBX Export 시 Forward를 `-Y Forward`, Up을 `Z Up`으로 수동 변경 필요. |
| 26 | **블렌더 STL Import 시 축 설정 변경 금지** — 보기 좋게 세우려고 Import 축을 바꾸면 URDF mesh offset과 불일치. | STL Import 시 Forward/Up 축은 기본값 그대로 유지. 블렌더에서 이상하게 누워보여도 무시. |
| 27 | **UE 하위 폴더 간 include 실패** — `Robot/`에서 `RosBridge/`의 헤더를 include 못 찾음 (`C1083: No such file or directory`). UE가 모듈 내 하위 폴더를 자동으로 include path에 넣지 않음. | `.Build.cs`에 `PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "폴더명"))` 추가. |
| 28 | **URDF/LeRobot zero point 오프셋으로 인한 Unreal 비주얼 차이** — gripper 닫힌 정도, rest 포즈 부품 겹침. URDF home pose(0°) ≠ LeRobot calibration zero point. | Calibration JSON 수정 금지 (실물 영향). Unreal 측에서 joint별 오프셋 보정값을 UPROPERTY로 추가하여 에디터에서 미세 조정. |
| 29 | **rosbridge를 FastDDS로 띄우면 CycloneDDS 터미널에서 토픽이 안 보임** — rosbridge 자체의 WebSocket 경로는 FastDDS에서도 동작하지만, ROS2 토픽 재발행 시 DDS를 사용하므로 다른 노드/CLI와 RMW가 달라지면 토픽이 보이지 않음. `ros2 topic list`에 rosbridge가 발행한 토픽이 안 뜸. | rosbridge 터미널에서도 `export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` + `export ROS_LOCALHOST_ONLY=1` 필수. Appendix A #14의 "모든 ROS2 터미널" 원칙을 rosbridge에도 예외 없이 적용. |
| 30 | **MoveIt Setup Assistant가 `joint_limits.yaml`에 정수 값을 저장** — `max_velocity: 10`, `max_acceleration: 0`으로 저장되어 move_group이 `InvalidParameterTypeException: expected [double] got [integer]`로 crash. | `joint_limits.yaml`에서 모든 수치를 `10.0`, `0.0` 등 실수(float)로 수정. |
| 31 | **URDF/LeRobot zero point 오프셋으로 MoveIt self-collision false positive** — rest pose에서 `shoulder_link` ↔ `lower_arm_link` 충돌 감지. URDF 메시가 겹쳐 보이지만 실물은 충돌하지 않음. MoveIt이 "Computed path is not valid"로 플래닝 거부. | SRDF에 `<disable_collisions link1="shoulder_link" link2="lower_arm_link" reason="Adjacent"/>` 추가. |
| 32 | **MoveIt demo.launch.py에서 robot_state_publisher를 함께 띄우면 `/joint_states` 충돌** — bridge node가 이미 `/joint_states`를 발행 중이므로 launch가 띄운 것과 충돌. | `demo.launch.py`에서 `robot_state_publisher`를 제외하고 별도 터미널에서 실행. |
| 33 | **MoveIt trajectory 실행 시 서보 떨림** — waypoint를 하나씩 개별 ZMQ 명령으로 보내는 stop-and-go 패턴. STS3215가 자체 보간을 하지 않음. | Velocity Scaling을 0.30~0.50으로 올려 완화. 근본 해결은 trajectory 다운샘플링 또는 서보 프로파일 활용. |
| 34 | **`moveit_commander`가 ROS2 Humble에서 미제공** — `ros-humble-moveit-commander` 패키지가 존재하지 않음. `moveit_commander`는 ROS 1 전용. | `moveit_msgs/action/MoveGroup` action client를 rclpy로 직접 작성. MoveItPy는 소스 빌드 필요하므로 기각. |
| 35 | **SO-ARM-101 (5DOF)에 6DOF Cartesian pose goal 전송 시 IK 실패** — MoveIt `Unable to sample any valid states for goal tree`. 5개 관절로는 position + orientation을 동시에 만족시킬 수 없음. | Orientation constraint를 제거하고 position-only goal로 전송. 또는 joint space goal 사용 (추천). |
| 36 | **Sync On 시 follower가 leader 위치로 순간 이동(튀는 현상)** — 텔레옵 활성화 시 leader와 follower 위치 차이가 크면 서보가 최대 속도로 이동. 기어 충격 부하 + 시각적으로 위험해 보임. | Sync On에 approach 보간 추가 (`"syncing"` 상태). Cosine ease-in-out, 기본 45°/s. 1° 미만 차이 시 스킵. |
| 37 | **Replay loop 전환 시 서보 순간 이동** — recording의 시작/끝 위치가 다르면 loop 전환 시 서보가 최대 속도로 첫 프레임으로 복귀. 반복 재생 시 매 cycle마다 충격 발생. | Loop 전환 시에도 approach 보간 삽입. 마지막 프레임 → 첫 프레임 사이에 cosine 보간. |

---

# Appendix B — Recurring Commands

## WSL2 측 — ROS2 (모든 ROS2 터미널 공통)
```bash
# ⚠️ ROS2 작업 전 필수: miniforge PATH 제거 + CycloneDDS 설정 (매 터미널)
export PATH=$(echo $PATH | tr ':' '\n' | grep -v miniforge | tr '\n' ':' | sed 's/:$//')
source /opt/ros/humble/setup.bash
source ~/UnrealRobotics/install/setup.bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export ROS_LOCALHOST_ONLY=1

# rosbridge 기동
ros2 launch rosbridge_server rosbridge_websocket_launch.xml

# 테스트 퍼블리셔/서브스크라이버 (CLI pub/echo 대신)
ros2 run demo_nodes_cpp talker
ros2 run demo_nodes_cpp listener

# URDF 시각화 (bridge node가 /joint_states를 퍼블리시하는 경우)
# robot_state_publisher만 단독 실행 (joint_state_publisher 없이):
xacro ~/UnrealRobotics/src/so101_description/urdf/so101_arm.urdf.xacro variant:=follower > /tmp/so101_follower.urdf
python3 -c "
import yaml
with open('/tmp/so101_follower.urdf') as f: urdf = f.read()
params = {'robot_state_publisher': {'ros__parameters': {'robot_description': urdf}}}
with open('/tmp/rsp_params.yaml', 'w') as f: yaml.dump(params, f, default_flow_style=False)
"
ros2 run robot_state_publisher robot_state_publisher --ros-args --params-file /tmp/rsp_params.yaml

# RViz2 (별도 터미널)
rviz2
# → Add → RobotModel, Fixed Frame = base_link, Description Topic = /robot_description

# URDF 시각화 (슬라이더 GUI로 관절 조작 — bridge node 없이 독립 테스트):
ros2 launch so101_description display.launch.py variant:=follower
ros2 launch so101_description display.launch.py variant:=leader

# workspace 재빌드 (필요 시)
cd ~/UnrealRobotics
colcon build --packages-select lerobot_ros2_bridge --symlink-install
colcon build --packages-select so101_description --symlink-install
colcon build --packages-select so101_moveit_config --symlink-install
```

## WSL2 측 — LeRobot ↔ ROS2 Bridge (Phase 3.3 아키텍처)

### 실행 순서 (5개 터미널)
```bash
# ─── 터미널 1: LeRobot Worker (conda env) ───
source ~/miniforge3/etc/profile.d/conda.sh
conda activate lerobot
sudo chmod 666 /dev/ttyACM*
cd ~/UnrealRobotics/src/lerobot_ros2_bridge
python scripts/lerobot_worker.py
# 옵션: --follower-only, --leader-only, --rate 30, --teleop
#        --cmd-joints all, --cmd-limit 10.0

# ─── 터미널 1 (텔레옵 모드): ───
python scripts/lerobot_worker.py --teleop
# → "Move leader arm to starting position, then press Enter to begin..."
# → Leader arm 시작 자세 잡고 Enter

# ─── 터미널 2: ROS2 Bridge Node ───
export PATH=$(echo $PATH | tr ':' '\n' | grep -v miniforge | tr '\n' ':' | sed 's/:$//')
source /opt/ros/humble/setup.bash
source ~/UnrealRobotics/install/setup.bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export ROS_LOCALHOST_ONLY=1
ros2 run lerobot_ros2_bridge bridge_node

# ─── 터미널 3: robot_state_publisher ───
# (위 "ROS2 공통" 환경변수 설정 후)
ros2 run robot_state_publisher robot_state_publisher --ros-args --params-file /tmp/rsp_params.yaml

# ─── 터미널 4: RViz2 ───
# (위 "ROS2 공통" 환경변수 설정 후)
rviz2

# ─── 터미널 5: rosbridge (Unreal 연동 시) ───
# (위 "ROS2 공통" 환경변수 설정 후)
ros2 launch rosbridge_server rosbridge_websocket_launch.xml
```

## WSL2 측 — MoveIt + Unreal 연동 (Phase 8.1 아키텍처 — 8개 터미널)
```bash
# ⚠️ PowerShell에서 먼저: Attach-Both (USB 연결)

# ─── 터미널 1: LeRobot Worker (conda env) ───
source ~/miniforge3/etc/profile.d/conda.sh
conda activate lerobot
sudo chmod 666 /dev/ttyACM*
cd ~/UnrealRobotics/src/lerobot_ros2_bridge
python scripts/lerobot_worker.py --cmd-joints all --cmd-limit 120
# ⚠️ MoveIt 사용 시 --cmd-limit 120 필수 (기본 5°는 trajectory 클램핑)

# ─── 터미널 2: ROS2 Bridge Node ───
# (ROS2 공통 환경변수 설정 후)
ros2 run lerobot_ros2_bridge bridge_node

# ─── 터미널 3: FollowJointTrajectory Action Server ───
# (ROS2 공통 환경변수 설정 후)
python3 ~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/joint_trajectory_action_server.py

# ─── 터미널 4: MoveIt Demo Launch ───
# (ROS2 공통 환경변수 설정 후)
ros2 launch so101_moveit_config demo.launch.py

# ─── 터미널 5: robot_state_publisher ───
# (ROS2 공통 환경변수 설정 후)
xacro ~/UnrealRobotics/src/so101_description/urdf/so101_arm.urdf.xacro variant:=follower > /tmp/so101_follower.urdf
python3 -c "
import yaml
with open('/tmp/so101_follower.urdf') as f: urdf = f.read()
params = {'robot_state_publisher': {'ros__parameters': {'robot_description': urdf}}}
with open('/tmp/rsp_params.yaml', 'w') as f: yaml.dump(params, f, default_flow_style=False)
"
ros2 run robot_state_publisher robot_state_publisher --ros-args --params-file /tmp/rsp_params.yaml

# ─── 터미널 6: MoveIt Goal Node (Phase 8.1에서 추가) ───
# (ROS2 공통 환경변수 설정 후)
python3 ~/UnrealRobotics/src/lerobot_ros2_bridge/scripts/moveit_goal_node.py

# ─── 터미널 7: rosbridge (Unreal 연동) ───
# (ROS2 공통 환경변수 설정 후)
ros2 launch rosbridge_server rosbridge_websocket_launch.xml

# ─── Unreal Editor: PIE Play ───
# RobotVisualizer Details 패널 → ROS|MoveIt 섹션에서 명령 전송
```

## WSL2 측 — Record/Replay + Unreal 연동 (Phase 9 아키텍처 — 4개 터미널)
```bash
# ⚠️ PowerShell에서 먼저: Attach-Both (USB 연결)

# ─── 터미널 1: LeRobot Worker (conda env) ───
source ~/miniforge3/etc/profile.d/conda.sh
conda activate lerobot
sudo chmod 666 /dev/ttyACM*
cd ~/UnrealRobotics/src/lerobot_ros2_bridge
python scripts/lerobot_worker.py --cmd-joints all --cmd-limit 120

# ─── 터미널 2: ROS2 Bridge Node ───
# (ROS2 공통 환경변수 설정 후)
ros2 run lerobot_ros2_bridge bridge_node

# ─── 터미널 3: rosbridge (Unreal 연동) ───
# (ROS2 공통 환경변수 설정 후)
ros2 launch rosbridge_server rosbridge_websocket_launch.xml

# ─── Unreal Editor: PIE Play ───
# RobotVisualizer Details 패널:
#   ROS|Sync → Sync On (leader-follower 정렬)
#   ROS|Record → Start Record / Stop Record
#   ROS|Replay → Start Replay / Stop Replay (ApproachSpeed 조절 가능)
#   ROS|Safety → EStop (긴급 정지)
```

## WSL2 측 — LeRobot 단독 (calibration, sanity check 등)
```bash
# ⚠️ LeRobot 전용 터미널에서만 사용 (ROS2 명령과 혼용 금지)
conda activate lerobot

# 권한 (매 세션 필요할 수 있음)
sudo chmod 666 /dev/ttyACM*

# Calibration 재실행 (필요 시)
lerobot-calibrate --robot.type=so101_follower --robot.port=/dev/ttyACM0 --robot.id=so101_twin_follower
lerobot-calibrate --teleop.type=so101_leader --teleop.port=/dev/ttyACM1 --teleop.id=so101_twin_leader

# Calibration 파일 위치
ls ~/.cache/huggingface/lerobot/calibration/robots/so101_follower/
ls ~/.cache/huggingface/lerobot/calibration/teleoperators/so101_leader/

# Follower 각도 읽기
python - <<'EOF'
from lerobot.robots.so_follower import SO101Follower, SO101FollowerConfig
cfg = SO101FollowerConfig(port="/dev/ttyACM0", id="so101_twin_follower")
robot = SO101Follower(cfg)
robot.connect()
obs = robot.get_observation()
for k, v in obs.items():
    if "pos" in k: print(f"  {k:25s} = {v:+7.2f}")
robot.disconnect()
EOF

# Leader 각도 읽기
python - <<'EOF'
from lerobot.teleoperators.so_leader import SOLeader, SO101LeaderConfig
cfg = SO101LeaderConfig(port="/dev/ttyACM1", id="so101_twin_leader")
leader = SOLeader(cfg)
leader.connect()
obs = leader.get_action()
for k, v in obs.items(): print(f"  {k:25s} = {v:+7.2f}")
leader.disconnect()
EOF

# Raw encoder sanity check (calibration 없이)
python - <<'EOF'
from lerobot.robots.so_follower import SO101Follower, SO101FollowerConfig
cfg = SO101FollowerConfig(port="/dev/ttyACM0", id="probe")
robot = SO101Follower(cfg)
robot.connect(calibrate=False)
raw = robot.bus.sync_read("Present_Position", normalize=False)
for name, val in raw.items():
    print(f"  {name:15s} id={robot.bus.motors[name].id}  raw_pos={val}")
robot.disconnect()
EOF
```

## Windows 측 — Unreal 빌드
```powershell
# 네트워크 검증
Test-NetConnection -ComputerName 127.0.0.1 -Port 9090

# Unreal 빌드 (VS 없이)
& "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" `
  SO101_TwinEditor Win64 Development `
  -Project="$PWD\SO101_Twin.uproject" -WaitMutex

# 프로젝트 파일 재생성
& "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" `
  -projectfiles -project="$PWD\SO101_Twin.uproject" -game -rocket -progress
```

## Windows 측 — USB 포워딩 (usbipd)
PowerShell 프로필(`$PROFILE`)에 함수 등록됨:
```powershell
Attach-Follower   # busid 1-7 → /dev/ttyACM0
Attach-Leader     # busid 3-2 → /dev/ttyACM1
Attach-Both       # 둘 다

# 상태 불일치 시
usbipd detach --busid 1-7
Attach-Follower

usbipd detach --busid 3-2
Attach-Leader

# 현재 상태 확인
usbipd list
```

---

# Session Log (Rough)

주요 마일스톤 달성 시점을 기록. 새 세션 시작 시 맥락 파악용.

- **2026-04-09 (Session 1)**: Phase 0 전체 완료, Phase 1 전체 완료. `?x=1` 워크어라운드와 `127.0.0.1` 이슈 발견 및 해결. 첫 end-to-end 메시지 수신 성공. PROGRESS.md, CLAUDE.md, SKILL.md 초안 작성.

- **2026-04-10 (Session 2)**: Phase 2.1 완료. Waveshare 드라이버 보드 2개(Follower 12V, Leader 5V) 확정. usbipd-win으로 WSL2 포워딩 셋업 (PowerShell 프로필 헬퍼 함수 `Attach-Follower`/`Attach-Leader`/`Attach-Both`). WSL2에 Miniforge + conda env `lerobot` (Python 3.12) + LeRobot editable install + feetech extra. `dialout` 그룹 이슈 해결 후 `lerobot-setup-motors`로 Leader + Follower 총 12개 STS3215 모터에 ID 1~6 할당 완료.

- **2026-04-15 (Session 3)**: **Phase 2 전체 완료 + Phase 3.1 결정**.
  - Leader 모터 사양 조사: 업체가 ST3215-C046(1:147) × 6개만 공급하는 커스텀 구성 확인. 공식 SO-ARM101 Leader 사양(C044×2 + C001×1 + C046×3)과 다르지만 기능적으로 동작 가능. Gripper 기어 제거는 수행 안 함.
  - 양쪽 arm 조립 완료. 2.3 스윕 테스트는 raw 값 검증의 안전성 문제로 2.4로 바로 진입.
  - Follower calibration 성공. Leader calibration은 `Magnitude 3188 exceeds 2047` 에러 발생 → 전원 cycling + usbipd 재attach로 해결하고 성공.
  - LeRobot import 경로가 문서와 다름을 발견: 실제 경로는 `lerobot.robots.so_follower`(Follower)와 `lerobot.teleoperators.so_leader`(Leader). Leader는 `SOLeaderConfig` 대신 `SO101LeaderConfig`(=`SOLeaderTeleopConfig`, `TeleoperatorConfig` 상속)를 써야 `id` 파라미터 받음.
  - Follower/Leader 양쪽 degrees 단위 관절 값 읽기 성공 (`get_observation()` / `get_action()`).
  - **Phase 3.1 드라이버 결정**: 옵션 A (LeRobot Python API 래핑) 확정. 옵션 B 후보들 조사 결과 `legalaspro/so101-ros-physical-ai`는 가장 완성도 높지만 **Ubuntu 24.04 + ROS 2 Jazzy 전용**이라 우리 환경(Humble)과 비호환으로 기각. `msf4-0/so101_ros2`는 가볍지만 ros2_control 부재로 MoveIt 통합 어려움. 옵션 A가 Phase 2 성과 재사용 + 환경 호환성 + scope 측면에서 모두 유리.
  - `legalaspro` 레포의 URDF/STL 메시는 ROS 버전 독립적이라 Phase 3.2에서 파일 단위로 복사해 활용 예정.

- **2026-04-17 (Session 4)**: **Phase 3.2 완료 (URDF 확보)**.
  - URDF 소스 두 후보 (`legalaspro/so101-ros-physical-ai` vs `TheRobotStudio/SO-ARM100`) 비교 조사. legalaspro는 공식 URDF를 기반으로 ROS2 xacro로 리팩토링한 파생물임을 확인 (joint 이름/limit 수치 완전 일치). 완전한 ROS2 패키지 구조 + launch 파일을 갖춘 legalaspro 채택.
  - ROS2 workspace `~/UnrealRobotics/` 확정 (`src/` 폴더 이미 존재). `so101_description/` 패키지를 sparse-checkout으로 가져와 배치.
  - **conda/miniforge PATH 충돌 발견 및 해결**: `conda deactivate` 후에도 miniforge의 Python이 PATH에 남아 `colcon build` 시 `catkin_pkg` 에러 발생. PATH에서 miniforge를 수동 제거하는 워크어라운드 적용. 이후 ROS2 작업과 LeRobot 작업은 **반드시 별도 터미널**에서 수행하는 규칙 확립.
  - **launch 파일 버그 수정**: `display.launch.py`가 없는 plain URDF(`so101_new_calib.urdf`)를 참조 → xacro 기반 + `variant` 인자 지원으로 수정.
  - Joint 이름 6개가 LeRobot calibration과 100% 일치 확인. Joint limit은 공식 값 그대로 사용 (Leader 커스텀 기어비는 LeRobot calibration에서 이미 흡수됨).
  - `colcon build --symlink-install` 성공 후 RViz2에서 follower 모델 시각화 + 슬라이더 조작 검증 완료.

- **2026-04-21 (Session 5)**: **Phase 3.3 완료 (LeRobot ↔ ROS2 Bridge)**.
  - **환경 비호환 발견 및 해결**: `rclpy`(CPython 3.10 ABI) ↔ `lerobot 0.5.2`(`requires-python >= 3.12`) 한 프로세스 공존 불가능 확인. 시스템 Python 3.10 venv에 LeRobot 설치 시도 → 실패. 구 버전(0.3.4) 다운그레이드 방안 조사 → SO101 지원 여부 불확실 + calibration 호환성 리스크로 기각.
  - **Two-process + ZeroMQ IPC 아키텍처 확정**: `lerobot_worker.py`(conda, Py3.12) + `ros_bridge_node.py`(system Py3.10 + rclpy). ZMQ PUB/SUB(5555) + REQ/REP(5556). 양쪽 env에 `pyzmq` 설치. Cross-process loopback 검증 성공 (17/20 메시지 수신).
  - **`lerobot_ros2_bridge` ROS2 패키지 생성**: ament_python 구조. `colcon build --symlink-install` 성공.
  - **CycloneDDS 필수 발견**: FastDDS가 WSL2 mirrored mode에서 ROS2 노드 간 DDS discovery 실패. `ros-humble-rmw-cyclonedds-cpp` 설치 + `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` + `ROS_LOCALHOST_ONLY=1` 환경변수로 해결. rosbridge 경로(WebSocket)는 FastDDS에서도 정상 동작하므로, CycloneDDS 필요성은 ROS2 노드 간 통신에 한정.
  - **system Python에 pyzmq/numpy 설치**: `ros2 run`이 venv의 site-packages를 못 찾는 문제 → `sudo pip3 install pyzmq numpy`로 해결.
  - **robot_state_publisher URDF 파라미터 전달 이슈**: URDF 내용이 길어 CLI 인자로 전달 불가 → YAML 파일로 파라미터 전달 방식 적용.
  - **RViz Description Topic 설정**: RobotModel의 Description Topic이 자동 감지 안 됨 → `/robot_description`으로 명시 설정 필요.
  - **End-to-end 검증 성공**: dummy worker(sine wave ±30°, 30Hz) → ZMQ → bridge node → `/joint_states` → `robot_state_publisher` → TF → RViz에서 SO-ARM-101 3D 모델이 실시간으로 움직이는 것 확인.
  - SKILL.md 섹션 7에 신규 quirk 3건 추가 (7.4 CycloneDDS, 7.5 LeRobot Python 제약, 7.6 기존 7.4 renumber). Appendix A에 gotcha 6건 추가 (#14~#19).

- **2026-04-22 (Session 6)**: **Phase 3.4 완료 (실물 로봇 연결 + 텔레옵) + Phase 4.2 완료**.
  - **실물 로봇 연결 성공**: dummy worker 대신 실물 follower + leader에 연결. Worker가 30Hz로 양쪽 arm 관절 데이터 ZMQ 발행. Follower 움직임 → RViz 3D 모델 실시간 반영 확인.
  - **텔레옵 모드 구현**: `lerobot_worker.py`에 `--teleop` 플래그 추가. Leader arm 조작 → follower arm 실시간 추종 + RViz 동시 업데이트 성공. 텔레옵 시작 전 Enter 키 대기로 안전성 확보.
  - **Phase 4.2 동시 달성**: 실물 ↔ RViz 동기화 검증 완료.
  - **`ros2 daemon` quirk 발견**: daemon이 없는 상태에서 `ros2 daemon stop` 실행 시 무한 대기. `ros2 daemon start` → `stop` → `start` 순서로 해결. Appendix A #20에 기록.

- **2026-04-27 (Session 7)**: **Phase 3.5 완료 (Joint Command 구독 검증) + Phase 3 전체 완료**.
  - **Command 경로 검증**: `/follower_joint_commands` → bridge node (ZMQ REQ) → worker (`send_action()`) → follower 실물 동작 확인. 코드는 이미 구현되어 있어 안전 메커니즘 추가에 집중.
  - **Safety Clamp 구현**: baseline ± cmd_limit (session clamp) + 실측 physical limits (absolute clamp) 이중 보호. `--cmd-joints`로 허용 관절 제한, `--cmd-limit`로 범위 제한.
  - **URDF vs LeRobot 좌표계 불일치 발견**: URDF 0° ≠ LeRobot 0°. URDF limit을 LeRobot space에 적용하면 rest position(-103.47°)이 URDF limit(-100°) 밖이 되어 잘못된 클램핑 발생. 원인: 서로 다른 zero point 정의.
  - **해결**: 토크 해제 후 두 포인트 측정 → 선형 보간으로 calibration range를 LeRobot degrees로 변환. `JOINT_LIMITS_DEG` 실측 값 확정.
  - **Calibration JSON 경로 수정**: `so101_follower` → `so_follower` (실제 파일시스템 경로와 일치하도록).
  - **test_joint_cmd.py 작성**: 단발/sweep/sweep-all 모드, baseline-relative sweep, `--sweep-rate` 인자.
  - **전 관절 ±10° 왕복 테스트 성공**: 6개 관절 모두 정상 동작, 이중 클램핑 정상 작동 확인.
  - Appendix A에 gotcha 3건 추가 (#21 URDF/LeRobot 좌표계, #22 calibration 경로, #23 conda/rclpy 혼용).

- **2026-05-07 (Session 8)**: **Phase 5 전체 완료 (Unreal에 3D 로봇 파트 올리기)**.
  - **5.1 Mesh 준비**: legalaspro 레포에서 STL 16개 확인, Follower용 13개 식별. STL 단위가 미터(m)임을 확인. Blender 5.1.1로 STL→FBX 변환. Export 축 설정 주의: `-Y Forward, Z Up` (블렌더 5.x 기본값과 다름). STL 곡면 부위에서 degenerate triangle 다수 발견 → Decimate(0.1~0.2) + Auto Smooth + 수동 수정으로 해결.
  - **5.2 Unreal 임포트**: 13개 FBX를 `Content/Robot/Meshes/`에 Import Scale 1로 임포트 (블렌더 FBX가 cm 단위 포함).
  - **5.3 로봇 액터 구현**: `ARobotVisualizer` C++ 액터 작성. URDF의 7링크 6조인트를 SceneComponent 계층으로 구축. 17개 StaticMeshComponent를 BeginPlay에서 로드 및 부착. `.Build.cs`에 `PublicIncludePaths` 추가로 하위 폴더 간 include 문제 해결.
  - **5.4 좌표 변환 헬퍼**: `RosCoordConv.h` 작성 (namespace, FORCEINLINE). 위치/RPY/쿼터니언/관절 각도 변환.
  - **5.5 실시간 포즈 반영**: `/joint_states` 구독 → JSON 파싱 → JointComponentMap으로 매칭 → 쿼터니언 합성(BaseQuat * JointQuat)으로 관절 회전 적용. 실물 follower 움직임 → Unreal 모델 실시간 반영 확인. 텔레옵(leader → follower → Unreal) 동작도 확인.
  - **비주얼 이슈 발견 (보류)**: gripper 닫힌 정도 차이 + rest 포즈 부품 겹침. URDF/LeRobot zero point 오프셋 차이가 원인. Unreal 측 joint별 오프셋 보정으로 해결 예정 (Calibration JSON은 건드리지 않음).
  - Appendix A에 gotcha 5건 추가 (#24 degenerate triangle, #25 블렌더 5.x 축, #26 STL Import 축, #27 UE include path, #28 zero point 오프셋).

- **2026-05-12 (Session 9)**: **Phase 6 전체 완료 + Phase 7 전체 완료**.
  - **Phase 6 (Unreal → ROS 송신 파이프)**:
    - `FOnRosBridgeConnected`/`FOnRosBridgeDisconnected` 델리게이트 추가. 타이머 폴링 → 델리게이트 기반 전환.
    - `Advertise()`/`Publish()` 구현. Subscribe/Advertise 연결 전 큐잉 → 연결 시 자동 전송.
    - 자동 재연결: 지수 백오프(1s→30s cap). 재연결 시 Subscriptions + Advertisements 자동 복원.
    - 송신 검증: `/unreal_test` Publish → WSL2에서 `ros2 topic echo`로 수신 성공.
    - rosbridge DDS 도메인 이슈 발견: rosbridge 터미널에서도 CycloneDDS 필수.
  - **Phase 7 (MoveIt 통합)**:
    - MoveIt 2 설치 (`ros-humble-moveit`, 25개 패키지).
    - Setup Assistant로 `so101_moveit_config` 패키지 생성 (SRDF, kinematics, joint_limits, collision matrix).
    - `joint_limits.yaml` int→double 수정, SRDF에 shoulder_link↔lower_arm_link collision disable 추가.
    - Controller 방식 결정: ros2_control 대신 FollowJointTrajectory action server(ZMQ 경유) 채택.
    - `joint_trajectory_action_server.py` 작성 — MoveIt trajectory waypoint를 ZMQ로 worker에 전달.
    - `demo.launch.py` 수정 — `moveit_simple_controller_manager` 사용, `robot_state_publisher` 분리.
    - **End-to-end 성공**: RViz MotionPlanning → Plan → Execute → 실물 follower arm 동작.
    - IK(interactive marker) + FK(Joints 슬라이더) 양쪽 방식으로 목표 설정 + 실행 확인.
    - Trajectory 실행 시 서보 떨림 발견 — waypoint 개별 전송 방식의 한계. Velocity Scaling 올리면 완화.
    - Phase 8 준비: Unreal→MoveIt 호출 방법 3가지(A: rosbridge action, B: 서비스 우회, C: 중간 노드) 조사. 방법 C 추천.
    - Appendix A에 gotcha 4건 추가 (#30 joint_limits int, #31 self-collision false positive, #32 robot_state_publisher 충돌, #33 서보 떨림).

- **2026-05-13 (Session 10)**: **Phase 8.1 완료 (Unreal → MoveIt 목표 전달)**.
  - **방법 C 확정**: 중간 ROS2 Python 노드 방식. `moveit_commander`는 Humble에서 미제공 확인. `moveit_msgs/action/MoveGroup` action client를 rclpy로 직접 작성.
  - **5DOF 제약 발견**: Cartesian pose goal (position + orientation)로 전송 시 IK 실패 (`Unable to sample any valid states for goal tree`). SO-ARM-101의 5개 관절로는 6DOF pose를 만족시킬 수 없음.
  - **해결**: orientation constraint 제거 → position-only Cartesian goal + joint space goal 기본 사용.
  - **`moveit_goal_node.py` 작성**: 3가지 토픽 구독 (`/moveit_goal_named`, `/moveit_goal_joints`, `/moveit_goal_pose`) → MoveGroup action 전달. Named target 내장 ("home", "ready").
  - **ROS2 측 검증 성공**: `ros2 topic pub` → named target, joint goal 모두 실물 동작 확인.
  - **Unreal C++ 수정**: `ARobotVisualizer`에 `SendNamedTarget()`/`SendJointGoal()`/`SendPoseGoal()` 추가. `CallInEditor` 버튼으로 Details 패널에서 즉시 테스트 가능. 3개 MoveIt 토픽 자동 advertise.
  - **Unreal End-to-end 성공**: PIE → SendNamedTarget("ready"/"home") → 실물 동작 + 3D 모델 실시간 업데이트 확인. SendJointGoal도 성공.
  - Appendix A에 gotcha 2건 추가 (#34 moveit_commander 미제공, #35 5DOF IK 실패).

- **2026-05-20 (Session 11)**: **Phase 9 전체 완료 (Record/Replay/Sync)**.
  - **방향성 결정**: 디지털 트윈의 본질적 가치는 모니터링 + 상태 파악. 뷰포트 클릭 포즈 지정(8.1b)은 실용적 근거 약해 우선순위 하향. Record/Replay가 실제 공장 반복 작업에 해당하는 핵심 기능.
  - **LeRobot 기능 조사**: record/replay CLI 기본 제공, ACT/Diffusion Policy 학습, VLA 모델, Phone Teleop 등 확인. 현재는 경량 자체 구현(트랙 A), 미래에 LeRobot imitation learning(트랙 B) 확장 가능.
  - **Worker 대규모 확장 (480→978줄)**: 7개 새 ZMQ 명령 (start/stop_teleop, start/stop_record, start/stop_replay, estop). 상태 관리 (`idle`/`syncing`/`recording`/`replaying`). 관절 궤적 JSON 저장/로드. Cosine ease-in-out approach 보간.
  - **Approach 보간 구현**: Sync On, Replay 시작, Loop 전환 3곳에 적용. 기본 45°/s, Unreal에서 조절 가능. 서보 기어 충격 방지 + 시각적 자연스러움.
  - **Bridge node 확장 (200→318줄)**: `/robot_command` 구독 + `/robot_status` 발행. 상태/teleop 변화 전파.
  - **Unreal C++ 확장**: Details 패널에 ROS|Sync, ROS|Record, ROS|Replay, ROS|Safety, ROS|Status 섹션 추가. 뷰포트 컬러 피드백 (초록/시안/노랑/빨강).
  - **End-to-end 전체 워크플로우 검증 성공**: Sync On → Record → Stop → Replay (loop) → EStop → Sync On → 복귀.
  - **발견된 이슈와 해결**: Sync On 시 follower 튀는 현상 → approach 보간으로 해결. Stop Record 후 teleop 해제되는 문제 → teleop 유지하도록 수정. Start Record 시 teleop 미활성 상태에서 호출 가능한 문제 → Sync On 선행 필수로 변경.
  - Appendix A에 gotcha 2건 추가 (#36 Sync On 튀는 현상, #37 Replay loop 순간 이동).

---

**Next session start**: Phase 8.3 나머지 (연결 끊김 경고, 플래닝 실패 피드백) → Phase 10 (UI 기획 및 구현). 비주얼 미세 조정(joint별 zero point 오프셋 보정)은 큰 기능 완료 후 처리.
