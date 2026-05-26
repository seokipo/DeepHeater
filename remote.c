#include <stdio.h>
#include "remote.h"
#include "main.h"
#include "system.h"
#include "pwm_control.h"


// 리모컨 상태 변수 정의
static volatile unsigned char ir_state = 0; // 0:IDLE 1:LEADER_WAIT 2:DATA 3:ERROR_WAIT
static volatile unsigned long ir_raw_data = 0;
static volatile unsigned char ir_bit_cnt = 0;
volatile unsigned char ir_cmd_ready = 0;
volatile unsigned char ir_cmd_value = 0;

volatile unsigned char ir_leader_detected = 0;
volatile unsigned char ir_err_code = 0;
volatile unsigned int ir_err_t = 0;

void IR_Init(void) {
  T1CLK = 0x01;  // Timer1 클럭소스: Fosc/4 (8MHz)
  T1CON = 0x33;  // CKPS=11(1:8 프리스케일러), RD16=1, ON=1 → 1μs/tick
  T1GCON = 0x00; // Timer1 게이트 제어 기능 비활성화 (카운터 정지 방지)
  TMR1H = 0;
  TMR1L = 0;
  IOCAPbits.IOCAP4 = 0; // RA4 상승 엣지 감지 비활성화 (하강 엣지만 사용)
  IOCANbits.IOCAN4 = 1; // RA4 하강 엣지(falling) 감지 활성화
  IOCAFbits.IOCAF4 = 0; // 플래그 초기화
  PIE0bits.IOCIE = 1;   // IOC 인터럽트 활성화
  INTCONbits.PEIE = 1;  // 주변장치 인터럽트 활성화
  INTCONbits.GIE = 1;   // 전역 인터럽트 활성화
}

/**
 * @brief 인터럽트 내 하강 에지 계측에 연동하여 실행될 디코더 상태 머신
 */
void IR_Decode_Process(void) {
  unsigned int t;

  // Timer1 오버플로우 플래그 확인 (65.5ms 이상의 긴 유휴 상태 판단)
  if (PIR4bits.TMR1IF) {
    PIR4bits.TMR1IF = 0;
    t = 30000; // 25ms를 초과하는 강제 동기화 값 적용
  } else {
    unsigned char tmr1_l = TMR1L;
    unsigned char tmr1_h = TMR1H;
    t = ((unsigned int)tmr1_h << 8) | tmr1_l;
  }

  // 400us 미만의 극단적으로 짧은 엣지 간격은 노이즈로 간주하고 완벽 차단
  if (t < 400) {
    return; // TMR1을 리셋하지 않고 그대로 무시
  }

  TMR1H = 0; // 유효한 엣지일 때만 다음 계측 시작
  TMR1L = 0;

  // 25ms (25000us) 이상의 긴 유휴 간격이 감지되면 상태 머신을 IDLE로 강제 동기화
  if (t > 25000) {
    ir_state = 1; // 새로운 리더의 첫 엣지로 계승
    return;
  }

  switch (ir_state) {
  case 0: // IDLE
    ir_state = 1;
    break;

  case 1: // LEADER
    if (t > 6000 && t < 22000) {
      ir_raw_data = 0;
      ir_bit_cnt = 0;
      ir_state = 2;
      ir_leader_detected = 1;
    } else {
      ir_err_code = 1;
      ir_err_t = t;
      ir_state = 3;
    }
    break;

  case 2: // DATA
    if (t >= 400 && t < 1500) {
      ir_raw_data >>= 1;
    } else if (t >= 1500 && t < 3500) {
      ir_raw_data >>= 1;
      ir_raw_data |= 0x80000000UL;
    } else {
      ir_err_code = 2;
      ir_err_t = t;
      ir_state = 3;
      break;
    }

    if (++ir_bit_cnt >= 32) {
      unsigned int addr = (unsigned int)(ir_raw_data & 0xFFFF);
      unsigned char cmd = (unsigned char)((ir_raw_data >> 16) & 0xFF);
      unsigned char cmd_inv = (unsigned char)((ir_raw_data >> 24) & 0xFF);
      if (addr == IR_DEVICE_ADDR && (unsigned char)(cmd ^ cmd_inv) == 0xFF) {
        ir_cmd_value = cmd;
        ir_cmd_ready = 1;
        ir_state = 0;
      } else {
        ir_err_code = 5;
        ir_err_t = addr;
        ir_state = 3;
      }
    }
    break;

  case 3: // ERROR_WAIT
    if (t >= 8000) {
      ir_state = 1;
    }
    break;
  }
}

