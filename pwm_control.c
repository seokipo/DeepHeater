#include "pwm_control.h"
#include "main.h"
#include "system.h"

// 풋 센서 ADC 문턱 전압 임계값 상한/하한 정의
const unsigned int FOOT_ADC_THRES_LO[6] = {310, 465, 620, 713, 775, 806};
const unsigned int FOOT_ADC_THRES_HI[6] = {403, 651, 775, 837, 868, 868};

// 히터 피드백 제어 관련 내부 정적 변수
volatile unsigned int last_foot_adc_val = 0;
static unsigned int feedback_duration_us = 500;
static unsigned int last_feedback_adc_val = 0;

// 시스템 전역 부저 제어 변수 정의
volatile unsigned int buzzer_timer = 0;
volatile unsigned int buzzer_init_value = 1;
volatile unsigned char buzzer_mode = 0;
volatile unsigned char buzzer_stage = 0;

/**
 * @brief 피에조 부저용 하드웨어 PWM(CCP1 & Timer2) 초기화 함수
 * - Timer2를 클럭 소스로 활용하여 약 2.7kHz의 구형파 생성
 * - 50% 듀티 비율(CCPR1L = 23, PR2 = 45)로 맑은 버저음 발생 설정
 */
void Buzzer_PWM_Init(void) {
  TRISCbits.TRISC0 = 0; // RC0 출력 설정
  LATCbits.LATC0 = 0;   // RC0 0V 초기화
  CCP1CON = 0x00;       // CCP1 모듈 완전히 끄기
  T2CON = 0x00;         // Timer2 끄기

  // [핵심] HLT(Hardware Limit Timer) 잔재 완벽 초기화 (Lock-up 원천 차단)
  T2HLT = 0x00;
  T2RST = 0x00;
  TMR2 = 0x00;

  // 1. CCP1 타이머 배정 (V0.9.21 규격 확정 설정)
  // C1TSEL=01: Timer2 사용
  CCPTMRS0 = (CCPTMRS0 & 0xFC) | 0x01;

  // 2. 타이머 설정
  T2CLKCON = 0x01; // Timer2 클럭 소스를 Fosc/4 (8MHz)로 설정
  PR2 = 45;        // 주파수 2717Hz (약 2.7kHz) 설정

  // 3. 듀티 설정
  CCPR1L = 0; // 처음엔 무음 상태 (0% 듀티)
  CCPR1H = 0;

  // 5. 타이머 끄기 (초기 상태 무음 유지)
  T2CON = 0x00;

  // 6. 모듈 끄기
  CCP1CON = 0x00;

  // 7. 부저 핀 매핑 고정 (하드웨어 PPS Lock 이슈 회피를 위해 최초 1회만 매핑)
  RC0PPS = 0x09; // CCP1 출력을 RC0에 고정 연결
}

void Heater_PWM_Init(void) {
  TMR4 = 0;
  T4HLT = 0;
  T4RST = 0;
  TMR6 = 0;
  T6HLT = 0;
  T6RST = 0;

  CCPTMRS1 = (CCPTMRS1 & 0xF3) | 0x08;

  T4CLKCON = 0x01;
  PR4 = 7;
  T4CON = 0x80;

  PWM6CON = 0x80;
  PWM6DCH = 0;
  PWM6DCL = 0;

  CCPTMRS1 = (CCPTMRS1 & 0xCF) | 0x30;

  T6CLKCON = 0x05;
  PR6 = 155;
  T6CON = 0x80;

  PWM7CON = 0x80;
  PWM7DCH = 0;
  PWM7DCL = 0;

  MDCON0 = 0x80;
  MDCON1 = 0x00;
  MDSRC = 0x08;
  MDCARH = 0x09;
  MDCARL = 0x00;

  MDCARLPPS = 0x13;
  RC1PPS = 0x00;
  TRISCbits.TRISC1 = 0;
  LATCbits.LATC1 = 0;
}

