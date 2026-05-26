# 그래피파이 사용 가이드 (graphify_guide.md)

`graphify`는 프로젝트 내 모든 파일(소스 코드, 문서 등)의 연관 관계를 분석하여 한눈에 볼 수 있는 **대화형 HTML 지식 그래프**와 **상세 리포트(`GRAPH_REPORT.md`)**를 자동으로 작성해주는 지식 매핑 스킬입니다.

---

## 1. 주요 기능
1. **의존성 시각화**: 소스 코드 간의 import, 호출 관계뿐만 아니라 문서 및 주석에 등장하는 개념적 의존성을 추출합니다.
2. **커뮤니티 감지 (Community Detection)**: 서로 연관성이 높은 파일들을 그룹(커뮤니티)으로 묶어 시스템 구조를 쉽게 이해할 수 있게 돕습니다.
3. **Obsidian 연동**: 시각화된 구조를 옵시디언(Obsidian) 노트의 Canvas 레이아웃으로 추출할 수 있습니다.

## 2. 사용 명령어 요약

에이전트 환경 또는 CMD 터미널에서 다음과 같은 명령어로 그래피파이를 활성화하여 분석 리포트를 얻을 수 있습니다.

```bash
# 1. 현재 디렉토리 기준 전체 프로젝트 지식 그래프 및 리포트 생성
/graphify

# 2. 특정 심층 모드(Deep Mode) 분석 실행 (잠재적 결합 및 간접 의존성 추적 강도 업)
/graphify . --mode deep

# 3. 디렉토리 변경 사항만 빠르게 갱신 (증분 빌드)
/graphify . --update

# 4. 시각화 HTML 그래프 파일만 출력
/graphify . --html
```

## 3. 출력 결과물 경로
분석이 정상 완료되면 프로젝트 내 `graphify-out/` 디렉토리에 다음 파일들이 생성됩니다:
- `graphify-out/graph.html`: 브라우저로 더블 클릭하여 바로 열 수 있는 인터랙티브 네트워크 노드 맵.
- `graphify-out/GRAPH_REPORT.md`: 발견된 주요 허브 노드(God Nodes) 및 특이 연관 관계 분석 텍스트 리포트.
- `graphify-out/graph.json`: 원본 데이터 파일.
- `graphify-out/cost.json`: 토큰 및 빌드 정보 기록.
