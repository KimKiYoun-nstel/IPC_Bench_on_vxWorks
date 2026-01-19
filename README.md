# VxWorks DKM↔RTP IPC Benchmark Template (T2080 / VxWorks 23.03)

동일 보드/동일 VxWorks 환경에서 **DKM(.out) ↔ RTP(.vxe)** 간 IPC 후보들의 **지연/지터(p50/p90/p99/max)** 를 동일 조건으로 비교하기 위한 템플릿입니다.

## 구성
- `common/` : 공용 벤치 모듈(프로토콜/통계/러너/transport 플러그인)
- `dkm/` : DKM 모듈 엔트리(쉘에서 호출할 함수 제공)
- `rtp/` : RTP 실행파일(옵션으로 server/client 선택)
- `docs/` : 설계/실행&분석/절차서
- `workbench/` : Workbench Makefile Project 추가 가이드

## 빌드
- VxWorks env: set `VSB_DIR` (or `WIND_CC_SYSROOT`) and `WIND_BASE` (see `BuildRef/forDKM/vx_set_env.cmd`)
- `make all` (or `make dkm`, `make rtp`), use `make MODE=debug` for debug builds
- 루트에서 `make all` 또는 Workbench에서 Build Target `all/dkm/rtp` 실행
- 툴체인/링크 플래그는 `config.mk`에서 환경에 맞게 조정

## 실행(요약)
- RTP 서버: `bench_rtp -s --transport udp --bind 0.0.0.0 --port 41000 --mode rr`
- DKM 클라(예): `C benchClientRun("udp","127.0.0.1",41000,0,"udp_loop",1,200,30,256)`

자세한 내용은 `docs/` 참고.