void Update_Heater_PWM(void) {
  static unsigned char prev_is_running = 0;

  if (is_running && !prev_is_running) {
    feedback_duration_us = 500;
    last_feedback_adc_val = ADC_Read(0x12);
  }
  if (!is_running && prev_is_running) {
    feedback_duration_us = 500;
    last_feedback_adc_val = 0;
  }
  prev_is_running = is_running;

  if (!is_running || setting_mode) {
    RC1PPS = 0x00;
    LATCbits.LATC1 = 0;
    PWM6DCH = 0x00;
    PWM6DCL = 0x00;
    PWM7DCH = 0x00;
    PWM7DCL = 0x00;
    return;
  }

  unsigned int duty_val_6 = (unsigned int)((32UL * pwm_setting_value) / 100UL);
  PWM6DCH = (duty_val_6 >> 2) & 0xFF;
  PWM6DCL = (duty_val_6 & 0x03) << 6;

  unsigned int max_duration_us = 0;
  switch (current_level) {
  case 1:
    max_duration_us = is_hi_mode ? 750 : 500;
    break;
  case 2:
    max_duration_us = is_hi_mode ? 1500 : 1000;
    break;
  case 3:
    max_duration_us = is_hi_mode ? 2250 : 1500;
    break;
  case 4:
    max_duration_us = is_hi_mode ? 3000 : 2000;
    break;
  case 5:
    max_duration_us = is_hi_mode ? 3750 : 2500;
    break;
  case 6:
    max_duration_us = is_hi_mode ? 4500 : 3000;
    break;
  default:
    max_duration_us = 0;
    break;
  }

  static unsigned char adc_interval_cnt = 0;
  if (adc_interval_cnt < 30) {
    adc_interval_cnt++;
  }
  if (adc_interval_cnt >= 30) {
    last_foot_adc_val = ADC_Read(0x12);
    adc_interval_cnt = 0;
  }

  unsigned int duration_us = feedback_duration_us;
  if (duration_us > max_duration_us) {
    duration_us = max_duration_us;
  }
  if (duration_us < 500) {
    duration_us = 500;
  }

  unsigned int duty_val_7 = (unsigned int)((duration_us * 624UL) / 5000UL);
  PWM7DCH = (duty_val_7 >> 2) & 0xFF;
  PWM7DCL = (duty_val_7 & 0x03) << 6;

  TRISCbits.TRISC1 = 0;
  RC1PPS = 0x1B;
}

/**
 * @brief 비동기 부저 소프트웨어 타이머 처리 및 음량 감쇄 제어
 */
