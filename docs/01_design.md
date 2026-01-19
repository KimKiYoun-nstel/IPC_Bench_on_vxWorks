# IPC Benchmark 설계 디자인

## 목적
- T2080 + VxWorks 23.03에서 DKM(.out) ↔ RTP(.vxe) IPC 후보들의 지연/지터를 동일 조건으로 비교
- 주요 지표: **p50/p90/p99/max**, loss, out-of-order

## IPC 후보(1차)
- `udp` : AF_INET/UDP (loopback vs board_ip 포함)
- `local` : AF_LOCAL/COMP 로컬 소켓(이미지 옵션에 따라)
- `msgq` : Public Message Queue
- `shmsem` : SHM ring + notify(sem/msgQ)

## 측정 시나리오
### RR (REQ/RSP) — 메인 결과
- Client(initiator)가 `t0=send 직전 now()`를 패킷 헤더에 넣고, RSP 수신 시 `t1=now()`로 RTT 계산
- Server(responder)는 수신 즉시 echo
- 장점: 클럭 동기 문제 최소, 반박이 가장 적음

### ONEWAY (DATA stream) — 보조
- Receiver에서 inter-arrival jitter를 측정: `(recv[i]-recv[i-1]) - period`
- 참고로 `recv_ts - t0`도 출력 가능(동일 보드 monotonic 가정 명시)

## 설계 원칙
- 공용 러너(`bench_runner.c`)는 transport 인터페이스만 사용
- transport 교체만으로 동일 시나리오/조건 재사용
- 측정 루프 내 `printf`/동적할당 금지(왜곡 방지)

## 구현 포인트
- `bench_transport_*` 구현 시: open/bind/connect/send/recv/timeout 처리 통일
- RR server는 최소 작업(즉시 echo)
- SHM full-duplex는 SPSC 링 2개(방향별)
- msgQ는 queue depth/드롭 정책을 실험 조건에 기록
