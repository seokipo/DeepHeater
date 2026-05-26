# Graph Report - .  (2026-05-22)

## Corpus Check
- Corpus is ~32,216 words - fits in a single context window. You may not need a graph.

## Summary
- 22 nodes · 32 edges · 5 communities detected
- Extraction: 59% EXTRACTED · 41% INFERRED · 0% AMBIGUOUS · INFERRED: 13 edges (avg confidence: 0.9)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_코어 펌웨어 및 프로젝트 문서 (Core Firmware & Docs)|코어 펌웨어 및 프로젝트 문서 (Core Firmware & Docs)]]
- [[_COMMUNITY_MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)|MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)]]
- [[_COMMUNITY_그래피파이 시스템 분석 가이드 (Graphify Analysis Guide)|그래피파이 시스템 분석 가이드 (Graphify Analysis Guide)]]
- [[_COMMUNITY_FND 디스플레이 회로 및 스펙 (FND Display Hardware & Spec)|FND 디스플레이 회로 및 스펙 (FND Display Hardware & Spec)]]
- [[_COMMUNITY_MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)|MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)]]

## God Nodes (most connected - your core abstractions)
1. `main()` - 7 edges
2. `System_Init()` - 4 edges
3. `Buzzer_PWM_Init()` - 4 edges
4. `IR_Init()` - 4 edges
5. `__interrupt()` - 4 edges
6. `MCU 데이터시트 (pic16f18855.pdf)` - 4 edges
7. `UART_Init()` - 3 edges
8. `Key_Scan()` - 3 edges
9. `개발 일지 (DEVELOPMENT_LOG.md)` - 2 edges
10. `분석 보고서 (GRAPH_REPORT.md)` - 2 edges

## Surprising Connections (you probably didn't know these)
- `main()` --conceptually_related_to--> `디스플레이 회로도 (Display_part.png)`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → Docs/Display_part.png
- `System_Init()` --conceptually_related_to--> `MCU 데이터시트 (pic16f18855.pdf)`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → Docs/pic16f18855.pdf
- `System_Init()` --conceptually_related_to--> `MCU 회로도 (회로도-mcu.png)`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → Docs/회로도-mcu.png
- `Buzzer_PWM_Init()` --conceptually_related_to--> `MCU 데이터시트 (pic16f18855.pdf)`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → Docs/pic16f18855.pdf
- `Buzzer_PWM_Init()` --conceptually_related_to--> `부저 및 IR 회로도 (ir_buzz_pwm.png)`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → Docs/ir_buzz_pwm.png

## Communities

### Community 0 - "코어 펌웨어 및 프로젝트 문서 (Core Firmware & Docs)"
Cohesion: 0.32
Nodes (6): 의존성 지도 (dependency_map.md), 개발 일지 (DEVELOPMENT_LOG.md), 카파시 가이드라인 (karpathy_guidelines.md), __interrupt(), 프로그램 용어 사전 (PROGRAMMING_TERMS.md), 리모컨 데이터 포맷 (리모콘데이타.png)

### Community 1 - "MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)"
Cohesion: 0.39
Nodes (8): 부저 및 IR 회로도 (ir_buzz_pwm.png), Buzzer_PWM_Init(), IR_Init(), main(), System_Init(), UART_Init(), MCU 데이터시트 (pic16f18855.pdf), MCU 회로도 (회로도-mcu.png)

### Community 2 - "그래피파이 시스템 분석 가이드 (Graphify Analysis Guide)"
Cohesion: 1.0
Nodes (2): 분석 보고서 (GRAPH_REPORT.md), 그래피파이 가이드 (graphify_guide.md)

### Community 3 - "FND 디스플레이 회로 및 스펙 (FND Display Hardware & Spec)"
Cohesion: 1.0
Nodes (2): FND 스펙 (CLD-5622BUR-11.pdf), 디스플레이 회로도 (Display_part.png)

### Community 4 - "MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)"
Cohesion: 1.0
Nodes (2): 키 및 LED 회로도 (Key&led.png), Key_Scan()

## Knowledge Gaps
- **7 isolated node(s):** `의존성 지도 (dependency_map.md)`, `그래피파이 가이드 (graphify_guide.md)`, `카파시 가이드라인 (karpathy_guidelines.md)`, `FND 스펙 (CLD-5622BUR-11.pdf)`, `키 및 LED 회로도 (Key&led.png)` (+2 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `그래피파이 시스템 분석 가이드 (Graphify Analysis Guide)`** (2 nodes): `분석 보고서 (GRAPH_REPORT.md)`, `그래피파이 가이드 (graphify_guide.md)`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `FND 디스플레이 회로 및 스펙 (FND Display Hardware & Spec)`** (2 nodes): `FND 스펙 (CLD-5622BUR-11.pdf)`, `디스플레이 회로도 (Display_part.png)`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)`** (2 nodes): `키 및 LED 회로도 (Key&led.png)`, `Key_Scan()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)` to `코어 펌웨어 및 프로젝트 문서 (Core Firmware & Docs)`, `FND 디스플레이 회로 및 스펙 (FND Display Hardware & Spec)`, `MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)`?**
  _High betweenness centrality (0.233) - this node is a cross-community bridge._
- **Why does `System_Init()` connect `MCU 하드웨어 초기화 및 메인 구동 (MCU Hardware Init & Main)` to `코어 펌웨어 및 프로젝트 문서 (Core Firmware & Docs)`?**
  _High betweenness centrality (0.113) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `System_Init()` (e.g. with `MCU 데이터시트 (pic16f18855.pdf)` and `MCU 회로도 (회로도-mcu.png)`) actually correct?**
  _`System_Init()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `Buzzer_PWM_Init()` (e.g. with `MCU 데이터시트 (pic16f18855.pdf)` and `부저 및 IR 회로도 (ir_buzz_pwm.png)`) actually correct?**
  _`Buzzer_PWM_Init()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `IR_Init()` (e.g. with `MCU 데이터시트 (pic16f18855.pdf)` and `부저 및 IR 회로도 (ir_buzz_pwm.png)`) actually correct?**
  _`IR_Init()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 3 inferred relationships involving `__interrupt()` (e.g. with `리모컨 데이터 포맷 (리모콘데이타.png)` and `프로그램 용어 사전 (PROGRAMMING_TERMS.md)`) actually correct?**
  _`__interrupt()` has 3 INFERRED edges - model-reasoned connections that need verification._
- **What connects `의존성 지도 (dependency_map.md)`, `그래피파이 가이드 (graphify_guide.md)`, `카파시 가이드라인 (karpathy_guidelines.md)` to the rest of the system?**
  _7 weakly-connected nodes found - possible documentation gaps or missing edges._