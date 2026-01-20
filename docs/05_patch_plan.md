# IPC Bench 개선 수정안 (Patch Plan)

대상: `IPC_Bench` (DKM .out + RTP .vxe + common transport)
목표: **측정 결과의 신뢰도(재현성/설명력) 향상** + **벤치 스펙의 “해석 일관성” 확보**

---

## 0) 수정안 요약(핵심만)

1. **warmup / duration 정의 통일**
   - RR과 ONEWAY가 “duration 의미”가 다르게 동작하는 문제를 수정
2. **송신 주기 생성(pacing) 개선 옵션 추가**
   - tick 기반 `taskDelay()`만으로 200Hz 이상에서 흔들릴 수 있으므로 “tick + 짧은 spin” 옵션 제공
3. **ONEWAY TX drop(전송 실패) 계측 추가**
   - SHM/msgQ에서 full/실패가 발생할 때 TX 측 지표가 누락되는 문제 보완
4. **결과 보고 형식 고정**
   - 어떤 조건에서 어떤 정책(timeout/drop)이었는지 자동으로 로그/CSV에 포함

---

## 1) warmup/duration 정의 통일

### 1.1 현재 문제(관찰)
- RR client 측정 루프가 “샘플 수(count) 기준 종료”로 되어 있으면,
  - warmup 동안 샘플을 기록하지 않더라도 **전체 실행 시간이 duration보다 길어질 수 있음**
- 반면 ONEWAY는 “시간(end timestamp) 기준 종료”를 쓰는 경우가 많아,
  - warmup이 duration 안에 포함되는 형태가 됨

→ 결과적으로 **RR과 ONEWAY의 “duration” 의미가 서로 달라져** 보고서/설명 시 혼란 발생.

### 1.2 표준화 제안(권장 규칙)
- `warmup_sec` : “측정에서 제외되는 준비 구간”
- `duration_sec` : “실제 측정 구간(샘플링 구간)의 길이”
- 총 실행 시간 = warmup + duration

구현은 “시간 기준”으로 통일:
- `t_start = now()`
- `t_warmup_end = t_start + warmup`
- `t_end = t_warmup_end + duration`
- 루프는 `now() < t_end` 동안 수행
- 샘플 저장은 `now() >= t_warmup_end` 일 때만 수행

### 1.3 구현 체크리스트(파일/함수)
- `common/src/bench_runner.c`
  - `run_rr_client()` / `run_rr_server()` / `run_oneway_*()` 모두 위 규칙으로 통일
  - 샘플 버퍼 capacity:
    - `capacity = rate_hz * duration_sec`
    - warmup은 capacity 계산에서 제외
  - “측정 구간 샘플이 capacity를 초과”하는 경우 정책 고정:
    - (권장) 초과 샘플은 버리기(혹은 마지막 N개 유지) — **어느 쪽이든 문서에 명시**

---

## 2) 송신 pacing 개선(200Hz 이상에서의 주기 품질)

### 2.1 왜 필요한가?
- `taskDelay(ticks)`는 tick 해상도에 종속
- 예: sysClkRate=1000Hz(1ms tick)에서 200Hz(5ms)는 괜찮을 수 있으나,
  500~1000Hz에서는 분해능/스케줄링 영향이 커짐
- 벤치 결과가 **transport 지터가 아니라 “송신 스케줄링 지터”**를 더 많이 반영할 수 있음

### 2.2 구현 옵션: tick + spin 하이브리드
목표: CPU를 과도하게 태우지 않으면서도 “마지막 수백 µs” 정밀도를 확보

권장 방식:
- 매 루프마다 `target_ts`(다음 송신 예정 시각)를 누적
- 남은 시간이 큰 동안은 `taskDelay()`
- 임계 구간(예: remain_ns < 300~500µs)은 짧은 busy-wait(spin)

의사코드:
```
period = 1e9 / rate
spin_th = 400_000   # 400us (튜닝)

target = start
while now < end:
    target += period
    while True:
        remain = target - now
        if remain <= 0:
            break
        if remain > spin_th:
            taskDelay(ns_to_ticks(remain - spin_th))
        else:
            busy_spin(remain)
            break
    send()
```

### 2.3 구현 위치/빌드 옵션
- `common/src/bench_time.c` 또는 `common/src/bench_runner.c`
  - `bench_sleep_until()`을 옵션형으로 변경
- 컴파일 옵션:
  - `BENCH_PACING_HYBRID=1` (기본 0)

---

## 3) ONEWAY TX drop 계측 추가

### 3.1 문제
- SHM ring이 full이거나 msgQ가 가득 차서 send 실패하는 경우,
  - ONEWAY TX에서 “전송 실패”가 통계에 반영되지 않으면
  - 결과 해석이 왜곡될 수 있음(실제로는 못 보냈는데 loss가 낮게 보이는 등)

### 3.2 수정안
- `bench_stats_t`에 TX 관련 카운터 추가:
  - `tx_fail` (send 실패 횟수)
  - (선택) `tx_retry` (재시도 정책 사용 시)
- ONEWAY TX에서 `send()` 실패 시 `tx_fail++` 기록

### 3.3 출력/CSV 반영
- 콘솔 summary + CSV에 `tx_fail` 포함

---

## 4) RR/ONEWAY 공통: timeout 정책 고정 및 로그화

### 4.1 RR(Client) timeout(transport별 적용)
- UDP: `setsockopt(... SO_RCVTIMEO ...)` 또는 `select()`
- msgQ: `msgQReceive(timeout_ticks)`
- sem: `sem_timedwait()`

권장:
- `timeout_ms`를 공통 cfg로 두고
- transport별로 같은 의미로 적용
- 결과 로그에 `timeout_ms`를 반드시 포함

### 4.2 로그/CSV 추천 컬럼
- transport, mode(rr/oneway), direction(c2s/s2c), payload, rate, warmup, duration
- timeout_ms
- sent, recv, loss, out_of_order, tx_fail
- p50, p90, p99, max (RR=RTT / ONEWAY=jitter)

---

## 5) SHM(=shm_open+mmap) 관련 운영 주의사항
- 링버퍼 인덱스 업데이트/읽기 앞뒤로 memory barrier 필요
- PPC SMP 환경은 캐시/메모리 가시성 이슈가 나타날 수 있음
- full 정책(drop/overwrite)과 결과 해석을 문서에 명시하고 고정

---

## 6) 패치 적용 순서(추천)
1) warmup/duration 규칙 통일
2) ONEWAY TX drop 계측 추가
3) pacing 하이브리드 옵션 추가(필요 시)
4) 로그/CSV 컬럼 고정

---

## 7) 변경 후 검증 체크리스트
- 동일 조건 3회 반복 시 p99/max 분산이 줄어드는지
- warmup 값을 바꿔도 “측정 구간 duration”이 동일하게 유지되는지
- ONEWAY에서 tx_fail이 0이면 recv율/손실이 일관적인지
