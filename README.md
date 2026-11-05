# PikachuVolleyBall

네트워크 게임 프로그래밍 3팀 텀 프로젝트. 공통 프로토콜, TCP 접속·수락·수신, 방 입장 요청 전송, 키 입력 처리와 서버의 이동·점프를 구현했다.

## 열기와 실행

- Visual Studio에서 `PikachuVolleyBall/PikachuVolleyBall.slnx`를 연다. `Client`를 시작 프로젝트로 설정한다.
- C++20, Windows SDK, Visual Studio C++ 도구가 필요하다. 팀 기준 v145를 우선 사용하며 설치되어 있지 않으면 기존 설정대로 v143을 사용한다.
- 저장소 루트에서 PowerShell로 `./Tools/Build.ps1 -Verify`를 실행하면 세 프로젝트를 빌드하고 검증한다. `-Configuration Release`, `-Platform Win32`도 지원한다.
- 실행 파일은 `PikachuVolleyBall/bin/x64/Debug/Client/Client.exe`에 생성된다. `Resources`도 자동 복사하고 실행 시 실행 파일 위치에서 리소스를 찾으므로 다른 작업 디렉터리에서 시작해도 된다.
- 현재 Client는 기존 **로컬 2인 게임**으로 실행된다. 1P는 R(점프), D/G(좌우), Z(스파이크/슬라이딩), 2P는 방향키와 Enter를 사용한다.
- `Server.exe`는 TCP 포트 9000에서 메인 스레드가 접속을 수락하며, 콘솔에서 Enter를 누르면 종료한다. `Server.exe --check`는 임시 포트로 접속·세션 스레드 생성·종료까지 확인한다. 점검 모드에서만 테스트 클라이언트를 보조 스레드로 실행하고, 접속 수락은 동일하게 메인 스레드에서 수행한다. 세션별 패킷 수신은 연결했으며 방 배정과 패킷별 게임 처리는 후속 일정이다.
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

나머지 빈 함수에는 이후 구현 일정의 TODO를 표시했다. `SendPacket` 등은 구현 전까지 `false`, 클라이언트 RecvThread·RoomThread는 `ERROR_CALL_NOT_IMPLEMENTED`를 반환한다. 온라인 UI 전환과 공 물리·득점은 아직 연결하지 않았다.

## 11/2 작업 범위

| 담당 | 완료 내용 |
|---|---|
| 강석주 | Server::InitServer의 Winsock 초기화, 소켓 생성, bind·listen 및 오류 처리 |
| 정진영 | 키 입력을 이동·점프·슬라이딩·스파이크 요청으로 변환, 눌림·유지·뗌 구분 |
| 김경준 | NetworkClient::Connect의 IPv4 TCP 연결과 실패 후 재시도 |

초기화와 접속을 테스트할 수 있도록 소멸자와 소켓 정리도 함께 구현했다. 중복 초기화·중복 접속은 기존 연결을 보존하고 실패하며 `GetLastError()`로 Winsock 오류를 확인한다. 서버의 `GetPort()`는 실제 바인딩한 포트이며 0으로 초기화하면 빈 포트를 자동 배정받는다. `IsConnected()`는 로컬 소켓 연결 여부로, 상대의 종료 감지는 수신 루프 구현 시 추가한다. 연결·종료 함수는 현재 동일한 제어 스레드에서 호출한다.

`GameWorld::Update`는 `ApplyInputs`로 두 플레이어의 입력을 해석하고 좌우 이동과 점프를 갱신한다. `GetPlayerInput(index)`에서 이동 방향과 동작 요청을 얻으며 공 물리·애니메이션은 후속 일정에서 갱신한다. 좌우 동시 입력은 상쇄되고, 점프·액션은 새로 누른 순간만 요청한다. 슬라이딩·승패 동작 중에는 다른 동작 요청을 막는다. 공중 액션은 스파이크, 지상에서 방향+액션은 슬라이딩으로 변환한다.

## 11/3 작업 범위

