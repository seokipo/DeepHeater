#include "display.h"
#include "main.h"

// FND 폰트 데이터 정의
const unsigned char FND_Font[16] = {
    0xC0, // 0
    0xF9, // 1
    0xA4, // 2
    0xB0, // 3
    0x99, // 4
    0x92, // 5
    0x82, // 6
    0xF8, // 7
    0x80, // 8
    0x90, // 9
    0x88, // A
    0x83, // b
    0xC6, // C
    0xA1, // d
    0x86, // E
    0x8E  // F
};

// 디스플레이 점멸 변수 정의
unsigned int blink_ticks = 0;
unsigned char blink_state = 1;

/**
 * @brief 디스플레이 3단계 동적 스캔 및 점멸 제어 프로세스
 * @param display_minute FND에 표시할 현재 분(minute) 또는 설정 모드의 PWM 설정값
 */
void Display_Process(unsigned char display_minute) {
  unsigned char level_led_data = 0xFF;

  // --- 대기 상태(IDLE) 0.5초 점멸 제어 ---
  if (is_running) {
    // 동작 중: 상시 점등
    blink_state = 1;
    blink_ticks = 0;
  } else {
    // 대기 중: 루프 카운터 기반 0.5초(약 156회 루프) 주기 점멸
    blink_ticks++;
    if (blink_ticks >= 156) {
      blink_ticks = 0;
      blink_state = !blink_state;
    }
  }

  // --- 1단계: 일의 자리 출력 (Digit 1 - 하드웨어 우측 디스플레이) ---
  LATB = 0xFF; // 고스트 방지 (모든 세그먼트 소등)
  LATA2 = 1;   // Digit 2 OFF (좌측 디지트 차단)
  LATA3 = 1;   // LED DRV OFF (개별 LED 차단)
  __delay_us(50); // 하드웨어 PNP TR Turn-off 스위칭 지연 시간 확보 (Dead Time)
  
  if (setting_mode || blink_state) {
    LATB = FND_Font[display_minute % 10]; // 일의 자리 데이터 선출력
    LATA1 = 0;                      // Digit 1 ON (Active LOW)
  }
  __delay_ms(1); // 1ms 지연
  LATA1 = 1;     // 소등

  // --- 2단계: 십의 자리 출력 (Digit 2 - 하드웨어 좌측 디스플레이) ---
  LATB = 0xFF; // 고스트 방지 (모든 세그먼트 소등)
  LATA1 = 1;   // Digit 1 OFF (우측 디지트 차단)
  LATA3 = 1;   // LED DRV OFF (개별 LED 차단)
  __delay_us(50); // 하드웨어 PNP TR Turn-off 스위칭 지연 시간 확보 (Dead Time)
  
  if (setting_mode || blink_state) {
    LATB = FND_Font[display_minute / 10]; // 십의 자리 데이터 선출력
    LATA2 = 0;                      // Digit 2 ON (Active LOW)
  }
  __delay_ms(1); // 1ms 지연
  LATA2 = 1;     // 소등

  // --- 3단계: 개별 LED 출력 (LED DRV - Level Lo 등 상태 표시) ---
  LATB = 0xFF; // 고스트 방지 (모든 세그먼트 소등)
  LATA1 = 1;   // Digit 1 OFF (우측 디지트 차단)
  LATA2 = 1;   // Digit 2 OFF (좌측 디지트 차단)
  __delay_us(50); // 하드웨어 PNP TR Turn-off 스위칭 지연 시간 확보 (Dead Time)
  
  if (!setting_mode && is_running) {
    // 작동 중일 때만 선택된 레벨 LED 및 LV_Lo/Hi 켜기
    unsigned char level_mask = 0;
    // 1~6 레벨에 맞춰 LED1~6을 채워나가는 바 그래프 마스크 비트 켜기
    for (unsigned char i = 0; i < current_level; i++) {
      level_mask |= (1 << i);
    }
    // Lo/Hi 모드에 따라 LED7(Lo) / LED8(Hi)을 교차로 켬
    if (is_hi_mode) {
      level_mask |= (1 << 7); // LED8 (Hi) ON
    } else {
      level_mask |= (1 << 6); // LED7 (Lo) ON
    }
    level_led_data = ~level_mask; // Active LOW이므로 전체 반전
    LATB = level_led_data;        // 개별 LED 캐소드 출력
    LATA3 = 0;                    // LED DRV ON (Active LOW)
  } else if (!setting_mode) {
    // 대기 상태(is_running == 0): blink_state에 연동하여 Lo/Hi LED 0.5초 점멸
    if (blink_state) {
      unsigned char level_mask = 0;
      if (is_hi_mode) {
        level_mask |= (1 << 7); // LED8 (Hi) ON
      } else {
        level_mask |= (1 << 6); // LED7 (Lo) ON
      }
      level_led_data = ~level_mask; // Active LOW이므로 전체 반전
      LATB = level_led_data;
      LATA3 = 0; // LED DRV ON (Active LOW)
    }
  }
  __delay_ms(1); // 1ms 지연
  LATA3 = 1;     // LED DRV OFF
}
