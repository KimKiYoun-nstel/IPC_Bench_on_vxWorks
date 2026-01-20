# 실행 방법 및 결과 분석

## 빌드
- Workbench Makefile Project로 루트(이 레포) 추가
- `scripts/vx_set_env.cmd`로 VxWorks 환경변수 설정
- VSCode에서 `.vscode/tasks.json`의 `Vx Make: ...` 태스크(모드/타겟 별) 또는
- 터미널에서 `D:\WindRiver\wrenv.exe cmd.exe /d /s /c "call scripts\vx_set_env.cmd && make [all|dkm|rtp] [MODE=debug]"` 실행
- `config.mk`에서 `WIND_BASE`/`VSB_DIR`/`LLVM_ROOT`/`MODE` 등의 설정을 확인

## 실행 개요
1) 서버(RX) 먼저 실행  
2) 클라이언트(TX) 실행  
3) 콘솔 출력 확인(p50/p90/p99/max, loss, ooo, jitter)  

## 실행 중 진행 로그
- 실행 중 1초 간격으로 진행 로그가 출력됨
  - RR 클라이언트: `sent/s`, `recv/s`, `loss/s`, `ooo/s`, `tx_fail/s`
  - RR 서버: `recv/s`, `rsp/s`
  - ONEWAY RX: `recv/s`, `loss/s`, `ooo/s`
  - ONEWAY TX: `sent/s`, `tx_fail/s`

## 동시 실행 방법(중요)
- `rtp exec`는 기본적으로 **셸을 점유**하므로, 같은 콘솔에서 DKM을 실행하려면 RTP를 **백그라운드/분리**해야 함
- 방법 A: `rtp exec -d` 사용 (지원되는 경우)
  - `rtp exec -d bench_rtp.vxe -- -s ...`
- 방법 B: `&`로 백그라운드 실행 (쉘 지원 시)
  - `rtp exec bench_rtp.vxe -- -s ... &`
- 방법 C: **두 번째 콘솔/쉘**(telnet/serial)에서 DKM 실행

> 서버는 **클라이언트 실행 시간 + 여유분**(시작 지연)을 포함하도록 `--dur`을 더 길게 잡는 것이 안전합니다.
> 예: 클라이언트 `dur=30`이면 서버는 `dur=40` 권장.

## 공통 파라미터 정리
- transport: `udp` / `msgq` / `shmsem` / `local`
- mode: `rr`(요청/응답, DKM에서는 `1`) / `oneway`(단방향, DKM에서는 `0`)
- payload: 바이트 크기 (예: 0/64/256/900)
- rate: 전송 주기(Hz)
- duration: 측정 시간(초)
- tag: 출력 라벨(콘솔 표시용)
- port: UDP에서만 사용 (msgq/shmsem/local은 0 사용 권장)
- name/path:
  - msgq/shmsem: 동일한 base 이름 사용(예: `bench_msgq`)
  - local: 소켓 경로 사용(예: `/tmp/bench_local`)
- **서버 측 rate/payload는 실제 송신 주기를 결정하지 않음**
  - RR/ONEWAY의 실제 페이로드 길이는 **클라이언트가 보낸 값**이 기준
  - 서버는 내부 버퍼 최대치(현재 2048 bytes)까지 수신/응답 가능
  - 따라서 서버는 `--payload`를 크게 줄 필요가 없고, **클라이언트 payload가 2048을 넘지 않도록** 맞추는 것이 안전

## RTP 실행 형식
- `rtp exec bench_rtp.vxe --` 형태로 실행 (일관성 유지)
- 서버:
  - `rtp exec bench_rtp.vxe -- -s --transport <udp|msgq|shmsem|local> --bind <ip|name|path> --port <port> --mode <rr|oneway> --dur <sec> --tag <label>`
- 클라이언트:
  - `rtp exec bench_rtp.vxe -- -c --transport <udp|msgq|shmsem|local> --dst <ip|name|path> --port <port> --mode <rr|oneway> --rate <Hz> --dur <sec> --payload <bytes> --tag <label>`
