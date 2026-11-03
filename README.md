# PikachuVolleyBall

네트워크 게임 프로그래밍 3팀 텀 프로젝트. 기존 로컬 게임을 클라이언트 프로젝트로 옮기고 공통 프로토콜, TCP 접속·수락, 키 입력 처리와 서버의 좌우 이동을 구현했다.

## 열기와 실행

- Visual Studio에서 `PikachuVolleyBall/PikachuVolleyBall.slnx`를 연다. `Client`를 시작 프로젝트로 설정한다.
- C++20, Windows SDK, Visual Studio C++ 도구가 필요하다. 팀 기준 v145를 우선 사용하며 설치되어 있지 않으면 기존 설정대로 v143을 사용한다.
- 저장소 루트에서 PowerShell로 `./Tools/Build.ps1 -Verify`를 실행하면 세 프로젝트를 빌드하고 검증한다. `-Configuration Release`, `-Platform Win32`도 지원한다.
- 실행 파일은 `PikachuVolleyBall/bin/x64/Debug/Client/Client.exe`에 생성된다. `Resources`도 자동 복사하고 실행 시 실행 파일 위치에서 리소스를 찾으므로 다른 작업 디렉터리에서 시작해도 된다.
- 현재 Client는 기존 **로컬 2인 게임**으로 실행된다. 1P는 R(점프), D/G(좌우), Z(스파이크/슬라이딩), 2P는 방향키와 Enter를 사용한다.
- `Server.exe`는 TCP 포트 9000에서 접속을 수락하며 Enter 입력까지 대기한다. `Server.exe --check`는 임시 포트로 접속·세션 스레드 생성·종료까지 확인한다. 패킷 수신과 방 처리는 아직 구현하지 않아 온라인 경기는 시작되지 않는다.
- `NetworkClient::Connect`는 IPv4 주소로 TCP 연결을 맺는다. 현재 접속 화면에는 연결하지 않았으며 수신 스레드 시작은 수신 루프를 구현하는 일정에 맞춰 추가한다.

## 구조

```text
PikachuVolleyBall/
  PikachuVolleyBall.slnx
  Directory.Build.props
  Client/        기존 Source·Resources, 새 클라이언트 클래스 틀
  Server/        서버·세션·방·게임 로직 클래스 틀
  Common/        GameConst.h, Protocol.h, Common.h/.cpp
  Tests/         패킷 규약·TCP 수신·클래스 연결 검증
Tools/Build.ps1
```

`original` 태그는 작업 전 원본 커밋 `cde49abe32c4f9255d928c89ad6386929ebaedf8`를 이미 가리키고 있어 변경하지 않았다. 로컬 게임의 규칙·스프라이트 좌표는 유지하고 창 크기, 틱, 이동/점프, 공 물리, 서브, 득점 상수는 `Common/GameConst.h`로 추출하여 기존 코드에서 참조한다.

## 10/31~11/1 작업 범위

| 담당 | 날짜 | 완료 내용 |
|---|---|---|
| 강석주 | 10/31 | 기존 프로젝트 Client 전환, Server·Common 추가, original 태그 확인 |
| 정진영 | 10/31 | GameConst.h 상수 추출과 기존 게임 연결 |
| 김경준 | 10/31 | Common.h와 recvn 정리 |
| 강석주 | 11/1 | Protocol.h, Server·ClientSession·RoomManager·Room 헤더 및 빈 함수 |
| 정진영 | 11/1 | GameWorld·GameEvent 헤더 및 빈 함수 |
| 김경준 | 11/1 | NetworkClient·InputManager·GameClient·Renderer 헤더 및 빈 함수 |

나머지 빈 함수에는 이후 구현 일정의 TODO를 표시했다. `SendPacket` 등은 구현 전까지 `false`, 스레드 진입점은 `ERROR_CALL_NOT_IMPLEMENTED`를 반환한다. 온라인 UI 전환과 물리 시뮬레이션은 아직 연결하지 않았다. 기본 상태 복사는 네트워크 수신이나 시뮬레이션 구현을 의미하지 않는다.

## 11/2 작업 범위

| 담당 | 완료 내용 |
|---|---|
| 강석주 | Server::InitServer의 Winsock 초기화, 소켓 생성, bind·listen 및 오류 처리 |
| 정진영 | 키 입력을 이동·점프·슬라이딩·스파이크 요청으로 변환, 눌림·유지·뗌 구분 |
| 김경준 | NetworkClient::Connect의 IPv4 TCP 연결과 실패 후 재시도 |

