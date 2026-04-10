# SO101_Twin — Project Progress & Roadmap

디지털 트윈 프로젝트: **SO-ARM-101 실물 로봇 ↔ Unreal Engine 5.4.4 양방향 연동**

## Project Overview

- **Goal**: SO-ARM-101 로봇의 디지털 트윈 구축. 실물 로봇의 관절 상태를 Unreal에 실시간 반영하고, Unreal에서 계획한 동작을 실물 로봇에 전달.
- **Hardware**: SO-ARM-101 (Leader arm + Follower arm, 각 6x Feetech STS3215 servos, Waveshare bus servo driver board)
  - **Follower**: STS3215 **12V** 모터 x6 + Waveshare 보드 + **12V** 어댑터
  - **Leader**: STS3215 **7.4V** 모터 x6 + Waveshare 보드 + **5V** 어댑터
- **Software stack**:
  - Windows 11 + Unreal Engine 5.4.4 (C++ 프로젝트 `SO101_Twin`)
  - WSL2 (Ubuntu 22.04) + ROS2 Humble
  - WSL2 (Ubuntu 22.04) + conda env `lerobot` (Python 3.12) + LeRobot editable install
  - 통신: rosbridge_suite (WebSocket, port 9090)
- **Project root**: `C:\UnrealProjects\UnrealRobotics-SO101\`
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
- `ROS_LOCALHOST_ONLY=1`나 CycloneDDS 전환은 불필요함이 확인됨. 기본 FastDDS로 rosbridge 경로는 정상 동작.

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

# Phase 2 — Hardware Verification (현재 단계) 🔄

## ✅ 2.1 STS3215 모터 개별 검증 및 세팅

### Hardware 구성 확정
- **Follower**: Waveshare bus servo driver + 12V 어댑터 + STS3215 12V 모터 x6
- **Leader**: Waveshare bus servo driver + 5V 어댑터 + STS3215 7.4V 모터 x6
- 양쪽 Waveshare 보드 모두 점퍼 2개를 **B 채널(USB)**에 설정
- ⚠️ **전원/모터 교차 연결 절대 금지** — 7.4V 모터에 12V 어댑터를 잘못 연결하면 모터 손상 위험

### WSL2 USB 포워딩 셋업
- `usbipd-win`으로 Waveshare 보드를 WSL2에 포워딩
- PowerShell 프로필에 헬퍼 함수 등록: `Attach-Follower`, `Attach-Leader`, `Attach-Both`
  - BUSID 기반 단순 버전 (`$PROFILE`에 저장됨)
  - 현재 USB 포트 기준: Follower=`1-7`, Leader=`3-2`
- ⚠️ WSL 재시작/셧다운 후 usbipd attach 풀림 → 헬퍼 함수로 재연결

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
- `sudo usermod -aG dialout $USER` → `/dev/ttyACM0` 접근 권한 부여
- 그룹 변경 반영을 위해 `wsl --shutdown` 후 재시작 필요

### 모터 ID 할당 (양쪽 arm 완료)
- `lerobot-find-port`로 각 보드의 포트 확정 (`/dev/ttyACM0`)
- Follower 6개 모터 세팅:
  ```bash
  lerobot-setup-motors --robot.type=so101_follower --robot.port=/dev/ttyACM0
  ```
- Leader 6개 모터 세팅:
  ```bash
  lerobot-setup-motors --teleop.type=so101_leader --teleop.port=/dev/ttyACM0
  ```
- 순서: gripper(6) → wrist_roll(5) → wrist_flex(4) → elbow_flex(3) → shoulder_lift(2) → shoulder_pan(1)
- 총 12개 모터 전부 ID + baudrate (1,000,000) 할당 완료

### Sanity check (데이지 체인 연결 상태)
- `robot.bus.sync_read("Present_Position", normalize=False)`로 raw encoder 값 읽기
- Follower 6개, Leader 6개 모두 정상 범위(0~4095, 12-bit 인코더)에서 응답 확인
- `normalize=True` 기본값은 calibration 파일이 없어서 실패함 → sanity check에서는 `normalize=False`로 우회
- 정식 calibration은 Phase 5(Unreal 통합 직전)에서 `lerobot-calibrate`로 진행 예정

### 해결된 이슈
- **`/dev/ttyACM0` PermissionError**: `dialout` 그룹 미가입 → `usermod -aG dialout` + `wsl --shutdown`으로 해결
- **usbipd attach 상태 불일치**: `usbipd list`는 `Attached`라고 하는데 WSL2에 장치가 없는 경우 → `usbipd detach --busid X-Y` 후 재attach로 해결 (WSL 재시작 시 종종 발생)
- **`get_observation()` calibration 에러**: sanity check에는 calibration이 불필요하므로 `sync_read(..., normalize=False)`로 bus 레벨에서 raw 접근

### ❓ Open questions (해결됨)
- ~~드라이버 보드 종류~~: **Waveshare** ✅
- ~~전원 어댑터 사양~~: Follower **12V**, Leader **5V** (모터 전압에 맞춤) ✅

## ⏳ 2.2 SO-ARM-101 조립 (NEXT)
- [ ] Leader arm 조립 (공급자 매뉴얼 참조)
- [ ] Follower arm 조립
- [ ] 각 관절에 라벨링된 모터 장착 (ID + L/F 구분)
- [ ] 케이블링 (bus servo chain)
- [ ] 전원 배선 검증 후 첫 통전

### 조립 중 참고사항
- 공식 가이드: https://huggingface.co/docs/lerobot/so101
- STS3215 중앙 위치는 raw 값 약 2048 (0~4095 중간)
- 조립 전 각 모터의 라벨링 권장: `F1-SP`, `F2-SL`, ..., `F6-GR`, `L1-SP`, ..., `L6-GR`

## 📋 2.3 조립 후 전체 스윕 테스트
- [ ] 12개 모터 모두 데이지 체인으로 응답 확인 (양쪽 arm)
- [ ] 각 관절 손으로 돌렸을 때 해당 ID의 raw_pos 값 변화 확인
- [ ] 각 관절 ±10도 정도 안전 범위 테스트 동작
- [ ] 기계적 간섭 부위 확인

## 📋 2.4 LeRobot Calibration
- [ ] `lerobot-calibrate --robot.type=so101_follower ...`
- [ ] `lerobot-calibrate --teleop.type=so101_leader ...`
- [ ] Calibration 파일 위치: `~/.cache/huggingface/lerobot/calibration/`
- [ ] Calibration 후 `get_observation()`으로 degrees 단위 관절 값 확인

---

# Phase 3 — SO-ARM-101 → ROS2 Humble 통합

## 📋 3.1 ROS2 드라이버 선택
- [ ] 옵션 A: LeRobot의 기존 feetech 드라이버 활용 (Python)
- [ ] 옵션 B: 커뮤니티 ros2_feetech 드라이버 찾기
- [ ] 옵션 C: 직접 작성 (scservo_sdk + rclpy/rclcpp)
- [ ] ❓ 어느 옵션으로 갈지 결정

## 📋 3.2 URDF 확보
- [ ] LeRobot 또는 SO-ARM-101 커뮤니티에서 URDF 획득
- [ ] 메시 파일(STL) 포함 여부 확인
- [ ] 좌표계와 관절 축 방향 검증
- [ ] WSL2에 `so101_description` 패키지로 배치

## 📋 3.3 로봇 상태 퍼블리셔 셋업
- [ ] `robot_state_publisher` 실행 (URDF 로드)
- [ ] 모터 드라이버 노드가 실제 서보에서 각도 읽어 `/joint_states` 퍼블리시
- [ ] `ros2 topic echo /joint_states`로 값 확인
- [ ] TF 트리 확인 (`ros2 run tf2_tools view_frames`)

## 📋 3.4 Joint Command 구독
- [ ] `/joint_commands` (또는 합의된 토픽명) 구독 노드 작성
- [ ] 수신된 각도를 서보로 전달
- [ ] 단일 관절 명령 테스트 (CLI에서 `ros2 topic pub`로 한 관절만)

---

# Phase 4 — RViz 시각화

## 📋 4.1 RViz2 기본 셋업
- [ ] RViz2 실행, URDF 로드
- [ ] `/joint_states` 구독 설정
- [ ] TF 트리 시각화 활성화

## 📋 4.2 실물 ↔ RViz 동기화 검증
- [ ] 손으로 로봇 관절을 움직이면 RViz의 모델도 따라 움직이는지 확인
- [ ] (가능하다면) 반대로 RViz GUI에서 관절 슬라이더를 움직이면 실물이 반응하는지

---

# Phase 5 — Unreal에 3D 로봇 파트 올리기

## 📋 5.1 Mesh 준비
- [ ] URDF의 STL 메시를 FBX 또는 glTF로 변환
- [ ] 또는 공급자가 제공한 CAD/FBX 파일 사용
- [ ] 관절 기준 원점(origin) 정렬 확인

## 📋 5.2 Unreal에 임포트
- [ ] 각 링크를 StaticMesh로 임포트
- [ ] 또는 Skeletal Mesh로 전체 로봇을 하나의 스켈레톤으로 임포트 (권장)
- [ ] Content Browser에 `Robot/` 폴더 구성

## 📋 5.3 로봇 액터 구현
- [ ] `ARobotVisualizer` (또는 비슷한 이름) C++ 액터 작성
- [ ] 관절 구조에 맞게 컴포넌트 계층 구성
- [ ] 각 관절에 대한 회전 상태 변수

## 📋 5.4 좌표 변환 헬퍼 구현
- [ ] `URosCoordConv` 유틸리티 클래스
- [ ] SKILL.md 섹션 2의 변환 함수 구현 (position, quaternion)
- [ ] 단위 테스트 (알려진 값으로 round-trip 검증)

## 📋 5.5 /joint_states 구독 → 로봇 포즈 반영
- [ ] `URosBridgeSubsystem`로 `/joint_states` 구독
- [ ] `sensor_msgs/JoinState` JSON 파싱 → 각 관절 각도 추출
- [ ] 라디안 → 도 변환 (Unreal은 도 단위)
- [ ] 관절에 회전 적용
- [ ] 실물 로봇을 손으로 움직였을 때 Unreal 안의 모델도 따라 움직이는지 검증

---

# Phase 6 — Unreal → ROS 송신 파이프 확장

현재 `URosBridgeSubsystem`은 수신만 구현됨. 송신 기능 추가.

## 📋 6.1 Publish API 추가
- [ ] `URosBridgeSubsystem::Advertise(Topic, Type)` 구현
- [ ] `URosBridgeSubsystem::Publish(Topic, JsonMessage)` 구현
- [ ] `AdvertisedTopics` 트래킹 (재연결 시 복원)
- [ ] SKILL.md 섹션 3의 "advertise 먼저, 그 다음 publish" 원칙 준수

## 📋 6.2 Connected 델리게이트 추가
- [ ] 지금은 `ARosTestActor`가 타이머로 1초 대기 후 Subscribe하는 임시 방식
- [ ] Subsystem에 `FOnConnected` 델리게이트 추가
- [ ] 액터는 이 이벤트를 기다렸다가 Subscribe/Advertise 호출
- [ ] 타이머 폴링 방식 제거

## 📋 6.3 자동 재연결 로직
- [ ] `OnConnectionError`/`OnClosed`에서 재연결 타이머 시작
- [ ] 지수 백오프 (1s, 2s, 4s, 8s, cap 30s)
- [ ] 재연결 성공 시 `SubscribedTopics` + `AdvertisedTopics` 자동 복원

## 📋 6.4 송신 검증
- [ ] Unreal에서 더미 토픽(`/unreal_test`) 퍼블리시
- [ ] WSL2에서 `ros2 topic echo /unreal_test`로 확인 (단, CLI quirk 주의 — `demo_nodes_cpp` listener 패턴 권장)

---

# Phase 7 — MoveIt 통합

## 📋 7.1 MoveIt 2 설치
- [ ] `sudo apt install ros-humble-moveit`

## 📋 7.2 MoveIt Setup Assistant로 설정
- [ ] SO-ARM-101 URDF 로드
- [ ] Self-collision 매트릭스 생성
- [ ] 플래닝 그룹 정의 (arm, gripper)
- [ ] 엔드 이펙터 설정
- [ ] 사전 정의 포즈 (home, ready)
- [ ] ros2_control 인터페이스 설정
- [ ] `so101_moveit_config` 패키지 생성

## 📋 7.3 MoveIt 데모 동작
- [ ] `ros2 launch so101_moveit_config demo.launch.py`
- [ ] RViz에서 MotionPlanning 패널로 목표 포즈 지정 → Plan → Execute
- [ ] 실물 로봇으로 전달

## 📋 7.4 프로그래밍 인터페이스
- [ ] `move_group` 액션 서버 직접 호출 가능한지 확인
- [ ] rosbridge로 액션 호출 가능 여부 조사
  - ❓ rosbridge v2는 액션 지원이 제한적일 수 있음. 서비스 우회 방법 검토 필요.

---

# Phase 8 — 최종 통합: 양방향 디지털 트윈

## 📋 8.1 Unreal에서 타겟 지점 지정
- [ ] 뷰포트 클릭 또는 드래그로 3D 공간에 타겟 포즈 생성
- [ ] 좌표 변환 (UE → ROS)
- [ ] `geometry_msgs/PoseStamped` JSON으로 직렬화

## 📋 8.2 MoveIt에 목표 전달
- [ ] rosbridge를 통해 MoveIt 서비스/액션 호출
- [ ] 계획된 경로를 실물 로봇이 실행

## 📋 8.3 실시간 상태 피드백 루프
- [ ] 실행 중 `/joint_states`를 Unreal이 구독 → 3D 모델 실시간 업데이트
- [ ] 계획된 경로(플래닝 결과)를 Unreal에 시각화 (선/웨이포인트)

## 📋 8.4 오류 및 안전 처리
- [ ] 연결 끊김 시 Unreal UI 경고
- [ ] 플래닝 실패 시 사용자 피드백
- [ ] E-stop 기능 (Unreal의 버튼 → rosbridge → ros2 topic → 서보 정지)

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

---

# Appendix B — Recurring Commands

## WSL2 측 — ROS2
```bash
# rosbridge 기동
source /opt/ros/humble/setup.bash
ros2 launch rosbridge_server rosbridge_websocket_launch.xml

