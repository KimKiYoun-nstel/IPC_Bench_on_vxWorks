# 구현 상세 가이드 (VxWorks 23.03 기준) — IPC 벤치 하네스

이 문서는 `docs/01_design.md`를 **“실제로 구현할 수 있을 정도로”** 구체화한 구현 가이드입니다.  
대상: T2080 + VxWorks 23.03, DKM(.out) ↔ RTP(.vxe)

> **문서 기반(로컬 제공본)**  
> - *VxWorks Application Programmer's Guide, 23.03*  
>   - “Message Queues”, “Inter-Process Communication With Public Message Queues”  
>   - “Intertask and Interprocess Communication / Inter-Process Communication With Public Objects”  
>   - “Shared Data Structures”, “Interrupt Locks”  
> - *VxWorks Sockets Programmer's Guide, 23.03*  
>   - “Using an ioctl( ) Call to Make the Socket Non-Blocking”  
>   - “Using a connect( ) Call with a Datagram Socket”  
>   - blocking/non-blocking, send buffer copy 동작  
> - *VxWorks System Viewer User's Guide, 23.03* / *VxWorks Analysis Tools User's Guide, 23.03*  
>   - System Viewer(WindView)로 task/ISR/네트워크 스택 이벤트 트레이스

> **추가 참고(공식 도메인 접근이 제한되어, 미러된 API 레퍼런스 PDF에서 확인된 내용)**  
> - POSIX shared memory objects(shmLib): shmFs가 `/shm`로 마운트되고 `INCLUDE_POSIX_SHM`로 포함될 수 있음  
>   (최종 구현 전, 사용 중인 23.03 API Reference에서 `shmLib`, `mmanLib` 항목 확인 권장)

---

## 1. 구현 범위(이번 벤치에서 “반드시” 구현해야 하는 것)

### 1) 공통: 벤치 러너(core)
- `common/src/bench_runner.c`
  - RR(REQ/RSP) 모드: RTT 샘플 수집 + p50/p90/p99/max + loss
  - ONEWAY(DATA) 모드: inter-arrival jitter(권장) + loss/out-of-order

### 2) Transport(최소 1차 목표)
- `common/src/transport_udp.c` : **우선 완성**
- 그 다음 후보:
  - `common/src/transport_msgq.c` : Public message queue
  - `common/src/transport_shm_sem.c` : SHM ring + notify(sem/msgQ)
  - `common/src/transport_local.c` : local domain socket(지원 여부 확인 필요)

---

## 2. RR vs ONEWAY: 구현/해석 관점 핵심

### RR(REQ/RSP) — “clock 동기화 논쟁 최소”
- Client 한쪽에서만 t0/t1을 찍고 RTT를 계산 → clock 동기화 이슈가 거의 없음
- 1차 의사결정은 RR 결과(p99/max)가 가장 설득력 높음

### ONEWAY(DATA stream)
- “진짜 one-way latency”는 시계 기준 논쟁 가능
- 기본 지표는 **Receiver inter-arrival jitter**
- 참고 지표로만 `recv_ts - t0` (동일 보드 monotonic 가정 명시)

---

## 3. 공통 구현 상세 (bench_runner)

### 3.1 샘플 버퍼 크기 산정
- N = rate_hz * duration_sec
- 예: 1000 Hz * 60 s = 60,000 samples
- 샘플 배열은 스택에 두지 말고(크면 위험), 정적/힙(초기 1회)로

### 3.2 측정 루프에서 금지
- `printf`, 동적 할당, 파일 I/O
- tail(p99/max)를 크게 왜곡함

### 3.3 RR(Client) 루프
1) `t0 = now_ns()`
2) REQ(seq,t0,payload) 전송
3) RSP(seq) 수신(타임아웃 적용)
4) `t1 = now_ns()`
5) RTT=t1-t0 저장

- loss: timeout 발생 시 loss++
- out-of-order: seq mismatch 카운트