초기화와 접속을 테스트할 수 있도록 소멸자와 소켓 정리도 함께 구현했다. 중복 초기화·중복 접속은 기존 연결을 보존하고 실패하며 `GetLastError()`로 Winsock 오류를 확인한다. 서버의 `GetPort()`는 실제 바인딩한 포트이며 0으로 초기화하면 빈 포트를 자동 배정받는다. `IsConnected()`는 로컬 소켓 연결 여부로, 상대의 종료 감지는 수신 루프 구현 시 추가한다. 연결·종료 함수는 현재 동일한 제어 스레드에서 호출한다.

`GameWorld::Update`는 `ApplyInputs`로 두 플레이어의 입력을 해석하고 좌우 좌표를 갱신한다. `GetPlayerInput(index)`에서 이동 방향과 동작 요청을 얻으며 점프·공 물리·애니메이션은 후속 일정에서 갱신한다. 좌우 동시 입력은 상쇄되고, 점프·액션은 새로 누른 순간만 요청한다. 슬라이딩·승패 동작 중에는 다른 동작 요청을 막는다. 공중 액션은 스파이크, 지상에서 방향+액션은 슬라이딩으로 변환한다.

## 11/3 작업 범위

| 담당 | 완료 내용 |
|---|---|
| 강석주 | Server::AcceptLoop에서 접속 수락, ClientSession 생성과 세션 스레드 시작 |
| 정진영 | 두 피카츄의 좌우 이동, 코트 경계 제한과 이동·정지 상태 반영 |
| 김경준 | InputManager::OnKeyDown·OnKeyUp의 방향키·Z 처리와 키 상태 변경 판정 |

AcceptLoop는 서버 프로그램의 별도 스레드에서 실행한다. 서버가 각 세션과 Win32 스레드 핸들을 소유하며 종료할 때 대기 루프를 멈추고 소켓·스레드를 정리한다. 접속이 없는 상태에서도 종료할 수 있도록 리슨 소켓에는 select와 논블로킹 accept를 사용하며, 수락한 클라이언트 소켓은 다시 블로킹 모드로 바꾼다. ClientThread 본체는 11/4 일정이므로 현재 스레드는 임시 함수를 실행하고 끝나며 세션은 서버 종료까지 보관한다. `GetAcceptedCount()`는 누적 수락 수, `GetClientCount()`는 서버가 소유한 세션 수다. AcceptLoop를 직접 실행하는 호출자는 서버 종료 후 해당 스레드를 join해야 한다.

GameWorld 생성 시 피카츄를 기존 시작 위치에 놓는다. 좌우 이동은 틱마다 일반 속도 10, 각성 상태는 15를 사용하고 마지막 이동에서 코트 경계를 넘지 않게 제한한다. 공중에서는 높이와 동작을 보존하고, 슬라이딩·승패 동작 중에는 일반 걷기를 막는다. 점프와 슬라이딩 자체의 이동은 후속 일정이다.

InputManager는 왼쪽·오른쪽·위쪽 방향키와 Z를 각각 LEFT·RIGHT·UP·ACTION으로 변환한다. 최초 키 입력과 실제 키 해제에만 true를 반환하며 키 반복, 이미 해제된 키, Enter 등 무관한 키에는 false를 반환한다. `Reset()`은 키 상태를 모두 비운다. 실제 화면 메시지 및 SendInput 연결은 후속 일정이고 현재 로컬 게임의 조작은 유지한다.

## 공통 프로토콜

PDF 15~18쪽의 패킷 번호 1~12와 필드를 따른다. 문서에서 정하지 않은 세부 표현은 이번 코드에서 다음과 같이 명시했다. 서버와 클라이언트가 동일한 헤더를 공유해야 한다.

