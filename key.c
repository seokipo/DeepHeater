#include <stdio.h>
#include "key.h"
#include "main.h"
#include "system.h"
#include "pwm_control.h"

// 디바운스 필터 및 상태 변수 정의
unsigned char key_db_count[6] = {0, 0, 0, 0, 0, 0};
unsigned int key_hold_count[6] = {0, 0, 0, 0, 0, 0};
unsigned int key_repeat_count[6] = {0, 0, 0, 0, 0, 0};
unsigned char key_active[6] = {0, 0, 0, 0, 0, 0};

/**
 * @brief 키 매트릭스 6개 스위치 스캔 함수
 * - FND 세그먼트 소등 상태(데드 타임)에서 수행하여 디스플레이 왜곡 방지
 * - RB0 ~ RB5 핀을 순차적으로 LOW로 구동한 뒤 RC7(KEY IN) 값을 읽어 눌림 확인
 * @return 눌린 키 번호 (1: START, 2: LEVEL, 3: TIME_UP, 4: TIME_DN, 5: LEVEL_UP, 6: LEVEL_DN, 0: 없음)
 */
unsigned char Key_Scan(void) {
  unsigned char pressed_key = 0;

  // 1. FND 및 개별 LED 구동 공통 애노드 전면 차단하여 간섭 소거
  LATB = 0xFF;
  LATA1 = 1;      // DIGIT 1 OFF
  LATA2 = 1;      // DIGIT 2 OFF
  LATA3 = 1;      // LED DRV OFF
  __delay_us(50); // 하드웨어 PNP TR의 Turn-off 완수를 위한 데드 타임

  // 2. RC7을 출력 LOW로 설정하여 스캔 전압 인가
  TRISCbits.TRISC7 = 0; // 출력 모드
  LATCbits.LATC7 = 0;   // LOW 출력

  // 3. PORTB의 RB0~RB5를 일시적으로 입력 모드로 전환 (내부 풀업은 WPUB=0x00으로 비활성 유지)
  TRISB = 0x3F;
  __delay_us(5); // 신호 안정화

  // 4. PORTB 핀 상태 판독 (Active LOW)
  unsigned char key_state = PORTB & 0x3F;
  if ((key_state & 0x01) == 0) {
    pressed_key = 1; // SW1 (START/STOP)
  } else if ((key_state & 0x02) == 0) {
    pressed_key = 2; // SW2 (LEVEL)
  } else if ((key_state & 0x04) == 0) {
    pressed_key = 3; // SW3 (TIME_UP)
  } else if ((key_state & 0x08) == 0) {
    pressed_key = 4; // SW4 (TIME_DN)
  } else if ((key_state & 0x10) == 0) {
    pressed_key = 5; // SW5 (LEVEL_UP)
  } else if ((key_state & 0x20) == 0) {
    pressed_key = 6; // SW6 (LEVEL_DN)
  }

  // 5. I/O 포트 상태 원복
  TRISB = 0x00;         // PORTB 전체 출력 모드로 복원
  LATCbits.LATC7 = 1;   // RC7 HIGH
  TRISCbits.TRISC7 = 1; // RC7을 입력 모드로 환원 (안전 보호)

  return pressed_key;
}

/**
 * @brief 키 입력 디바운스, 릴리즈 감쇄, 연속 및 단발 기능 동작 처리
 */
