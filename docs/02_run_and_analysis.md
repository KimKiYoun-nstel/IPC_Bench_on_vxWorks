# 실행 방법 및 결과 분석

## 빌드
- Workbench Makefile Project에 루트 저장소 추가
- `scripts/vx_set_env.cmd`으로 VxWorks 환경 변수 로드
- VSCode에서 `.vscode/tasks.json`의 Vx Make 태스크 실행
- 터미널에서 `D:/WindRiver/wrenv.exe cmd.exe /d /s /c "call scripts/vx_set_env.cmd && make [all|dkm|rtp] [MODE=debug]"` 실행
- `config.mk`의 WIND_BASE/VSB_DIR/LLVM_ROOT/MODE 설정 확인

## 실행 개요
1) 서버(RX) 먼저 실행
2) 클라이언트(TX) 실행
3) 콘솔에서 p50/p90/p99/max, loss, ooo, jitter 확인

## 진행 로그
- RR 클라이언트: sent/s, recv/s, loss/s, ooo/s, tx_fail/s
- RR 서버: recv/s, rsp/s
- ONEWAY RX: recv/s, loss/s, ooo/s
- ONEWAY TX: sent/s, tx_fail/s

## 동시 실행 요령
- RTP는 셸을 점유하므로 백그라운드나 별도 콘솔에서 DKM 실행
- `rtp exec -d bench_rtp.vxe ...` 또는 `rtp exec ... &`를 활용
- 서버 --dur 65, 클라이언트 --dur 60

## 공통 파라미터
- transport: udp / msgq / shmsem
- mode: rr(요청/응답) / oneway(단방향)
- rate: 200 / 400 / 800 / 100 Hz
- payload: 256 / 512 / 1024 / 1500 byte
- duration: 서버 65초, 클라이언트 60초
- tag: 출력 라벨
- name: msgq/shmsem은 동일 base (bench_msgq, bench_shm)
- UDP 포트 41000, msgq/shmsem은 포트 0
- 서버는 클라이언트 rate/payload 기준으로 처리

## RTP 실행 방식
- 서버: `rtp exec bench_rtp.vxe -- -s --transport <udp|msgq|shmsem> --bind <ip|name> --port <port> --mode <rr|oneway> --dur <sec> --tag <label>`
- 클라이언트: `rtp exec bench_rtp.vxe -- -c --transport <udp|msgq|shmsem> --dst <ip|name> --port <port> --mode <rr|oneway> --rate <Hz> --dur <sec> --payload <bytes> --tag <label>`

## DKM 실행 방식
- RR 서버: `C benchServerStartRR("transport","bind",port,0,65)`
- 일반 서버: `C benchServerStart("transport","bind",port,0,0,rate,65,payload)`
- 클라이언트: `C benchClientRun("transport","dst",port,0,"tag",mode,rate,60,payload)`

# RR 실행 예시
## UDP (RTP 서버 + DKM 클라이언트)
- RTP 서버:
  - rtp exec bench_rtp.vxe -- -s --transport udp --bind 0.0.0.0 --port 41000 --mode rr --dur 65 --tag udp_srv
- DKM 클라이언트 (16개 조합):
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
## UDP (DKM 서버 + RTP 클라이언트)
- DKM 서버:
  - C benchServerStartRR("udp","0.0.0.0",41000,0,65)
- RTP 클라이언트 (16개 조합):
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
## MSGQ (RTP 서버 + DKM 클라이언트)
- RTP 서버:
  - rtp exec bench_rtp.vxe -- -s --transport msgq --bind bench_msgq --port 0 --mode rr --dur 65 --tag msgq_srv
- DKM 클라이언트 (16개 조합):
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
## MSGQ (DKM 서버 + RTP 클라이언트)
- DKM 서버:
  - C benchServerStartRR("msgq","bench_msgq",0,0,65)
- RTP 클라이언트 (16개 조합):
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
## SHMSEM (RTP 서버 + DKM 클라이언트)
- RTP 서버:
  - rtp exec bench_rtp.vxe -- -s --transport shmsem --bind bench_shm --port 0 --mode rr --dur 65 --tag shmsem_srv
- DKM 클라이언트 (16개 조합):
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
## SHMSEM (DKM 서버 + RTP 클라이언트)
- DKM 서버:
  - C benchServerStartRR("shmsem","bench_shm",0,0,65)
- RTP 클라이언트 (16개 조합):
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
# ONEWAY 실행 예시
## UDP (RTP 서버 RX + DKM 클라이언트 TX)
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
## UDP (DKM 서버 RX + RTP 클라이언트 TX)
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
## MSGQ (RTP 서버 RX + DKM 클라이언트 TX)
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
## MSGQ (DKM 서버 RX + RTP 클라이언트 TX)
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
## SHMSEM (RTP 서버 RX + DKM 클라이언트 TX)
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
## SHMSEM (DKM 서버 RX + RTP 클라이언트 TX)
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
## 권장 매트릭스
- payload: 256/512/1024/1500 bytes
- rate: 200/400/800/100 Hz
- duration: 서버 65s / 클라이언트 60s
- 동일 조건을 최소 3회 반복하고 중앙값/최댓값 비교

## 해석 팁
- p99/max 우선, loss/ooo/jitter 확인
- RR은 `[BENCH][RR]`에서 min/p50/p90/p99/max, loss/ooo 확인
- ONEWAY은 `[BENCH][ONEWAY][RX]`와 `[BENCH][ONEWAY][TX]` 확인
- loopback vs board_ip 결과 차이는 NIC/IRQ/드라이버 영향
- jitter 상승 구간은 ISR/태스크 스케줄을 WindView 등으로 분석

## 해상도 확인
- `-> sysClkRateGet()` : tick 해상도 (예: 1000 = 1ms)
- `-> sysTimestampFreq()` : 0이면 tick 기반, 0이 아니면 고해상도
- DKM은 고해상도 사용, RTP는 clock_gettime 해상도 한계