- TCP, 포트 9000, 3바이트 헤더(`uint16_t size`, `uint8_t type`), 전체 구조체 크기를 size에 기록.
- Windows x86/x64 little-endian, 1바이트 패킹, 32비트 좌표/float. 패킷 크기를 `static_assert`로 고정한다. 다른 플랫폼용 직렬화는 현재 범위에 포함하지 않는다.
- 플레이어 인덱스 0/1, 유효하지 않은 인덱스 255. 스테이지 번호는 1/2.
- 키 비트 LEFT=1, RIGHT=2, UP=4, ACTION=8. 온라인 조작은 추후 방향키+Z, Enter 채팅으로 연결한다.
- `Action`: Idle=0, Move=1, Jump=2, Slide=3, Spike=4, Win=5, Lose=6.
- `Phase`: Ready=0, Playing=1, ServeWait=2, StageOver=3, GameOver=4.
- `EventKind`: Jump=0, Slide=1, Spike=2, Hit=3, Score=4.
- `JoinResult`: Waiting=0, Matched=1, Full=2, Rejected=3. 기권 사유 Normal=0, Quit=1.
- `PlayerState`: int32 x/y, uint8 action, uint16 frame, uint8 evil. `BallState`: float x/y/vx/vy, uint8 spike.
- 닉네임 16바이트, 채팅 64바이트 버퍼는 NUL 포함 UTF-8 용량이다. 향후 입력 검증은 바이트 수와 UTF-8 문자 경계를 함께 확인해야 한다.

| 패킷 | 번호 | 바이트 |
|---|---:|---:|
| JOIN_ROOM | 1 | 23 |
| INPUT | 2 | 4 |
| NEXT_STAGE | 3 | 3 |
| LEAVE | 4 | 3 |
| CHAT | 5 | 68 |
| JOIN_RESULT | 6 | 5 |
| GAME_START | 7 | 37 |
| GAME_STATE | 8 | 49 |
| EVENT | 9 | 5 |
| SCORE | 10 | 6 |
| STAGE_OVER | 11 | 5 |
| GAME_OVER | 12 | 5 |

`recvn`은 블로킹 소켓에서 지정 길이만큼 읽는다. 정상 완료는 요청 길이, 정상 연결 종료는 실제 받은 짧은 길이(0 포함), 오류는 SOCKET_ERROR를 반환한다. 호출자는 반환 길이가 요청과 같은지 반드시 확인해야 한다. WSAStartup/WSACleanup, 소켓 수명, 타임아웃은 호출자 책임이며 MSG_PEEK는 지원하지 않는다.

## 검증 범위

`Tests.exe`는 실제 loopback TCP로 1바이트씩 나뉜 패킷, 한 번에 붙어 온 두 패킷, 중간 연결 종료, 소켓 오류와 잘못된 인자를 확인한다. 12개 패킷의 헤더·크기와 알려진 INPUT 바이트열을 검사하고 모든 클래스의 함수 정의가 연결되는지도 확인한다. 서버·클라이언트의 실제 TCP 연결, 사용 중인 포트, 재접속, 접속 거절, 잘못된 주소, Winsock 참조 수 정리도 확인한다. 입력 테스트는 최초 눌림, 유지, 해제, 동작 제한과 두 플레이어의 독립 처리를 검사한다. 전체 온라인 대전 테스트는 후속 일정에서 수신·방·게임 로직을 구현한 뒤 수행한다.

2026-11-01 로컬 검증 결과: v143 도구로 x64 Debug, x64 Release, Win32 Debug의 Client·Server·Tests 빌드와 기반 검증을 모두 통과했다. 이미지·사운드 11개는 원본과 바이트가 같으며 스프라이트 JSON도 내용이 같다. 기존 로컬 게임의 수치 변환 경고는 남아 있다.

Client.exe 실행 후 `Pikachu_VolleyBall` 창 생성과 프로세스 응답을 확인했다. UI 캡처 도구의 시간 초과 및 최소화된 창 상태로 경기 화면·키 조작은 자동 확인하지 못했다. 온라인 대전이 동작한다는 의미는 아니다.

2026-11-02 검증: x64 Debug·Release와 Win32 Debug의 빌드 및 확장 테스트를 통과했다. 기본 Server.exe를 별도로 실행해 포트 9000의 TCP 연결과 Enter 입력 후 정상 종료를 확인했다. Client.exe는 창 생성 및 시작 키 입력 후 응답을 확인했으나 화면 캡처 도구의 시간 초과로 화면 변화는 확인하지 못했다.

2026-11-03 검증: x64 Debug·Release와 Win32 Debug의 빌드·테스트를 통과했다. 구성별로 36개 세션의 수락·정리, 반복 서버 시작·종료, 대기 중 종료, 동시 종료 호출과 핸들 누수를 검사했다. 좌우 이동·경계·각성 속도·동작 제한, 키 반복·해제·조합·Reset 및 InputManager에서 GameWorld로 이어지는 이동도 검증했다. 기본 Server.exe를 따로 실행해 포트 9000에서 클라이언트 3개가 수락되고 Enter 입력 후 정상 종료되는 것을 확인했다.
