# 실행 방법 및 결과 분석

## 빌드
- Workbench Makefile Project로 루트(이 레포) 추가
- `scripts/vx_set_env.cmd`로 VxWorks 환경변수 설정
- VSCode에서 `.vscode/tasks.json`의 `Vx Make: ...` 태스크(모드/타겟 별) 또는
- 터미널에서 `D:\WindRiver\wrenv.exe cmd.exe /d /s /c "call scripts\vx_set_env.cmd && make [all|dkm|rtp] [MODE=debug]"` 실행
- `config.mk`에서 `WIND_BASE`/`VSB_DIR`/`LLVM_ROOT`/`MODE` 등의 설정을 확인

## 실행 예시
### RTP 서버
- `bench_rtp -s --transport udp --bind 0.0.0.0 --port 41000 --mode rr`
- 환경 변수 `BENCH_TRANSPORT=msgq/shmsem/local` 설정 시 해당 transport로 RR/ONEWAY 실행
- 데이터 수집 중 `--rate`, `--dur`, `--payload`, `--tag` 옵션은 `bench_rtp` CLI에서 조정 가능

### DKM 클라이언트
- `.out` 로드 후:
- loopback:
  - `C benchClientRun("udp","127.0.0.1",41000,0,"udp_loop",1,200,30,256)`
- board ip:
  - `C benchClientRun("udp","<board_ip>",41000,0,"udp_boardip",1,200,30,256)`
- ONEWAY receiver는 `mode=oneway`로 실행하고 클라이언트는 동일한 환경변수+옵션으로 `benchClientRun(..., BENCH_MODE_ONEWAY, ...)` 호출

## 권장 매트릭스
- payload: 0/64/256/900 bytes
- rate: 200/500/1000 Hz
- duration: 30s 이상(가능하면 60s)
- transport별로 `BENCH_TRANSPORT`/`BENCH_QUEUE_OPTS`(msgQ) 또는 `BENCH_RING_SIZE`(shmsem)를 바꿔서 조건 비교

## 해석 팁
- 평균보다 **p99/max** 우선, loss/ooo도 주요 보조 지표
- `BENCH_TRANSPORT=msgq`, `BENCH_TRANSPORT=shmsem` 등으로 transport별 tail/손실/지터 비교
- loopback 대비 board_ip 결과 차이가 크면 NIC/IRQ/드라이버 영향
- ONEWAY는 inter-arrival jitter를 기록하므로 `p50/p90/p99` 상승 구간을 WindView로 추적하면 ISR/태스크 스케줄 영향도 확인 가능