### 3.4 RR(Server) 루프
- recv REQ → 즉시 RSP echo
- 서버에서는 추가 처리 금지(벤치 목적)

### 3.5 ONEWAY(Receiver) 루프
- `t = now_ns()`, `dt = t - prev_t`
- `jitter = dt - (1e9/rate_hz)`
- seq로 loss/out-of-order 체크

### 3.6 시간 함수
- 템플릿 기본: `clock_gettime(CLOCK_MONOTONIC)`
- 만약 이미지에서 POSIX time 구성이 제한되면 fallback 후보:
  - `tickGet()` + `sysClkRateGet()` 환산(해상도 한계 있음)
  - 가능하면 timestamp facility 사용(환경 의존)

---

## 4. Transport별 구현 가이드

### 4.1 UDP(AF_INET, SOCK_DGRAM)

#### 4.1.1 connected datagram 권장
- datagram에 `connect()`를 호출하면 실제 연결 없이도 `send()/recv()` 사용 가능
- 지정 peer에서 온 datagram만 수신되도록 제한됨

#### 4.1.2 non-blocking 설정
- non-blocking은 `ioctl(FIONBIO)`로 설정 가능
- non-blocking 수신은 `EWOULDBLOCK` 처리 필요

권장:
- RR 측정에서는 **blocking + timeout(select 또는 SO_RCVTIMEO)**가 tail 비교에 더 유리한 경우가 많음

#### 4.1.3 send의 copy 비용
- send/sendto/write는 애플리케이션 버퍼 → 소켓 send buffer로 kernel copy가 수행됨  
  (따라서 메시지 크기 증가 시 copy 비용이 tail에 반영될 수 있음)

#### 4.1.4 loopback vs board_ip
- `127.0.0.1` vs `board_ip`는 NIC/드라이버/IRQ 경로 개입 차이가 있을 수 있어 반드시 비교

---

### 4.2 Public Message Queue(msgQ)

#### 4.2.1 Public object 핵심 규칙
- public msgQ는 `msgQOpen()`로 생성/오픈
- 이름이 `/`로 시작하면 public namespace에서 검색/생성

#### 4.2.2 full-duplex는 큐 2개
- 방향별로 1개씩(REQ 큐, RSP 큐)

#### 4.2.3 파라미터/정책
- `maxMsgs`, `maxMsgLength`, `options(FIFO/PRIORITY)`
- 큐가 가득 찼을 때:
  - `NO_WAIT`로 drop(손실 증가)
  - timeout/WAIT_FOREVER로 backpressure(지연 증가)
- 벤치 결과 해석을 위해 “정책”을 실험 조건에 반드시 기록

---

### 4.3 SHM + notify(sem/msgQ)

#### 4.3.1 권장: SPSC 링버퍼 2개(방향별)
- DKM→RTP 링 1개, RTP→DKM 링 1개

#### 4.3.2 notify는 반드시 필요
- sem/msgQ/event 중 하나로 “데이터 도착”을 알려서 recv가 sleep 가능하게

#### 4.3.3 메모리 장벽/캐시(주의)
- PPC SMP 환경은 메모리 오더링/캐시 영향이 큼
- idx 업데이트와 payload 가시성 보장을 위해 barrier/flush 전략 필요(BSP/아키텍처에 따라 상이)

---

## 5. System Viewer/WindView 활용(스파이크 원인 추적)
- p99/max가 튀는 구간에서 “그 순간 무슨 이벤트가 있었는지”를 타임라인으로 상관 분석
- 예: ISR 폭주, 높은 우선순위 태스크 선점, 네트워크 스택 이벤트 등
- 벤치 숫자만으로는 설명이 어려운 tail spike를 “근거 자료”로 만들 때 유용

---

## 6. 구현 우선순위(추천)
1) UDP transport + RR 완성
2) loopback vs board_ip 결과 확보
3) msgQ RR 비교
4) SHM+notify(필요 시)
5) ONEWAY(inter-arrival jitter) 추가