void Buzzer_Process(void) {
  if (buzzer_timer > 0) {
    // 부저 최초 활성화 시점 안전 재확인 (CCP1/T2 확실히 ON)
    if (buzzer_timer == buzzer_init_value) {
      CCP1CON = 0x8F; // CCP1 활성화 (RC0PPS=0x09는 고정)
      T2CON = 0xE0;
    }
    buzzer_timer--;

    // 현재 활성 주파수 레벨에서의 최대 50% 듀티 값 산출
    unsigned char max_duty = (unsigned char)((PR2 + 1) / 2);

    if (buzzer_timer == 0) {
      // 한 단계 소리 작동 완료 시점의 3단계 전이 처리
      if (buzzer_mode == 1) { // 스타트 상승 삼중음
        if (buzzer_stage == 1) {
          // 1단계(낮은음) 종료 -> 2단계(중간음) 진입
          buzzer_stage = 2;
          buzzer_timer = 50;
          buzzer_init_value = 50;
          PR2 = 56;    // 중간음 (약 2.2kHz)
          CCPR1L = 28; // 50% 듀티
        } else if (buzzer_stage == 2) {
          // 2단계(중간음) 종료 -> 3단계(높은음) 진입
          buzzer_stage = 3;
          buzzer_timer = 120; // 3단계(끝음) 길게 설정 (약 360ms)
          buzzer_init_value = 120;
          PR2 = 47;    // 높은음 (약 2.6kHz)
          CCPR1L = 24; // 50% 듀티
        } else {
          // 3단계(높은음) 종료 → 전체 부저 출력 완료 및 하드웨어 소등
          buzzer_stage = 0;
          CCP1CON = 0x00;
          T2CON = 0x00;
          CCPR1L = 0;
          LATCbits.LATC0 = 0;
        }
      } else if (buzzer_mode == 2) { // 스톱 하강 삼중음
        if (buzzer_stage == 1) {
          // 1단계(높은음) 종료 -> 2단계(중간음) 진입
          buzzer_stage = 2;
          buzzer_timer = 50;
          buzzer_init_value = 50;
          PR2 = 56;    // 중간음 (약 2.2kHz)
          CCPR1L = 28; // 50% 듀티
        } else if (buzzer_stage == 2) {
          // 2단계(중간음) 종료 -> 3단계(낮은음) 진입
          buzzer_stage = 3;
          buzzer_timer = 120; // 3단계(끝음) 길게 설정 (약 360ms)
          buzzer_init_value = 120;
          PR2 = 73;    // 낮은음 (약 1.7kHz)
          CCPR1L = 37; // 50% 듀티
        } else {
          // 3단계(낮은음) 종료 → 전체 부저 출력 완료 및 하드웨어 소등
          buzzer_stage = 0;
          CCP1CON = 0x00;
          T2CON = 0x00;
          CCPR1L = 0;
          LATCbits.LATC0 = 0;
        }
      } else if (buzzer_mode == 3) { // 삐-삑 (저장 확인음)
        if (buzzer_stage == 1) {
          // 1단계(삐) 종료 -> 2단계(무음) 진입
          buzzer_stage = 2;
          buzzer_timer = 40; // 짧은 무음 간격
          buzzer_init_value = 40;
          CCPR1L = 0; // 0% 듀티로 뮤트 처리
        } else if (buzzer_stage == 2) {
          // 2단계(무음) 종료 -> 3단계(삑, 고음) 진입
          buzzer_stage = 3;
          buzzer_timer = 100; // 짧은 고음
          buzzer_init_value = 100;
          PR2 = 35; // 높은음 (~3.5kHz)
          CCPR1L = 18;
        } else {
          // 3단계(삑) 종료 → 완료 및 하드웨어 소등
          buzzer_stage = 0;
          CCP1CON = 0x00;
          T2CON = 0x00;
          CCPR1L = 0;
          LATCbits.LATC0 = 0;
          PR2 = 45;
        }
      } else {
        // 일반 단발음 감쇄 완료 및 하드웨어 소등
        buzzer_stage = 0;
        CCP1CON = 0x00; // 부저 비활성화 (RC0PPS=0x09 고정 유지)
        T2CON = 0x00;
        CCPR1L = 0;
        LATCbits.LATC0 = 0;
        PR2 = 45; // 기본 중간음 복원
      }
    } else {
      // 현재 단일 단계 내에서 비례 감쇄 연산 (감쇄음 효과)
      CCPR1L = (unsigned char)(((unsigned long)buzzer_timer * max_duty) /
                               buzzer_init_value);
    }
  }
}

/**
 * @brief 풋 센서 AD 피드백에 따른 히터 PWM 출력 갱신 처리 (1초 스케줄러 연동)
 */