/**
 * @brief 디코딩 완료된 리모컨 명령어에 대한 치료 동작 가변 업데이트 처리
 */
void IR_Process_Command(unsigned char *p_minute, char *p_second, unsigned int *p_tick_ms) {
  if (ir_cmd_ready) {
    INTCONbits.GIE = 0; // 원자적 읽기 보호
    unsigned char cmd = ir_cmd_value;
    ir_cmd_ready = 0;
    INTCONbits.GIE = 1;

    if (setting_mode) {
      if (cmd == IR_CMD_NUM2) {
        if (pwm_setting_value < 65) {
          pwm_setting_value++;
          printf("PWM Set: %d\r\n", pwm_setting_value);
          CCP1CON = 0x8F;
          T2CON = 0xE0;
          PR2 = 43;
          CCPR1L = 22; // 고음
          buzzer_mode = 0;
          buzzer_stage = 1;
          buzzer_timer = 100;
          buzzer_init_value = 100;
        }
      } else if (cmd == IR_CMD_NUM1) {
        if (pwm_setting_value > 20) {
          pwm_setting_value--;
          printf("PWM Set: %d\r\n", pwm_setting_value);
          CCP1CON = 0x8F;
          T2CON = 0xE0;
          PR2 = 73;
          CCPR1L = 37; // 저음
          buzzer_mode = 0;
          buzzer_stage = 1;
          buzzer_timer = 100;
          buzzer_init_value = 100;
        }
      } else if (cmd == IR_CMD_NUM3) {
        eeprom_write(0x00, pwm_setting_value);
        printf("Saved to EEPROM: %d\r\n", pwm_setting_value);
        // 삐-삑 저장 완료음
        CCP1CON = 0x8F;
        T2CON = 0xE0;
        PR2 = 45;
        CCPR1L = 23;
        buzzer_mode = 3;
        buzzer_stage = 1;
        buzzer_timer = 80;
        buzzer_init_value = 80;
      }
    } else {
      // 삼중음 진행 중(mode=1, 2)인지 감지
      unsigned char is_triple_buzzing = (buzzer_timer > 0 && (buzzer_mode == 1 || buzzer_mode == 2));
      
      switch (cmd) {
      case IR_CMD_START:
        if (!is_triple_buzzing) {
          CCP1CON = 0x8F;
          T2CON = 0xE0;
          if (is_running == 0) {
            // 스타트: 상승 삼중음
            buzzer_mode = 1;
            buzzer_stage = 1;
            buzzer_timer = 50;
            buzzer_init_value = 50;
            PR2 = 73;
            CCPR1L = 37;
            is_running = 1;
            current_level = 1;
            *p_second = 0;
            *p_tick_ms = 0;
            Update_Heater_PWM();
          } else {
            // 스톱: 하강 삼중음
            buzzer_mode = 2;
            buzzer_stage = 1;
            buzzer_timer = 50;
            buzzer_init_value = 50;
            PR2 = 47;
            CCPR1L = 24;
            is_running = 0;
            *p_second = 0;
            Update_Heater_PWM();
          }
        }
        break;
      case IR_CMD_LVL_UP:
        // 동작 중 + 최대 레벨 미만일 때만 부저
        if (is_running && current_level < 6) {
          current_level++;
          Update_Heater_PWM();
          if (!is_triple_buzzing) {
            CCP1CON = 0x8F;
            T2CON = 0xE0;
            buzzer_mode = 0;
            buzzer_stage = 1;
            buzzer_timer = 100;
            buzzer_init_value = 100;
            PR2 = 43;
            CCPR1L = 22;
          }
        }
        break;
      case IR_CMD_LVL_DN:
        // 동작 중 + 최소 레벨 초과일 때만 부저
        if (is_running && current_level > 1) {
          current_level--;
          Update_Heater_PWM();
          if (!is_triple_buzzing) {
            CCP1CON = 0x8F;
            T2CON = 0xE0;
            buzzer_mode = 0;
            buzzer_stage = 1;
            buzzer_timer = 100;
            buzzer_init_value = 100;
            PR2 = 73;
            CCPR1L = 37;
          }
        }
        break;
      case IR_CMD_LVL_HI:
        // Hi 모드 전환: 이미 Hi가 아닐 때만 부저
        if (!is_hi_mode) {
          is_hi_mode = 1;
          Update_Heater_PWM();
          if (!is_triple_buzzing) {
            CCP1CON = 0x8F;
            T2CON = 0xE0;
            buzzer_mode = 0;
            buzzer_stage = 1;
            buzzer_timer = 100;
            buzzer_init_value = 100;
            PR2 = 43;
            CCPR1L = 22;
          }
        }
        break;
      case IR_CMD_LVL_LO:
        // Lo 모드 전환: 이미 Lo가 아닐 때만 부저
        if (is_hi_mode) {
          is_hi_mode = 0;
          Update_Heater_PWM();
          if (!is_triple_buzzing) {
            CCP1CON = 0x8F;
            T2CON = 0xE0;
            buzzer_mode = 0;
            buzzer_stage = 1;
            buzzer_timer = 100;
            buzzer_init_value = 100;
            PR2 = 73;
            CCPR1L = 37;
          }
        }
        break;
      case IR_CMD_TIME_UP:
        // 최대(30분) 미만일 때만 부저
        if (*p_minute < 30) {
          (*p_minute)++;
          if (!is_triple_buzzing) {
            CCP1CON = 0x8F;
            T2CON = 0xE0;
            buzzer_mode = 0;
            buzzer_stage = 1;
            buzzer_timer = 100;
            buzzer_init_value = 100;
            PR2 = 43;
            CCPR1L = 22;
          }
        }
        break;
      case IR_CMD_TIME_DN:
        // 최소(0분) 초과일 때만 부저
        if (*p_minute > 0) {
          (*p_minute)--;
          if (!is_triple_buzzing) {
            CCP1CON = 0x8F;
            T2CON = 0xE0;
            buzzer_mode = 0;
            buzzer_stage = 1;
            buzzer_timer = 100;
            buzzer_init_value = 100;
            PR2 = 73;
            CCPR1L = 37;
          }
        }
        break;
      case IR_CMD_NUM1:
        if (!is_triple_buzzing) {
          CCP1CON = 0x8F;
          T2CON = 0xE0;
          buzzer_mode = 0;
          buzzer_stage = 1;
          buzzer_timer = 100;
          buzzer_init_value = 100;
          PR2 = 45;
          CCPR1L = 23;
        }
        break;
      case IR_CMD_NUM2:
        if (!is_triple_buzzing) {
          CCP1CON = 0x8F;
          T2CON = 0xE0;
          buzzer_mode = 0;
          buzzer_stage = 1;
          buzzer_timer = 100;
          buzzer_init_value = 100;
          PR2 = 45;
          CCPR1L = 23;
        }
        break;
      case IR_CMD_NUM3:
        if (!is_triple_buzzing) {
          CCP1CON = 0x8F;
          T2CON = 0xE0;
          buzzer_mode = 0;
          buzzer_stage = 1;
          buzzer_timer = 100;
          buzzer_init_value = 100;
          PR2 = 45;
          CCPR1L = 23;
        }
        break;
      case IR_CMD_NUM4:
        if (!is_triple_buzzing) {
          CCP1CON = 0x8F;
          T2CON = 0xE0;
          buzzer_mode = 0;
          buzzer_stage = 1;
          buzzer_timer = 100;
          buzzer_init_value = 100;
          PR2 = 45;
          CCPR1L = 23;
        }
        break;
      }
    }
  }
}
