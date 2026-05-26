# Graph Report - D:\Work\Pic\Model\DeepHeater  (2026-05-21)

## Corpus Check
- Corpus is ~21,781 words - fits in a single context window. You may not need a graph.

## Summary
- 7 nodes · 10 edges · 1 communities detected
- Extraction: 100% EXTRACTED · 0% INFERRED · 0% AMBIGUOUS
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]

## God Nodes (most connected - your core abstractions)
1. `main()` - 5 edges
2. `System_Init()` - 2 edges
3. `Buzzer_PWM_Init()` - 2 edges
4. `UART_Init()` - 2 edges
5. `Key_Scan()` - 2 edges

## Surprising Connections (you probably didn't know these)
- None detected - all connections are within the same source files.

## Communities

### Community 0 - "Community 0"
Cohesion: 0.48
Nodes (5): Buzzer_PWM_Init(), Key_Scan(), main(), System_Init(), UART_Init()

## Suggested Questions
_Not enough signal to generate questions. This usually means the corpus has no AMBIGUOUS edges, no bridge nodes, no INFERRED relationships, and all communities are tightly cohesive. Add more files or run with --mode deep to extract richer edges._