> RR 서버는 `--rate`, `--payload`를 넣을 필요가 없습니다. (실제 송신 주기/페이로드는 클라이언트가 결정)

## DKM 실행 형식 (VxWorks 쉘)
- 서버:
  - `C benchServerStart("transport","bind_or_name",port,"name",mode,rate,duration,payload)`
- 서버(RR, 간단 호출):
  - `C benchServerStartRR("transport","bind_or_name",port,"name",duration)`
- 클라이언트:
  - `C benchClientRun("transport","dst_or_name",port,"name","tag",mode,rate,duration,payload)`
- mode 값:
  - `1` = RR, `0` = ONEWAY

## DKM 모듈 로드/언로드 예시
- 로드:
  - `-> ld < bench_dkm.out`
- 로드 확인:
  - `-> moduleShow`
- 언로드(모듈 이름):
  - `-> unld "bench_dkm.out"`
- 언로드(모듈 ID):
  - `-> unld <moduleId>`

> DKM에 별도의 stop 함수는 현재 없습니다.  
> `benchServerStart()`/`benchClientRun()`은 duration이 끝나면 자동 종료되며,  
> 강제 중단이 필요하면 별도 태스크로 실행 후 `td <taskId>` 방식이 필요합니다.

## IPC별 RR 실행 예시
### UDP (RTP 서버 + DKM 클라이언트)
- RTP 서버:
  - `rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode rr --dur 40 --tag udp_srv`
- DKM 클라이언트 (loopback):
  - `C benchClientRun("udp","127.0.0.1",41000,0,"udp_loop",1,200,30,256)`
- DKM 클라이언트 (board ip):
  - `C benchClientRun("udp","<board_ip>",41000,0,"udp_boardip",1,200,30,256)`

### UDP (DKM 서버 + RTP 클라이언트)
- DKM 서버:
  - `C benchServerStartRR("udp","0.0.0.0",41000,0,40)`
- RTP 클라이언트:
  - `rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 30 --payload 256 --tag udp_cli`

### msgQ (RTP 서버 + DKM 클라이언트)
- RTP 서버:
  - `rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode rr --dur 30 --tag msgq_srv`
- DKM 클라이언트:
  - `C benchClientRun("msgq","bench_msgq",0,0,"msgq_cli",1,200,30,256)`

### msgQ (DKM 서버 + RTP 클라이언트)
- DKM 서버:
  - `C benchServerStartRR("msgq","bench_msgq",0,0,40)`
- RTP 클라이언트:
  - `rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 200 --dur 30 --payload 256 --tag msgq_cli`

### shmsem (RTP 서버 + DKM 클라이언트)
- RTP 서버:
  - `rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode rr --dur 30 --tag shm_srv`
- DKM 클라이언트:
  - `C benchClientRun("shmsem","bench_shm",0,0,"shm_cli",1,200,30,256)`

### shmsem (DKM 서버 + RTP 클라이언트)
- DKM 서버:
  - `C benchServerStartRR("shmsem","bench_shm",0,0,40)`
- RTP 클라이언트:
  - `rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 200 --dur 30 --payload 256 --tag shm_cli`

### local (RTP 서버 + DKM 클라이언트)
- RTP 서버:
  - `rtp exec bench_rtp.vxe -- -s --transport local --bind /tmp/bench_local --port 0 --mode rr --dur 30 --tag local_srv`
- DKM 클라이언트:
  - `C benchClientRun("local","/tmp/bench_local",0,0,"local_cli",1,200,30,256)`

### local (DKM 서버 + RTP 클라이언트)
- DKM 서버:
  - `C benchServerStartRR("local","/tmp/bench_local",0,0,40)`
- RTP 클라이언트:
  - `rtp exec bench_rtp.vxe -- -c --transport local --dst /tmp/bench_local --port 0 --mode rr --rate 200 --dur 30 --payload 256 --tag local_cli`

## ONEWAY 실행 예시 (RX=서버, TX=클라이언트)
### UDP
- RTP 서버(RX):
  - `rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 500 --dur 30 --tag udp_rx`
- DKM 클라이언트(TX):
  - `C benchClientRun("udp","127.0.0.1",41000,0,"udp_tx",0,500,30,256)`