| 담당 | 완료 내용 |
|---|---|
| 강석주 | Server::AcceptLoop에서 접속 수락, ClientSession 생성과 세션 스레드 시작 |
| 정진영 | 두 피카츄의 좌우 이동, 코트 경계 제한과 이동·정지 상태 반영 |
| 김경준 | InputManager::OnKeyDown·OnKeyUp의 방향키·Z 처리와 키 상태 변경 판정 |

계획서대로 `main`에서 `Server::AcceptLoop()`를 직접 호출한다. 접속마다 `ClientSession::Start()`가 `CreateThread`로 해당 클라이언트의 스레드를 만들고, `CRITICAL_SECTION`으로 서버의 공유 상태를 보호한다. 메인 스레드는 select의 대기 사이에 콘솔 Enter 입력도 확인하므로 별도 접속 수락·입력 스레드는 만들지 않는다. Enter 확인은 일반 실행에서만 활성화하며, Tests와 `--check`에서는 콘솔 입력을 읽지 않는다. 종료할 때는 소켓·세션·스레드를 정리한다. 다른 스레드에서 종료를 요청하는 점검 코드도 지원하도록 수동 리셋 이벤트와 `WaitForSingleObject`로 접속 대기 루프의 완료를 기다리며, 동시에 들어오는 종료 요청은 별도 임계 영역으로 순서대로 처리한다. 리슨 소켓에는 select와 논블로킹 accept를 사용하며, 수락한 클라이언트 소켓은 다시 블로킹 모드로 바꾼다. ClientThread 본체는 아래 11/4 작업에서 구현했으며 세션과 스레드 핸들은 서버 종료까지 보관한다. `GetAcceptedCount()`는 누적 수락 수, `GetClientCount()`는 서버가 소유한 세션 수다. 테스트에서 AcceptLoop를 별도 스레드로 호출한 경우에는 서버 종료 후 해당 스레드의 종료를 기다리고 핸들을 정리해야 한다.

GameWorld 생성 시 피카츄를 기존 시작 위치에 놓는다. 좌우 이동은 틱마다 일반 속도 10, 각성 상태는 15를 사용하고 마지막 이동에서 코트 경계를 넘지 않게 제한한다. 공중에서도 좌우 이동이 가능하고, 슬라이딩·승패 동작 중에는 일반 걷기를 막는다. 슬라이딩 자체의 이동은 후속 일정이다.

InputManager는 왼쪽·오른쪽·위쪽 방향키와 Z를 각각 LEFT·RIGHT·UP·ACTION으로 변환한다. 최초 키 입력과 실제 키 해제에만 true를 반환하며 키 반복, 이미 해제된 키, Enter 등 무관한 키에는 false를 반환한다. `Reset()`은 키 상태를 모두 비운다. 실제 화면 메시지 및 SendInput 연결은 후속 일정이고 현재 로컬 게임의 조작은 유지한다.

## 11/4 작업 범위와 강의자료 적용

| 담당 | 완료 내용 |
|---|---|
| 강석주 | ClientSession::ClientThread의 세션별 반복 수신·ProcessPacket 전달·종료 |
| 정진영 | 두 피카츄의 점프 속도·중력·착지와 중복 점프 방지 |
| 김경준 | NetworkClient::SendJoinRoom의 방 번호·닉네임 검증과 JOIN_ROOM 전송 |

참고 자료는 `네트워크게임프로그래밍_시험정리.pdf` Ch5(13·15쪽), Ch6(16~19쪽)와 `네트워크게임프로그래밍_이론정리.pdf` Ch5·Ch6이다. 강의자료의 클라이언트별 CreateThread·LPVOID 인수·반복 수신·소켓 종료 구조를 적용한다. 메인 스레드의 accept와 Win32 임계 영역은 계획서 및 기존 수정 사항을 따른다.

ClientThread는 LPVOID로 받은 세션에서 패킷을 반복 수신하고 ProcessPacket으로 넘긴다. 정상 EOF, 수신 오류 또는 종료 요청이 오면 스레드가 소켓을 닫고 리턴한다. `Stop()`은 종료 플래그를 설정하고 WaitForSingleObject로 종료를 기다린 뒤 핸들을 닫는다. 부분 패킷을 기다리는 중에도 종료할 수 있도록 수신 준비를 최대 100ms씩 확인한다. 상태와 수신 횟수는 CRITICAL_SECTION으로 보호한다. Start·Stop·WaitForFinish는 세션 소유 스레드에서 순서대로 호출한다.

