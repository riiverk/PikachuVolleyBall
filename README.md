# PikachuVolleyBall

네트워크 게임 프로그래밍 3팀 텀 프로젝트. 2026-10-31~11-01 계획에 맞춰 기존 로컬 게임을 클라이언트 프로젝트로 옮기고 온라인 게임의 공통 규약과 클래스 틀을 구성했다.

## 열기와 실행

- Visual Studio에서 `PikachuVolleyBall/PikachuVolleyBall.slnx`를 연다. `Client`를 시작 프로젝트로 설정한다.
- C++20, Windows SDK, Visual Studio C++ 도구가 필요하다. 팀 기준 v145를 우선 사용하며 설치되어 있지 않으면 기존 설정대로 v143을 사용한다.
- 저장소 루트에서 PowerShell로 `./Tools/Build.ps1 -Verify`를 실행하면 세 프로젝트를 빌드하고 검증한다. `-Configuration Release`, `-Platform Win32`도 지원한다.
- 실행 파일은 `PikachuVolleyBall/bin/x64/Debug/Client/Client.exe`에 생성된다. `Resources`도 자동 복사하고 실행 시 실행 파일 위치에서 리소스를 찾으므로 다른 작업 디렉터리에서 시작해도 된다.
- 현재 Client는 기존 **로컬 2인 게임**으로 실행된다. 1P는 R(점프), D/G(좌우), Z(스파이크/슬라이딩), 2P는 방향키와 Enter를 사용한다.
- Server는 현재 클래스 틀만 빌드한 콘솔 프로그램이다. 실행 시 미구현 상태를 출력하며 포트를 열지 않는다. `Server.exe --check`로 실행 파일을 확인할 수 있다.

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

빈 함수에는 이후 구현 일정의 TODO를 표시했다. `Connect`·`InitServer`·`SendPacket` 등은 구현 전까지 `false`, 스레드 진입점은 `ERROR_CALL_NOT_IMPLEMENTED`를 반환한다. 온라인 UI 전환과 물리 시뮬레이션은 아직 연결하지 않았다. 스캐폴드의 기본 상태 복사는 네트워크 수신이나 시뮬레이션 구현을 의미하지 않는다.

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

`Tests.exe`는 실제 loopback TCP로 1바이트씩 나뉜 패킷, 한 번에 붙어 온 두 패킷, 중간 연결 종료, 소켓 오류와 잘못된 인자를 확인한다. 12개 패킷의 헤더·크기와 알려진 INPUT 바이트열을 검사하고 모든 신규 클래스의 함수 정의가 연결되는지도 확인한다. 전체 온라인 대전 테스트는 후속 일정에서 실제 통신·게임 로직을 구현한 뒤 수행한다.

2026-11-01 로컬 검증 결과: v143 도구로 x64 Debug, x64 Release, Win32 Debug의 Client·Server·Tests 빌드와 기반 검증을 모두 통과했다. 이미지·사운드 11개는 원본과 바이트가 같으며 스프라이트 JSON도 내용이 같다. 기존 로컬 게임의 수치 변환 경고는 남아 있다.

Client.exe 실행 후 `Pikachu_VolleyBall` 창 생성과 프로세스 응답을 확인했다. UI 캡처 도구의 시간 초과 및 최소화된 창 상태로 경기 화면·키 조작은 자동 확인하지 못했다. 온라인 대전이 동작한다는 의미는 아니다.