### UDP (DKM 서버 RX + RTP 클라이언트 TX)
- DKM 서버(RX):
  - `C benchServerStart("udp","0.0.0.0",41000,0,0,500,40,256)`
- RTP 클라이언트(TX):
  - `rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 500 --dur 30 --payload 256 --tag udp_tx`

### msgQ/shmsem/local
- RTP 서버(RX)에서 `--mode oneway` 사용
- DKM 클라이언트(TX)에서 `mode=0` 사용
- transport와 name/path는 RR 예시와 동일

## 권장 매트릭스
- payload: 0/64/256/900 bytes
- rate: 200/500/1000 Hz
- duration: 30s 이상(가능하면 60s)
- transport별로 동일 조건에서 반복 실행(최소 3회)하고 중앙값/최악값 비교 권장

## 해석 팁
- 평균보다 **p99/max** 우선, loss/ooo도 주요 보조 지표
- RR: `[BENCH][RR]` 출력의 `min/p50/p90/p99/max`, `loss/ooo` 확인
- ONEWAY: `[BENCH][ONEWAY][RX]`의 `jitter_abs` 퍼센타일과 `[BENCH][ONEWAY][TX] sent` 확인
- loopback 대비 board_ip 결과 차이가 크면 NIC/IRQ/드라이버 영향
- ONEWAY는 inter-arrival jitter를 기록하므로 `p50/p90/p99` 상승 구간을 WindView로 추적하면 ISR/태스크 스케줄 영향도 확인 가능

## 해상도 확인/고해상도 사용
- 현재 타겟에서 해상도 확인:
  - `-> sysClkRateGet()` (tick 해상도, 예: 1000이면 1ms)
  - `-> sysTimestampFreq()` (0이 아니면 고해상도 타임스탬프 사용 가능)
- DKM에서는 `sysTimestampFreq()`가 0이 아니면 자동으로 **고해상도 타임스탬프**를 사용하도록 구현됨
- `sysTimestampFreq()`가 0이면 tick 기반으로 동작하므로 1ms 이하 측정이 0ns로 나올 수 있음

# RR ?? ?? (?? IPC, ?? ??)
## UDP (RTP ?? + DKM ?????)
- RTP ??:
  - rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode rr --dur 65 --tag udp_srv
- DKM ????? (16? ??):
```
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r200_p256",1,200,60,256)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r200_p512",1,200,60,512)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r200_p1024",1,200,60,1024)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r200_p1500",1,200,60,1500)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r400_p256",1,400,60,256)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r400_p512",1,400,60,512)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r400_p1024",1,400,60,1024)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r400_p1500",1,400,60,1500)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r800_p256",1,800,60,256)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r800_p512",1,800,60,512)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r800_p1024",1,800,60,1024)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r800_p1500",1,800,60,1500)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r100_p256",1,100,60,256)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r100_p512",1,100,60,512)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r100_p1024",1,100,60,1024)
C benchClientRun("udp","127.0.0.1",41000,0,"udp_rr_r100_p1500",1,100,60,1500)
```
## UDP (DKM ?? + RTP ?????)
- DKM ??:
  - C benchServerStartRR("udp","0.0.0.0",41000,0,65)
- RTP ????? (16? ??):
```
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 60 --payload 256 --tag udp_rr_r200_p256
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 60 --payload 512 --tag udp_rr_r200_p512
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 60 --payload 1024 --tag udp_rr_r200_p1024
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 60 --payload 1500 --tag udp_rr_r200_p1500
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 400 --dur 60 --payload 256 --tag udp_rr_r400_p256
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 400 --dur 60 --payload 512 --tag udp_rr_r400_p512
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 400 --dur 60 --payload 1024 --tag udp_rr_r400_p1024
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 400 --dur 60 --payload 1500 --tag udp_rr_r400_p1500
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 800 --dur 60 --payload 256 --tag udp_rr_r800_p256
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 800 --dur 60 --payload 512 --tag udp_rr_r800_p512
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 800 --dur 60 --payload 1024 --tag udp_rr_r800_p1024
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 800 --dur 60 --payload 1500 --tag udp_rr_r800_p1500
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 100 --dur 60 --payload 256 --tag udp_rr_r100_p256
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 100 --dur 60 --payload 512 --tag udp_rr_r100_p512
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 100 --dur 60 --payload 1024 --tag udp_rr_r100_p1024
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 100 --dur 60 --payload 1500 --tag udp_rr_r100_p1500
```
## msgQ (RTP ?? + DKM ?????)
- RTP ??:
  - rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode rr --dur 65 --tag msgq_srv