수신 스레드의 실제 실행에 필요한 RecvPacket 기본 처리를 함께 연결했다. TCP 메시지 경계를 헤더와 본문으로 구분하되 계획서의 `uint16_t size + uint8_t type` 3바이트 헤더와 패킷별 크기를 우선한다. 짧게 도착한 데이터는 누적하고 범위를 벗어난 크기·알 수 없는 타입·타입별 크기 불일치·중간 종료는 실패로 처리한다. ProcessPacket의 방 입장·입력 분기와 Disconnect의 방 이탈 알림은 후속 일정이다.

점프는 플레이어별 수직 속도를 저장하고 기존 상수 JumpPower=-25, JumpGravity=1을 사용한다. 공중에서 새 점프를 시작하지 않으며 착지할 때 바닥 위치와 속도를 초기화한다. Up을 계속 누른 채 착지하면 다시 뛰지 않고, 해제 후 다시 누르면 점프한다. 점프 중에도 좌우 이동은 가능하다.

SendJoinRoom은 계획서의 23바이트 JoinRoomPacket을 보낸다. 방 번호는 양수, 닉네임은 비어 있지 않은 UTF-8 문자열이며 NUL을 포함한 16바이트 버퍼에 들어가야 한다. 유효하지 않은 요청은 데이터를 전송하지 않고 GetLastError에 WSAEINVAL을 남긴다. send가 일부 바이트만 보냈으면 나머지를 이어서 보내며, 전송 실패 시 연결을 정리하고 오류를 보존한다. 클라이언트 접속 화면 연결과 서버의 실제 방 배정은 후속 일정이다.

## 11/5 작업 범위

| 담당 | 완료 내용 |
|---|---|
| 강석주 | ClientSession::RecvPacket을 공용 `recvn`으로 헤더 3바이트 → 나머지 바이트 수신하도록 정리, 수신 로그 |
| 정진영 | 두 피카츄의 슬라이딩(원작 moveSlide와 같은 이동량·높이·벽 판정) |
| 김경준 | NetworkClient::SendInput의 INPUT 전송, 송신 반복을 SendAll로 분리 |

RecvPacket은 세션 전용 select 대기 대신 Common의 `recvn`을 쓴다. 블로킹 `recv`는 Windows에서 `shutdown`만으로 깨어난다는 보장이 없으므로 `Stop()`이 소켓을 닫아 대기 중인 수신을 취소한 뒤 스레드 종료를 기다린다. 패킷 사이에서 연결이 끊기면 정상 종료로, 패킷 중간에 끊기면 오류로 기록한다. ClientThread는 받은 패킷마다 `[session 스레드ID] recv type=… size=…`를 출력한다.

슬라이딩은 지상에서 방향키를 누른 채 ACTION을 누르는 순간 시작한다. 40틱 동안 처음 10틱은 3px씩 오르고 14틱은 2px씩 내려온 뒤 바닥에 붙으며, 15틱은 14px, 9틱은 7px 이동한다. 슬라이딩 중에는 걷기·점프·새 슬라이딩 입력을 받지 않고, 끝난 다음 틱부터 누르고 있던 방향으로 다시 걷는다.

SendInput은 LEFT·RIGHT·UP·ACTION 비트만 허용하고 4바이트 INPUT 패킷을 보낸다. 다른 비트가 있으면 보내지 않고 오류로 기록한다.

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

2026-11-04 검증: x64 Debug·Release와 Win32 Debug의 빌드 및 전체 테스트를 통과했다. 실제 세션 스레드에서 조각난 헤더·연속 패킷·정상 EOF·잘못된 길이/타입·중간 종료·수신 취소를 검사했다. 점프의 최고점·중력·착지·좌우 이동·공중 재점프 제한·플레이어별 속도를 확인했고, 실제 TCP 연결에서 JOIN_ROOM의 23바이트 헤더·방 번호·UTF-8 닉네임·최대 길이와 잘못된 요청의 미전송을 검증했다. Server.exe --check의 메인 접속 수락·세션 생성·정상 종료도 세 구성에서 통과했다.
