# SO101_Twin — Project Progress & Roadmap

디지털 트윈 프로젝트: **SO-ARM-101 실물 로봇 ↔ Unreal Engine 5.4.4 양방향 연동**

## Project Overview

- **Goal**: SO-ARM-101 로봇의 디지털 트윈 구축. 실물 로봇의 관절 상태를 Unreal에 실시간 반영하고, Unreal에서 계획한 동작을 실물 로봇에 전달.
- **Hardware**: SO-ARM-101 (6x Feetech STS3215 servos, Waveshare/Feetech 계열 bus servo driver)
- **Software stack**:
  - Windows 11 + Unreal Engine 5.4.4 (C++ 프로젝트 `SO101_Twin`)
  - WSL2 (Ubuntu 22.04) + ROS2 Humble
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

## ⏳ 2.1 STS3215 모터 개별 검증 (🔄 NEXT)
로봇 조립 전에 모터가 정상인지 확인. 불량 모터를 조립 후 발견하면 분해가 필요하므로 먼저 해야 함.

### 정성적 체크
- [ ] 드라이버 보드에 12V 공급 시 전원 LED 점등
- [ ] 서보에 전원 인가 시 홀딩 사운드 ("틱" 소리와 축 경직)
- [ ] 전원 OFF 상태에서 축을 수동 회전 — 부드럽고 일정한 저항 (걸림/모래감 없어야 함)
- [ ] 전원 ON 상태에서 축이 손힘에 저항 — 정상 홀딩 토크 확인

### 소프트웨어 통신 검증
- [ ] `usbipd-win` 설치 및 WSL2 USB 포워딩 설정
- [ ] WSL2에서 `/dev/ttyUSB0` 또는 `/dev/ttyACM0`으로 드라이버 인식
- [ ] Python `feetech-servo-sdk`로 각 모터 ID 1개씩 응답 확인 (Present Position 읽기)
- [ ] 통신 baudrate 확인 (기본 1,000,000)

### 모터 ID 설정
- [ ] 6개 모터에 ID 1~6 순차 할당
- [ ] LeRobot `setup_motors` 스크립트로 SO-ARM-101 문맥에서 자동 설정 (권장)

### ❓ Open questions
- 받은 드라이버 보드가 Waveshare 버전인가 Feetech FE-URT-1인가?
- 전원 어댑터 12V 사양이 맞는가? (일부 키트는 7.4V 옵션도 있음)

## 📋 2.2 SO-ARM-101 조립
- [ ] 기구부 조립 (공급자 매뉴얼 참조)
- [ ] 각 관절에 검증된 모터 장착
- [ ] 케이블링 (bus servo chain)
- [ ] 전원 배선 검증 후 첫 통전

## 📋 2.3 조립 후 전체 스윕 테스트
- [ ] 6개 모터 모두 응답 확인
- [ ] 각 관절 ±10도 정도 안전 범위 테스트 동작
- [ ] 기계적 간섭 부위 확인

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

---

# Appendix B — Recurring Commands

## WSL2 측
```bash
# rosbridge 기동
source /opt/ros/humble/setup.bash
ros2 launch rosbridge_server rosbridge_websocket_launch.xml

# 테스트 퍼블리셔
ros2 run demo_nodes_cpp talker

# 테스트 서브스크라이버 (topic echo 대신)
ros2 run demo_nodes_cpp listener
```

## Windows 측
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

## USB 포워딩 (Phase 2에서 사용 예정)
```powershell
usbipd list
usbipd bind --busid <X-Y>
usbipd attach --wsl --busid <X-Y>
```

---

# Session Log (Rough)

주요 마일스톤 달성 시점을 기록. 새 세션 시작 시 맥락 파악용.

- **2026-04-09 (Session 1)**: Phase 0 전체 완료, Phase 1 전체 완료. `?x=1` 워크어라운드와 `127.0.0.1` 이슈 발견 및 해결. 첫 end-to-end 메시지 수신 성공. PROGRESS.md, CLAUDE.md, SKILL.md 초안 작성.

---

**Next session start**: Phase 2.1 (STS3215 모터 개별 검증)부터 시작. 드라이버 보드 종류와 전원 사양 확인이 첫 단계.