- DKM ????? (16? ??):
```
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r200_p256",1,200,60,256)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r200_p512",1,200,60,512)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r200_p1024",1,200,60,1024)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r200_p1500",1,200,60,1500)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r400_p256",1,400,60,256)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r400_p512",1,400,60,512)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r400_p1024",1,400,60,1024)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r400_p1500",1,400,60,1500)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r800_p256",1,800,60,256)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r800_p512",1,800,60,512)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r800_p1024",1,800,60,1024)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r800_p1500",1,800,60,1500)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r100_p256",1,100,60,256)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r100_p512",1,100,60,512)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r100_p1024",1,100,60,1024)
C benchClientRun("msgq","bench_msgq",0,0,"msgq_rr_r100_p1500",1,100,60,1500)
```
## msgQ (DKM ?? + RTP ?????)
- DKM ??:
  - C benchServerStartRR("msgq","bench_msgq",0,0,65)
- RTP ????? (16? ??):
```
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 200 --dur 60 --payload 256 --tag msgq_rr_r200_p256
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 200 --dur 60 --payload 512 --tag msgq_rr_r200_p512
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 200 --dur 60 --payload 1024 --tag msgq_rr_r200_p1024
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 200 --dur 60 --payload 1500 --tag msgq_rr_r200_p1500
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 400 --dur 60 --payload 256 --tag msgq_rr_r400_p256
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 400 --dur 60 --payload 512 --tag msgq_rr_r400_p512
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 400 --dur 60 --payload 1024 --tag msgq_rr_r400_p1024
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 400 --dur 60 --payload 1500 --tag msgq_rr_r400_p1500
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 800 --dur 60 --payload 256 --tag msgq_rr_r800_p256
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 800 --dur 60 --payload 512 --tag msgq_rr_r800_p512
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 800 --dur 60 --payload 1024 --tag msgq_rr_r800_p1024
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 800 --dur 60 --payload 1500 --tag msgq_rr_r800_p1500
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 100 --dur 60 --payload 256 --tag msgq_rr_r100_p256
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 100 --dur 60 --payload 512 --tag msgq_rr_r100_p512
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 100 --dur 60 --payload 1024 --tag msgq_rr_r100_p1024
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode rr --rate 100 --dur 60 --payload 1500 --tag msgq_rr_r100_p1500
```
## shmsem (RTP ?? + DKM ?????)
- RTP ??:
  - rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode rr --dur 65 --tag shmsem_srv
- DKM ????? (16? ??):
```
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r200_p256",1,200,60,256)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r200_p512",1,200,60,512)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r200_p1024",1,200,60,1024)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r200_p1500",1,200,60,1500)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r400_p256",1,400,60,256)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r400_p512",1,400,60,512)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r400_p1024",1,400,60,1024)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r400_p1500",1,400,60,1500)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r800_p256",1,800,60,256)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r800_p512",1,800,60,512)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r800_p1024",1,800,60,1024)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r800_p1500",1,800,60,1500)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r100_p256",1,100,60,256)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r100_p512",1,100,60,512)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r100_p1024",1,100,60,1024)
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_rr_r100_p1500",1,100,60,1500)
```
## shmsem (DKM ?? + RTP ?????)
- DKM ??:
  - C benchServerStartRR("shmsem","bench_shm",0,0,65)