void Heater_Feedback_Process(void) {
  unsigned int max_duration_us = 0;
  switch (current_level) {
  case 1:
    max_duration_us = is_hi_mode ? 750 : 500;
    break;
  case 2:
    max_duration_us = is_hi_mode ? 1500 : 1000;
    break;
  case 3:
    max_duration_us = is_hi_mode ? 2250 : 1500;
    break;
  case 4:
    max_duration_us = is_hi_mode ? 3000 : 2000;
    break;
  case 5:
    max_duration_us = is_hi_mode ? 3750 : 2500;
    break;
  case 6:
    max_duration_us = is_hi_mode ? 4500 : 3000;
    break;
  default:
    max_duration_us = 0;
    break;
  }

  if (max_duration_us > 500) {
    unsigned int cur_adc = last_foot_adc_val;
    if (cur_adc >=
        (unsigned int)((unsigned long)last_feedback_adc_val * 11 / 10)) {
      feedback_duration_us =
          (unsigned int)((unsigned long)feedback_duration_us * 13 / 10);
      if (feedback_duration_us > max_duration_us) {
        feedback_duration_us = max_duration_us;
      }
    } else if (cur_adc <
               (unsigned int)((unsigned long)last_feedback_adc_val * 9 / 10)) {
      feedback_duration_us =
          (unsigned int)((unsigned long)feedback_duration_us * 7 / 10);
      if (feedback_duration_us < 500) {
        feedback_duration_us = 500;
      }
    }
    last_feedback_adc_val = cur_adc;
  } else {
    feedback_duration_us = 500;
  }
  Update_Heater_PWM();
}

/**
 * @brief 치료 시간 경과에 따른 팬(FAN) 출력 제어 및 소프트웨어 PWM 타임 슬라이싱 처리
 * - 치료 시작 후 10분 경과 시: 20% 출력
 * - 치료 시작 후 20분 경과 시: 30% 출력
 * - 치료 시작 후 25분 경과 시: 50% 출력
 */
void Fan_Control_Process(void) {
  static unsigned int fan_tick_ms = 0;
  static unsigned int fan_elapsed_seconds = 0;
  static unsigned char fan_pwm_cnt = 0;
  static unsigned int one_second_cnt = 0;

  // 1. 치료 중이 아니면 넌블로킹 상태 타이머 변수 리셋 및 팬 즉시 정지
  if (!is_running) {
    fan_tick_ms = 0;
    fan_elapsed_seconds = 0;
    fan_pwm_cnt = 0;
    one_second_cnt = 0;
    FAN_LAT = 0; // 팬 소등
    return;
  }

  // 2. 넌블로킹 3ms 단위 틱 누적 및 시간 관리
  fan_tick_ms += 3;
  one_second_cnt += 3;
  if (one_second_cnt >= 1000) {
    one_second_cnt -= 1000;
    fan_elapsed_seconds++;
  }

  // 10ms 주기 소프트웨어 PWM 카운터 갱신
  if (fan_tick_ms >= 10) {
    fan_tick_ms -= 10;
    fan_pwm_cnt++;
    if (fan_pwm_cnt >= 10) { // 100ms 주기 완성
      fan_pwm_cnt = 0;
    }
  }

  // 3. 경과 시간에 따른 팬 구동 듀티 비율(%) 산정
  unsigned char duty_percent = 0;
  if (fan_elapsed_seconds >= 1500) {      // 25분 이상 경과 시
    duty_percent = 50;
  } else if (fan_elapsed_seconds >= 1200) { // 20분 이상 경과 시
    duty_percent = 30;
  } else if (fan_elapsed_seconds >= 600) {  // 10분 이상 경과 시
    duty_percent = 20;
  } else {                                  // 10분 미만 시 (0%)
    duty_percent = 0;
  }

  // 4. 소프트웨어 PWM 시분할 제어 매핑
  if (duty_percent == 0) {
    FAN_LAT = 0;
  } else if (duty_percent == 20) {
    FAN_LAT = (fan_pwm_cnt < 2) ? 1 : 0;
  } else if (duty_percent == 30) {
    FAN_LAT = (fan_pwm_cnt < 3) ? 1 : 0;
  } else if (duty_percent == 50) {
    FAN_LAT = (fan_pwm_cnt < 5) ? 1 : 0;
  }
}

