# VSB/VIP 설정 체크리스트 (벤치마크 정상 동작을 위한 구성)

이 문서는 **UDP / public msgQ / POSIX shm_open+mmap / POSIX sem_open** 기반 IPC 벤치를
VxWorks 23.03에서 정상 수행하기 위해 필요한 **VSB/VIP 구성 요소**를 정리합니다.

> ⚠️ 주의
> 정확한 컴포넌트 이름/의존성은 VSB/VIP 구성(레이어/프로파일/BSP)에 따라 달라질 수 있습니다.
> 아래 항목은 **업로드된 VxWorks 23.03 공식 PDF**에서 확인 가능한 내용 위주로 정리했습니다.

---

## 1) RTP 실행을 위한 기본 구성 (VIP)

VxWorks Platform Programmer's Guide, 23.03에서:
- VIP에서 **INCLUDE_RTP**를 포함하면 기본 RTP 지원(오버랩 VM, MMU 보호 포함)이 구성됨
(Platform Programmer's Guide 23.03, “Configuring VxWorks With Basic RTP Support”, p.11)

또한 같은 문서에서 RTP 개발/운영에 유용한 추가 VIP 컴포넌트를 예시로 나열합니다:
- `INCLUDE_ROMFS` : RTP를 ROMFS로 이미지에 포함
- `INCLUDE_RTP_APPL_USER`, `INCLUDE_RTP_APPL_INIT_STRING`, `INCLUDE_RTP_APPL_INIT_BOOTLINE`, `INCLUDE_RTP_APPL_INIT_CMD_SHELL_SCRIPT`
  : 부팅 시 RTP 자동 실행 방식
- `INCLUDE_SHL` : shared libraries
- `INCLUDE_RTP_HOOKS` : 프로세스 라이프사이클 hook
- `INCLUDE_POSIX_PTHREAD_SCHEDULER` + `INCLUDE_POSIX_CLOCK` : 프로세스에서 pthread/clock 지원
- `INCLUDE_SHELL_*` 및 `INCLUDE_NET_SYM_TBL`/`INCLUDE_STANDALONE_SYM_TBL` : CLI 실행/디버깅에 유용
(Platform Programmer's Guide 23.03, “Additional VIP Components for RTPs”, p.11 근방)

✅ 벤치 관점 권장
- 반복 실행/조건 변경을 많이 할 거라면 개발 단계에서 **쉘 관련 컴포넌트**를 켜두는 게 편합니다.

---

## 2) UDP(Socket) 벤치를 위한 구성

VxWorks Sockets Programmer's Guide 23.03에서:
- `ioctl(... FIONBIO ...)`로 non-blocking 설정이 가능
- input(예: recv)에서 non-blocking이면 데이터 없을 때 `EWOULDBLOCK`
- output(예: send)에서 커널이 **application buffer → socket send buffer로 copy**한다고 설명
(Sockets Programmer's Guide 23.03, “Using an ioctl() Call to Make the Socket Non-Blocking”)

또한 socket option 표에서:
- `SO_RCVTIMEO`의 기본값이 **Infinite**로 명시됨
(Sockets Programmer's Guide 23.03, “Socket Options” 표)

✅ 벤치 관점 권장
- RR에서 타임아웃 기반 loss를 재려면 `SO_RCVTIMEO`를 명시적으로 설정하거나 `select()`를 사용하세요.
- timeout_ms는 결과에 반드시 함께 기록하세요(조건의 일부).

---

## 3) POSIX shm_open + mmap (SHM 벤치) 구성

VxWorks Platform Programmer's Guide 23.03에서:
- POSIX shared memory objects는 `shm_open()`으로 FD를 얻고 `mmap()`으로 매핑해서 사용
- 이를 위해 필요한 커널 컴포넌트:
  - `INCLUDE_POSIX_SHM` : shmFs 제공(shm_open/shm_unlink)
  - `INCLUDE_POSIX_MAPPED_FILES` : mapped files용 `mmap()` 확장
(Platform Programmer's Guide 23.03, “POSIX Shared Memory Objects”, p.95)

✅ 벤치 관점 체크
- RTP/커널에서 `shm_open`/`mmap`이 동작하는지
- shmFs 네임스페이스가 준비되는지(환경에 따라 `/shm`)

---

## 4) POSIX sem_open/sem_timedwait (notify 채널)

현재 업로드된 23.03 PDF 세트에서는 “POSIX named semaphore 활성화 컴포넌트명(INCLUDE_*)”을
명시적으로 찾지 못했습니다.

✅ 권장 확인 방법
- VIP component selection에서 POSIX semaphore/semPxLib/sem_open 관련 항목을 포함했는지 확인
- RTP에서 `sem_open()`이 정상 호출되는지 작은 샘플로 점검

---

## 5) System Viewer/WindView (선택: 스파이크 원인 분석)

VxWorks System Viewer User's Guide 23.03에서 System Viewer 동작을 위한 컴포넌트가 정리돼 있습니다:
- `INCLUDE_WINDVIEW`
- `INCLUDE_WINDVIEW_CLASS`
- `INCLUDE_RBUFF`
- `INCLUDE_SEQ_TIMESTAMP`, `INCLUDE_USER_TIMESTAMP`, `INCLUDE_SYS_TIMESTAMP`
- `INCLUDE_TRIGGERING`
- `INCLUDE_WVNETD`
- `INCLUDE_SYSTEMVIEWER_AGENT`
(System Viewer User's Guide 23.03, “Necessary Components”, p.92)

✅ 벤치 관점
- p99/max 스파이크가 논쟁 포인트가 되면 System Viewer 로그로 “그 순간의 이벤트”를 같이 제시하면 강력합니다.

---

## 6) 최소 구동 확인 순서(권장)
1) VIP에서 RTP 실행 확인 (`INCLUDE_RTP`)
2) UDP loopback RR
3) UDP board_ip RR
4) msgQ RR
5) shm+sem RR
6) System Viewer(선택)로 스파이크 상관 분석