- RTP ????? (16? ??):
```
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 200 --dur 60 --payload 256 --tag shmsem_rr_r200_p256
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 200 --dur 60 --payload 512 --tag shmsem_rr_r200_p512
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 200 --dur 60 --payload 1024 --tag shmsem_rr_r200_p1024
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 200 --dur 60 --payload 1500 --tag shmsem_rr_r200_p1500
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 400 --dur 60 --payload 256 --tag shmsem_rr_r400_p256
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 400 --dur 60 --payload 512 --tag shmsem_rr_r400_p512
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 400 --dur 60 --payload 1024 --tag shmsem_rr_r400_p1024
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 400 --dur 60 --payload 1500 --tag shmsem_rr_r400_p1500
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 800 --dur 60 --payload 256 --tag shmsem_rr_r800_p256
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 800 --dur 60 --payload 512 --tag shmsem_rr_r800_p512
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 800 --dur 60 --payload 1024 --tag shmsem_rr_r800_p1024
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 800 --dur 60 --payload 1500 --tag shmsem_rr_r800_p1500
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 100 --dur 60 --payload 256 --tag shmsem_rr_r100_p256
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 100 --dur 60 --payload 512 --tag shmsem_rr_r100_p512
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 100 --dur 60 --payload 1024 --tag shmsem_rr_r100_p1024
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode rr --rate 100 --dur 60 --payload 1500 --tag shmsem_rr_r100_p1500
```
# ONEWAY ?? ?? (?? IPC, ?? ??)
## UDP (RTP ?? RX + DKM ????? TX)
```
rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 200 --dur 65 --payload 256 --tag udp_ow_srv_r200_p256
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r200_p256",0,200,60,256)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 200 --dur 65 --payload 512 --tag udp_ow_srv_r200_p512
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r200_p512",0,200,60,512)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 200 --dur 65 --payload 1024 --tag udp_ow_srv_r200_p1024
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r200_p1024",0,200,60,1024)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 200 --dur 65 --payload 1500 --tag udp_ow_srv_r200_p1500
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r200_p1500",0,200,60,1500)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 400 --dur 65 --payload 256 --tag udp_ow_srv_r400_p256
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r400_p256",0,400,60,256)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 400 --dur 65 --payload 512 --tag udp_ow_srv_r400_p512
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r400_p512",0,400,60,512)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 400 --dur 65 --payload 1024 --tag udp_ow_srv_r400_p1024
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r400_p1024",0,400,60,1024)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 400 --dur 65 --payload 1500 --tag udp_ow_srv_r400_p1500
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r400_p1500",0,400,60,1500)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 800 --dur 65 --payload 256 --tag udp_ow_srv_r800_p256
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r800_p256",0,800,60,256)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 800 --dur 65 --payload 512 --tag udp_ow_srv_r800_p512
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r800_p512",0,800,60,512)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 800 --dur 65 --payload 1024 --tag udp_ow_srv_r800_p1024
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r800_p1024",0,800,60,1024)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 800 --dur 65 --payload 1500 --tag udp_ow_srv_r800_p1500
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r800_p1500",0,800,60,1500)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 100 --dur 65 --payload 256 --tag udp_ow_srv_r100_p256
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r100_p256",0,100,60,256)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 100 --dur 65 --payload 512 --tag udp_ow_srv_r100_p512
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r100_p512",0,100,60,512)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 100 --dur 65 --payload 1024 --tag udp_ow_srv_r100_p1024
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r100_p1024",0,100,60,1024)

rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode oneway --rate 100 --dur 65 --payload 1500 --tag udp_ow_srv_r100_p1500
C benchClientRun("udp","127.0.0.1",41000,0,"udp_ow_r100_p1500",0,100,60,1500)
```
## UDP (DKM ?? RX + RTP ????? TX)
```
C benchServerStart("udp","0.0.0.0",41000,0,0,200,65,256)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 200 --dur 60 --payload 256 --tag udp_ow_r200_p256

C benchServerStart("udp","0.0.0.0",41000,0,0,200,65,512)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 200 --dur 60 --payload 512 --tag udp_ow_r200_p512

C benchServerStart("udp","0.0.0.0",41000,0,0,200,65,1024)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 200 --dur 60 --payload 1024 --tag udp_ow_r200_p1024

C benchServerStart("udp","0.0.0.0",41000,0,0,200,65,1500)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 200 --dur 60 --payload 1500 --tag udp_ow_r200_p1500

C benchServerStart("udp","0.0.0.0",41000,0,0,400,65,256)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 400 --dur 60 --payload 256 --tag udp_ow_r400_p256

C benchServerStart("udp","0.0.0.0",41000,0,0,400,65,512)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 400 --dur 60 --payload 512 --tag udp_ow_r400_p512

C benchServerStart("udp","0.0.0.0",41000,0,0,400,65,1024)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 400 --dur 60 --payload 1024 --tag udp_ow_r400_p1024

C benchServerStart("udp","0.0.0.0",41000,0,0,400,65,1500)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 400 --dur 60 --payload 1500 --tag udp_ow_r400_p1500

C benchServerStart("udp","0.0.0.0",41000,0,0,800,65,256)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 800 --dur 60 --payload 256 --tag udp_ow_r800_p256

C benchServerStart("udp","0.0.0.0",41000,0,0,800,65,512)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 800 --dur 60 --payload 512 --tag udp_ow_r800_p512

C benchServerStart("udp","0.0.0.0",41000,0,0,800,65,1024)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 800 --dur 60 --payload 1024 --tag udp_ow_r800_p1024

C benchServerStart("udp","0.0.0.0",41000,0,0,800,65,1500)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 800 --dur 60 --payload 1500 --tag udp_ow_r800_p1500

C benchServerStart("udp","0.0.0.0",41000,0,0,100,65,256)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 100 --dur 60 --payload 256 --tag udp_ow_r100_p256

C benchServerStart("udp","0.0.0.0",41000,0,0,100,65,512)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 100 --dur 60 --payload 512 --tag udp_ow_r100_p512

C benchServerStart("udp","0.0.0.0",41000,0,0,100,65,1024)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 100 --dur 60 --payload 1024 --tag udp_ow_r100_p1024

C benchServerStart("udp","0.0.0.0",41000,0,0,100,65,1500)
rtp exec bench_rtp.vxe -- -c --transport udp --dst 127.0.0.1 --port 41000 --mode oneway --rate 100 --dur 60 --payload 1500 --tag udp_ow_r100_p1500
```
## msgQ (RTP ?? RX + DKM ????? TX)
```
rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 200 --dur 65 --payload 256 --tag msgq_ow_srv_r200_p256
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r200_p256",0,200,60,256)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 200 --dur 65 --payload 512 --tag msgq_ow_srv_r200_p512
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r200_p512",0,200,60,512)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 200 --dur 65 --payload 1024 --tag msgq_ow_srv_r200_p1024
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r200_p1024",0,200,60,1024)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 200 --dur 65 --payload 1500 --tag msgq_ow_srv_r200_p1500
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r200_p1500",0,200,60,1500)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 400 --dur 65 --payload 256 --tag msgq_ow_srv_r400_p256
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r400_p256",0,400,60,256)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 400 --dur 65 --payload 512 --tag msgq_ow_srv_r400_p512
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r400_p512",0,400,60,512)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 400 --dur 65 --payload 1024 --tag msgq_ow_srv_r400_p1024
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r400_p1024",0,400,60,1024)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 400 --dur 65 --payload 1500 --tag msgq_ow_srv_r400_p1500
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r400_p1500",0,400,60,1500)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 800 --dur 65 --payload 256 --tag msgq_ow_srv_r800_p256
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r800_p256",0,800,60,256)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 800 --dur 65 --payload 512 --tag msgq_ow_srv_r800_p512
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r800_p512",0,800,60,512)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 800 --dur 65 --payload 1024 --tag msgq_ow_srv_r800_p1024
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r800_p1024",0,800,60,1024)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 800 --dur 65 --payload 1500 --tag msgq_ow_srv_r800_p1500
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r800_p1500",0,800,60,1500)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 100 --dur 65 --payload 256 --tag msgq_ow_srv_r100_p256
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r100_p256",0,100,60,256)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 100 --dur 65 --payload 512 --tag msgq_ow_srv_r100_p512
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r100_p512",0,100,60,512)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 100 --dur 65 --payload 1024 --tag msgq_ow_srv_r100_p1024
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r100_p1024",0,100,60,1024)

rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode oneway --rate 100 --dur 65 --payload 1500 --tag msgq_ow_srv_r100_p1500
C benchClientRun("msgq","bench_msgq",0,0,"msgq_ow_r100_p1500",0,100,60,1500)
```
## msgQ (DKM ?? RX + RTP ????? TX)
```
C benchServerStart("msgq","bench_msgq",0,0,0,200,65,256)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 200 --dur 60 --payload 256 --tag msgq_ow_r200_p256

C benchServerStart("msgq","bench_msgq",0,0,0,200,65,512)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 200 --dur 60 --payload 512 --tag msgq_ow_r200_p512

C benchServerStart("msgq","bench_msgq",0,0,0,200,65,1024)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 200 --dur 60 --payload 1024 --tag msgq_ow_r200_p1024

C benchServerStart("msgq","bench_msgq",0,0,0,200,65,1500)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 200 --dur 60 --payload 1500 --tag msgq_ow_r200_p1500

C benchServerStart("msgq","bench_msgq",0,0,0,400,65,256)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 400 --dur 60 --payload 256 --tag msgq_ow_r400_p256

C benchServerStart("msgq","bench_msgq",0,0,0,400,65,512)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 400 --dur 60 --payload 512 --tag msgq_ow_r400_p512

C benchServerStart("msgq","bench_msgq",0,0,0,400,65,1024)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 400 --dur 60 --payload 1024 --tag msgq_ow_r400_p1024

C benchServerStart("msgq","bench_msgq",0,0,0,400,65,1500)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 400 --dur 60 --payload 1500 --tag msgq_ow_r400_p1500

C benchServerStart("msgq","bench_msgq",0,0,0,800,65,256)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 800 --dur 60 --payload 256 --tag msgq_ow_r800_p256

C benchServerStart("msgq","bench_msgq",0,0,0,800,65,512)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 800 --dur 60 --payload 512 --tag msgq_ow_r800_p512

C benchServerStart("msgq","bench_msgq",0,0,0,800,65,1024)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 800 --dur 60 --payload 1024 --tag msgq_ow_r800_p1024

C benchServerStart("msgq","bench_msgq",0,0,0,800,65,1500)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 800 --dur 60 --payload 1500 --tag msgq_ow_r800_p1500

C benchServerStart("msgq","bench_msgq",0,0,0,100,65,256)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 100 --dur 60 --payload 256 --tag msgq_ow_r100_p256

C benchServerStart("msgq","bench_msgq",0,0,0,100,65,512)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 100 --dur 60 --payload 512 --tag msgq_ow_r100_p512

C benchServerStart("msgq","bench_msgq",0,0,0,100,65,1024)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 100 --dur 60 --payload 1024 --tag msgq_ow_r100_p1024

C benchServerStart("msgq","bench_msgq",0,0,0,100,65,1500)
rtp exec bench_rtp.vxe -- -c --transport msgq --dst bench_msgq --port 0 --mode oneway --rate 100 --dur 60 --payload 1500 --tag msgq_ow_r100_p1500
```
## shmsem (RTP ?? RX + DKM ????? TX)
```
rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 200 --dur 65 --payload 256 --tag shmsem_ow_srv_r200_p256
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r200_p256",0,200,60,256)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 200 --dur 65 --payload 512 --tag shmsem_ow_srv_r200_p512
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r200_p512",0,200,60,512)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 200 --dur 65 --payload 1024 --tag shmsem_ow_srv_r200_p1024
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r200_p1024",0,200,60,1024)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 200 --dur 65 --payload 1500 --tag shmsem_ow_srv_r200_p1500
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r200_p1500",0,200,60,1500)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 400 --dur 65 --payload 256 --tag shmsem_ow_srv_r400_p256
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r400_p256",0,400,60,256)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 400 --dur 65 --payload 512 --tag shmsem_ow_srv_r400_p512
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r400_p512",0,400,60,512)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 400 --dur 65 --payload 1024 --tag shmsem_ow_srv_r400_p1024
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r400_p1024",0,400,60,1024)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 400 --dur 65 --payload 1500 --tag shmsem_ow_srv_r400_p1500
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r400_p1500",0,400,60,1500)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 800 --dur 65 --payload 256 --tag shmsem_ow_srv_r800_p256
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r800_p256",0,800,60,256)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 800 --dur 65 --payload 512 --tag shmsem_ow_srv_r800_p512
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r800_p512",0,800,60,512)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 800 --dur 65 --payload 1024 --tag shmsem_ow_srv_r800_p1024
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r800_p1024",0,800,60,1024)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 800 --dur 65 --payload 1500 --tag shmsem_ow_srv_r800_p1500
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r800_p1500",0,800,60,1500)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 100 --dur 65 --payload 256 --tag shmsem_ow_srv_r100_p256
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r100_p256",0,100,60,256)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 100 --dur 65 --payload 512 --tag shmsem_ow_srv_r100_p512
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r100_p512",0,100,60,512)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 100 --dur 65 --payload 1024 --tag shmsem_ow_srv_r100_p1024
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r100_p1024",0,100,60,1024)

rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode oneway --rate 100 --dur 65 --payload 1500 --tag shmsem_ow_srv_r100_p1500
C benchClientRun("shmsem","bench_shm",0,0,"shmsem_ow_r100_p1500",0,100,60,1500)
```
## shmsem (DKM ?? RX + RTP ????? TX)
```
C benchServerStart("shmsem","bench_shm",0,0,0,200,65,256)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 200 --dur 60 --payload 256 --tag shmsem_ow_r200_p256

C benchServerStart("shmsem","bench_shm",0,0,0,200,65,512)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 200 --dur 60 --payload 512 --tag shmsem_ow_r200_p512

C benchServerStart("shmsem","bench_shm",0,0,0,200,65,1024)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 200 --dur 60 --payload 1024 --tag shmsem_ow_r200_p1024

C benchServerStart("shmsem","bench_shm",0,0,0,200,65,1500)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 200 --dur 60 --payload 1500 --tag shmsem_ow_r200_p1500

C benchServerStart("shmsem","bench_shm",0,0,0,400,65,256)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 400 --dur 60 --payload 256 --tag shmsem_ow_r400_p256

C benchServerStart("shmsem","bench_shm",0,0,0,400,65,512)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 400 --dur 60 --payload 512 --tag shmsem_ow_r400_p512

C benchServerStart("shmsem","bench_shm",0,0,0,400,65,1024)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 400 --dur 60 --payload 1024 --tag shmsem_ow_r400_p1024

C benchServerStart("shmsem","bench_shm",0,0,0,400,65,1500)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 400 --dur 60 --payload 1500 --tag shmsem_ow_r400_p1500

C benchServerStart("shmsem","bench_shm",0,0,0,800,65,256)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 800 --dur 60 --payload 256 --tag shmsem_ow_r800_p256

C benchServerStart("shmsem","bench_shm",0,0,0,800,65,512)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 800 --dur 60 --payload 512 --tag shmsem_ow_r800_p512

C benchServerStart("shmsem","bench_shm",0,0,0,800,65,1024)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 800 --dur 60 --payload 1024 --tag shmsem_ow_r800_p1024

C benchServerStart("shmsem","bench_shm",0,0,0,800,65,1500)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 800 --dur 60 --payload 1500 --tag shmsem_ow_r800_p1500

C benchServerStart("shmsem","bench_shm",0,0,0,100,65,256)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 100 --dur 60 --payload 256 --tag shmsem_ow_r100_p256

C benchServerStart("shmsem","bench_shm",0,0,0,100,65,512)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 100 --dur 60 --payload 512 --tag shmsem_ow_r100_p512

C benchServerStart("shmsem","bench_shm",0,0,0,100,65,1024)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 100 --dur 60 --payload 1024 --tag shmsem_ow_r100_p1024

C benchServerStart("shmsem","bench_shm",0,0,0,100,65,1500)
rtp exec bench_rtp.vxe -- -c --transport shmsem --dst bench_shm --port 0 --mode oneway --rate 100 --dur 60 --payload 1500 --tag shmsem_ow_r100_p1500
```