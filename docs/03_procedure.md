# IPC 벤치마크 절차서(템플릿)

## 목적
- IPC 후보 비교 결과로 구조 변경 요청을 데이터로 설득/수락

## 방식
- 동일 하네스에서 transport만 교체
- 시나리오: RR(메인) + ONEWAY(보조)

## 수행 절차
1) 환경 준비(동일 이미지/동일 부하 조건)
2) 빌드(`all`)
3) 실행(RTP 서버/클라, DKM 서버/클라 교차)
4) 결과 수집(콘솔/CSV)
5) 반복(조건당 3회 이상)

## 결과 분석(템플릿)
- 우선순위: p99/max/loss
- 선택 기준: 개선폭 vs 복잡도/리스크/유지보수 비용

## 수도코드
### RR client
- send REQ(seq,t0)
- wait RSP(seq)
- RTT=t1-t0 기록

### RR server
- recv REQ
- send RSP echo