# 테스트 퍼블리셔
ros2 run demo_nodes_cpp talker

# 테스트 서브스크라이버 (topic echo 대신)
ros2 run demo_nodes_cpp listener
```

## WSL2 측 — LeRobot
```bash
# 환경 활성화 (매 세션)
conda activate lerobot

# 포트 찾기
lerobot-find-port

# 모터 ID 세팅 (출고 상태 모터 → ID 할당)
lerobot-setup-motors --robot.type=so101_follower --robot.port=/dev/ttyACM0
lerobot-setup-motors --teleop.type=so101_leader --teleop.port=/dev/ttyACM0

# Sanity check (raw encoder 값 읽기)
python - <<'EOF'
from lerobot.robots.so_follower import SO101Follower, SO101FollowerConfig
cfg = SO101FollowerConfig(port="/dev/ttyACM0", id="test")
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
Attach-Follower   # busid 1-7
Attach-Leader     # busid 3-2
Attach-Both       # 둘 다

# 상태 불일치 시
usbipd detach --busid 1-7
Attach-Follower

# 현재 상태 확인
usbipd list
```

---

# Session Log (Rough)

주요 마일스톤 달성 시점을 기록. 새 세션 시작 시 맥락 파악용.

- **2026-04-09 (Session 1)**: Phase 0 전체 완료, Phase 1 전체 완료. `?x=1` 워크어라운드와 `127.0.0.1` 이슈 발견 및 해결. 첫 end-to-end 메시지 수신 성공. PROGRESS.md, CLAUDE.md, SKILL.md 초안 작성.

- **2026-04-10 (Session 2)**: Phase 2.1 완료. Waveshare 드라이버 보드 2개(Follower 12V, Leader 5V) 확정. usbipd-win으로 WSL2 포워딩 셋업 (PowerShell 프로필 헬퍼 함수 `Attach-Follower`/`Attach-Leader`/`Attach-Both`). WSL2에 Miniforge + conda env `lerobot` (Python 3.12) + LeRobot editable install + feetech extra. `dialout` 그룹 이슈 해결 후 `lerobot-setup-motors`로 Leader + Follower 총 12개 STS3215 모터에 ID 1~6 할당 완료. 데이지 체인 상태에서 `sync_read(..., normalize=False)` raw 읽기로 12개 모터 전부 응답 확인. Open question 2개(보드 종류, 전원 사양) 모두 closed.

---

**Next session start**: Phase 2.2 (SO-ARM-101 기구부 조립) 부터 시작. LeRobot 공식 `assemble_so101` 가이드 따라 Leader + Follower 두 암을 조립. 조립 후 2.3(전체 스윕 테스트) → 2.4(calibration)로 이어짐.