void Key_Process(unsigned char *p_minute, char *p_second, unsigned int *p_tick_ms) {
  unsigned char raw_key = 0;

  // CLI 테스트 모드 동작 중(디버그 토글 활성화, 부저 핀 강제 제어 등)이 아닐 때만 키 스캔 수행
  if (debug_toggle_mode == 0 && LATC0 == 0) {
    raw_key = Key_Scan();
  } else {
    raw_key = 0;
  }

  for (unsigned char k = 0; k < 6; k++) {
    if (raw_key == (k + 1)) {
      // 키가 물리적으로 입력되어 있는 경우
      if (key_db_count[k] < 10) {
        key_db_count[k]++;
        if (key_db_count[k] == 10 && key_active[k] == 0) {
          key_active[k] = 1;
          key_hold_count[k] = 0;
          key_repeat_count[k] = 0;

          // --- 설정 모드(setting_mode == 1) 시 물리 키 처리 전면 무시 (리모컨 단독 조작 보장) ---
          if (setting_mode) {
            continue; // 설정 모드일 때는 아래 일반 모드 로직 수행 안 하고 물리 키 입력을 무시함
          }

          // [최초 1회 실행] 동작 분기: 실제 기능이 작동될 때만 부저 울림
          if (k == 0) { // SW1 (START / STOP) - 항상 부저 울림
            // 삼중음(mode=1, 2) 진행 중에는 재트리거 차단
            if (!(buzzer_timer > 0 && (buzzer_mode == 1 || buzzer_mode == 2))) {
              CCP1CON = 0x8F;
              T2CON = 0xE0;
              if (is_running == 0) {
                // 스타트 시: 상승 삼중음 (낮은음 -> 중간음 -> 높은음) 시작
                buzzer_mode = 1;
                buzzer_stage = 1;
                buzzer_timer = 50; // 각 음당 약 150ms
                buzzer_init_value = 50;
                PR2 = 73;    // 낮은음 (약 1.7kHz)
                CCPR1L = 37; // 50% 듀티
                is_running = 1;
                current_level = 1; // Level 1 자동 초기 설정
                *p_second = 0;
                *p_tick_ms = 0;
                Update_Heater_PWM();
              } else {
                // 스톱 시: 하강 삼중음 (높은음 -> 중간음 -> 낮은음) 시작
                buzzer_mode = 2;
                buzzer_stage = 1;
                buzzer_timer = 50; // 각 음당 약 150ms
                buzzer_init_value = 50;
                PR2 = 47;    // 높은음 (약 2.6kHz)
                CCPR1L = 24; // 50% 듀티
                is_running = 0;
                *p_second = 0;
                Update_Heater_PWM();
              }
            }
          } else {
            // 일반 키: 기능이 실제로 작동(값 변경)될 때만 부저 울림
            unsigned char do_buzz = 0;

            if (k == 1) { // SW2 (LEVEL 토글) - 항상 작동
              is_hi_mode = !is_hi_mode;
              do_buzz = 1;
              Update_Heater_PWM();
            } else if (k == 2) { // SW3 (TIME_UP) - 최대(30분) 아닐 때만
              if (*p_minute < 30) {
                (*p_minute)++;
                do_buzz = 1;
              }
            } else if (k == 3) { // SW4 (TIME_DN) - 최소(0분) 아닐 때만
              if (*p_minute > 0) {
                (*p_minute)--;
                do_buzz = 1;
              }
            } else if (k == 4) { // SW5 (LEVEL_UP) - 동작 중이고 최대 레벨 아닐 때만
              if (is_running && current_level < 6) {
                current_level++;
                do_buzz = 1;
                Update_Heater_PWM();
              }
            } else if (k == 5) { // SW6 (LEVEL_DN) - 동작 중이고 최소 레벨 아닐 때만
              if (is_running && current_level > 1) {
                current_level--;
                do_buzz = 1;
                Update_Heater_PWM();
              }
            }

            if (do_buzz && buzzer_timer == 0) {
              // [핵심 안전장치] 부저 삼중음 진행 중에는 다른 키로 부저를 재트리거하지 않음
              if (!(buzzer_timer > 0 && (buzzer_mode == 1 || buzzer_mode == 2))) {
                CCP1CON = 0x8F;
                T2CON = 0xE0;
                buzzer_mode = 0;
                buzzer_stage = 1;
                buzzer_timer = 100;
                buzzer_init_value = 100;
                if (k == 2 || k == 4) {
                  PR2 = 43;
                  CCPR1L = 22; // UP 계열 → 고음
                } else if (k == 3 || k == 5) {
                  PR2 = 73;
                  CCPR1L = 37; // DN 계열 → 저음
                } else {
                  PR2 = 45;
                  CCPR1L = 23; // LEVEL 토글 → 중간음
                }
              }
            }
          }
        }
      } else {
        // 키가 계속 눌린 홀드 상태 (Hold)
        // TIME_UP(SW3) 및 TIME_DN(SW4) 버튼만 연속 입력(Auto-repeat)을 적용
        if (k == 2 || k == 3) {
          key_hold_count[k]++;
          if (key_hold_count[k] >= 150) { // 약 500ms 유지 시 작동 시작
            key_repeat_count[k]++;
            if (key_repeat_count[k] >= 30) { // 약 100ms 간격으로 반복
              key_repeat_count[k] = 0;

              if (k == 2) {
                if (*p_minute < 30)
                  (*p_minute)++;
              } else if (k == 3) {
                if (*p_minute > 0)
                  (*p_minute)--;
              }
            }
          }
        }
      }
    } else {
      // 키가 떨어져 있는 상태 (Release)
      // [안전 장치] 부저 구동 중 발생하는 강한 노이즈로 인한 키 판독 채터링 오작동을
      // 방지하기 위해, 부저 작동 중에는 릴리즈 감쇠를 일시 중지(Freeze)합니다.
      if (buzzer_timer == 0 && CCP1CON == 0x00) {
        if (key_db_count[k] > 0) {
          key_db_count[k]--;
        } else {
          key_active[k] = 0;
          key_hold_count[k] = 0;
          key_repeat_count[k] = 0;
        }
      }
    }
  }
}
