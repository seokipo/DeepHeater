# Graph Report - d:\\Work\\Pic\\Model\\DeepHeater  (2026-05-26)

## Corpus Check
- 30 files · ~231,301 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 36 nodes · 41 edges · 5 communities detected
- Extraction: 61% EXTRACTED · 39% INFERRED · 0% AMBIGUOUS · INFERRED: 16 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Main Control & PWM Actuators|Main Control & PWM Actuators]]
- [[_COMMUNITY_Interrupts & IR Decoder Module|Interrupts & IR Decoder Module]]
- [[_COMMUNITY_System Initialization & UART|System Initialization & UART]]
- [[_COMMUNITY_Key Matrix Scanner Module|Key Matrix Scanner Module]]
- [[_COMMUNITY_Dynamic Display Multiplexer|Dynamic Display Multiplexer]]

## God Nodes (most connected - your core abstractions)
1. `main()` - 13 edges
2. `Update_Heater_PWM()` - 6 edges
3. `Key_Process()` - 4 edges
4. `Heater_Feedback_Process()` - 3 edges
5. `IR_Process_Command()` - 3 edges
6. `System_Init()` - 3 edges
7. `ADC_Read()` - 3 edges
8. `Display_Process()` - 2 edges
9. `Key_Scan()` - 2 edges
10. `__interrupt()` - 2 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `Display_Process()`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → D:\Work\Pic\Model\DeepHeater\display.c
- `main()` --calls--> `UART_Init()`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → D:\Work\Pic\Model\DeepHeater\system.c
- `main()` --calls--> `IR_Init()`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → D:\Work\Pic\Model\DeepHeater\remote.c
- `Key_Process()` --calls--> `Update_Heater_PWM()`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\key.c → D:\Work\Pic\Model\DeepHeater\pwm_control.c
- `main()` --calls--> `Key_Process()`  [INFERRED]
  D:\Work\Pic\Model\DeepHeater\main.c → D:\Work\Pic\Model\DeepHeater\key.c

## Communities

### Community 0 - "Main Control & PWM Actuators"
Cohesion: 0.46
Nodes (7): main(), Buzzer_PWM_Init(), Heater_PWM_Init(), Update_Heater_PWM(), Buzzer_Process(), Heater_Feedback_Process(), ADC_Read()

### Community 1 - "Interrupts & IR Decoder Module"
Cohesion: 0.33
Nodes (4): __interrupt(), IR_Init(), IR_Decode_Process(), IR_Process_Command()

### Community 2 - "System Initialization & UART"
Cohesion: 0.5
Nodes (3): System_Init(), ADC_Init(), UART_Init()

### Community 3 - "Key Matrix Scanner Module"
Cohesion: 1.0
Nodes (2): Key_Scan(), Key_Process()

### Community 6 - "Dynamic Display Multiplexer"
Cohesion: 1.0
Nodes (1): Display_Process()

## Knowledge Gaps
- **Thin community `Key Matrix Scanner Module`** (3 nodes): `key.c`, `Key_Scan()`, `Key_Process()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Dynamic Display Multiplexer`** (2 nodes): `display.c`, `Display_Process()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `Main Control & PWM Actuators` to `Interrupts & IR Decoder Module`, `System Initialization & UART`, `Key Matrix Scanner Module`, `Dynamic Display Multiplexer`?**
  _High betweenness centrality (0.309) - this node is a cross-community bridge._
- **Why does `Key_Process()` connect `Key Matrix Scanner Module` to `Main Control & PWM Actuators`?**
  _High betweenness centrality (0.071) - this node is a cross-community bridge._
- **Why does `System_Init()` connect `System Initialization & UART` to `Main Control & PWM Actuators`?**
  _High betweenness centrality (0.045) - this node is a cross-community bridge._
- **Are the 12 inferred relationships involving `main()` (e.g. with `System_Init()` and `Heater_PWM_Init()`) actually correct?**
  _`main()` has 12 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `Update_Heater_PWM()` (e.g. with `Key_Process()` and `main()`) actually correct?**
  _`Update_Heater_PWM()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `Key_Process()` (e.g. with `Update_Heater_PWM()` and `main()`) actually correct?**
  _`Key_Process()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `IR_Process_Command()` (e.g. with `main()` and `Update_Heater_PWM()`) actually correct?**
  _`IR_Process_Command()` has 2 INFERRED edges - model-reasoned connections that need verification._