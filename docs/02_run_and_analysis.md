# 실행 방법 및 결과 분석

## 빌드
- Workbench Makefile Project로 이 루트 폴더를 추가
- Build Target: `all` 또는 `dkm`, `rtp`
- 툴체인/링크 옵션은 `config.mk`에서 환경에 맞게 조정

## 실행 예시
### RTP 서버
- `bench_rtp -s --transport udp --bind 0.0.0.0 --port 41000 --mode rr`

### DKM 클라이언트
- `.out` 로드 후:
- loopback:
  - `C benchClientRun("udp","127.0.0.1",41000,0,"udp_loop",1,200,30,256)`
- board ip:
  - `C benchClientRun("udp","<board_ip>",41000,0,"udp_boardip",1,200,30,256)`

## 권장 매트릭스
- payload: 0/64/256/900 bytes
- rate: 200/500/1000 Hz
- duration: 30s 이상(가능하면 60s)

## 해석 팁
- 평균보다 **p99/max**를 우선
- loopback 대비 board_ip에서 p99/max 악화가 크면, NIC/드라이버/IRQ 영향 가능성
- 개선 폭이 작으면 SHM 복잡도 대비 이득이 작을 수 있음
