#include <stdio.h>
#include "main.h"
#include "system.h"
#include "display.h"
#include "key.h"
#include "pwm_control.h"
#include "remote.h"

// 시스템 전역 변수 정의
volatile unsigned char setting_mode = 0;
volatile unsigned char is_running = 0;
volatile unsigned char current_level = 1;
volatile unsigned char is_hi_mode = 0;
volatile unsigned char pwm_setting_value = 45;

/**
 * @brief 전역 비동기 인터럽트 서비스 루틴 (ISR)
 * - EUSART TX 인터럽트 처리 (UART 비동기 송출)
 * - EUSART RX 인터럽트 처리 (UART CLI 명령어 입력 수신)
 * - IOC 인터럽트 처리 (적외선 리모컨 하강 엣지 감지 분석 연동)
 */
void __interrupt() isr(void) {
  // 1. 비동기 UART TX 인터럽트 처리
  if (PIE3bits.TXIE && PIR3bits.TXIF) {
    if (tx_head != tx_tail) {
      TX1REG = tx_buffer[tx_tail];
      tx_tail = (tx_tail + 1) & (TX_BUF_SIZE - 1);
    } else {
      PIE3bits.TXIE = 0; // 보낼 데이터 없으면 인터럽트 비활성화
    }
  }

  // 2. 비동기 UART RX 인터럽트 처리
  if (PIE3bits.RCIE && PIR3bits.RCIF) {
    rx_cmd = RC1REG;
    rx_ready = 1;
    if (RC1STAbits.OERR) { // 오버런 에러 클리어 복구
      RC1STAbits.CREN = 0;
      RC1STAbits.CREN = 1;
    }
  }

  // 3. 적외선 리모콘 수신 인터럽트 처리
  if (IOCAFbits.IOCAF4) {
    IOCAFbits.IOCAF4 = 0;
    IR_Decode_Process(); // remote.c에 구현된 디코더 상태 머신 호출
  }
}

void main(void) {
  // 시스템 전체 초기화 (클럭, GPIO, ADC 등)
  System_Init();

  // 히터 PWM(PWM6, PWM7) 하드웨어 기동
  Heater_PWM_Init();

  // 피에조 부저용 PWM(CCP1) 초기 기동
  Buzzer_PWM_Init();

  // UART 통신 기동 (9600bps)
  UART_Init();

  // 리모컨 외부 핀 인터럽트 기동
  IR_Init();

  // UV LED 소등 초기화 (Active LOW)
  LATA5 = 1;

  // FND 및 LED DRV 공통 애노드 차단 (Active LOW)
  LATA1 = 1;
  LATA2 = 1;
  LATA3 = 1;

  printf("Deep Heater Board is ALIVE (Async UART)!\r\n");

  // ===================================================================
  // 부저 하드웨어 자가진단 테스트 (부팅 시 1회 실행)
  // ===================================================================
  printf("[BOOT] Buzzer Diagnostic Start...\r\n");
  CCPTMRS0 = 0x01; // CCP1을 Timer2에 바인딩
  CCP1CON = 0x8F;  // CCP1 활성화
  T2CON = 0xE0;    // Timer2 활성화
  PR2 = 45;        // ~2.7kHz
  CCPR1L = 23;     // 50% 듀티
  __delay_ms(200); // 200ms 동안 비프음 발생
  CCPR1L = 0;
  CCP1CON = 0x00;
  T2CON = 0x00;
  LATC0 = 0;
  printf("[BOOT] Buzzer Diagnostic Done\r\n");
  __delay_ms(100);

  // 부팅 직후 특수기능 레지스터 덤프 정보 출력
  printf("=== REG DUMP ===\r\n");
  printf("CCP1=%02X T2=%02X PR2=%u D=%u\r\n", CCP1CON, T2CON, PR2, CCPR1L);
  printf("TMS0=%02X TMS1=%02X\r\n", CCPTMRS0, CCPTMRS1);
  printf("T2CLK=%02X T4CLK=%02X\r\n", T2CLKCON, T4CLKCON);
  __delay_ms(150); // TX 버퍼 비움 대기
  printf("PWM6=%02X T4=%02X PR4=%u\r\n", PWM6CON, T4CON, PR4);
  printf("RC0=%02X RC6=%02X\r\n", RC0PPS, RC6PPS);
  printf("TRISC=%02X LATC=%02X ANS=%02X\r\n", TRISC, LATC, ANSELC);
  printf("OSCEN=%02X STAT=%02X\r\n", OSCEN, OSCSTAT);
  printf("===END===\r\n");

  // EEPROM에 저장된 이전 PWM 설정 로딩 및 복구
  pwm_setting_value = eeprom_read(0x00);
  if (pwm_setting_value < 20 || pwm_setting_value > 65) {
    pwm_setting_value = 45;
  }

  // 타이머 및 시간 흐름 연동 변수 선언
  unsigned char minute = 29;
  char second = 0;
  unsigned int tick_ms = 0;

  // 설정 모드 진입 판별 (부팅 직후 SW1 + SW2 누름 감지)
  TRISCbits.TRISC7 = 0;
  LATCbits.LATC7 = 0;
  TRISB = 0x3F;
  __delay_us(10);
  if ((PORTB & 0x03) == 0x00) {
    setting_mode = 1;
    printf("Entering Setting Mode...\r\n");
    printf("PWM Set: %d\r\n", pwm_setting_value);

    // 진입 삑-음 재생
    CCP1CON = 0x8F;
    T2CON = 0xE0;
    CCPTMRS0 = 0x01;
    PR2 = 45;
    CCPR1L = 23;
    buzzer_mode = 0;
    buzzer_stage = 1;
    buzzer_timer = 200;
    buzzer_init_value = 200;
  }
  TRISB = 0x00;
  LATCbits.LATC7 = 1;
  TRISCbits.TRISC7 = 1;

  Update_Heater_PWM();

  while (1) {
    // --- UART CLI 디버그 수신 명령어 처리 ---
    if (rx_ready) {
      rx_ready = 0;
      char debug_cmd = rx_cmd;
      printf("CLI Cmd: %c\r\n", debug_cmd);

      if (debug_cmd == 'H' || debug_cmd == 'h') {
        debug_toggle_mode = 0;
        RC0PPS = 0x00;         // 일반 GPIO 전환
        ODCONCbits.ODCC0 = 0;  // Push-pull 활성화
        SLRCONCbits.SLRC0 = 0;
        LATCbits.LATC0 = 1;    // 5V 강제 출력
        printf("Buzzer (RC0) -> HIGH (5V)\r\n");
      } else if (debug_cmd == 'L' || debug_cmd == 'l') {
        debug_toggle_mode = 0;
        RC0PPS = 0x00;
        ODCONCbits.ODCC0 = 0;
        SLRCONCbits.SLRC0 = 0;
        LATCbits.LATC0 = 0;    // 0V 강제 출력
        printf("Buzzer (RC0) -> LOW (0V)\r\n");
      } else if (debug_cmd == 'T' || debug_cmd == 't') {
        debug_toggle_mode = !debug_toggle_mode;
        if (debug_toggle_mode) {
          RC0PPS = 0x00;
          ODCONCbits.ODCC0 = 0;
          SLRCONCbits.SLRC0 = 0;
          LATCbits.LATC0 = 1;
          printf("Buzzer (RC0) -> 1s Toggle Mode ON\r\n");
        } else {
          LATCbits.LATC0 = 0;
          printf("Buzzer (RC0) -> 1s Toggle Mode OFF\r\n");
        }
      } else if (debug_cmd == 'P' || debug_cmd == 'p') {
        debug_toggle_mode = 0;
        CCP1CON = 0x8F;
        T2CON = 0xE0;
        RC0PPS = 0x09; // CCP1 PWM 출력
        PR2 = 45;
        CCPR1L = 23; // 50% 듀티
        ODCONCbits.ODCC0 = 0;
        SLRCONCbits.SLRC0 = 0;
        printf("Buzzer (RC0) -> PWM Continuous Sound ON (2.7kHz)\r\n");
      } else if (debug_cmd == 'R' || debug_cmd == 'r') {
        printf("=== REG DUMP ===\r\n");
        printf("LATC=%02X TRISC=%02X RC0PPS=%02X\r\n", LATC, TRISC, RC0PPS);
        printf("CCP1CON=%02X T2CON=%02X PR2=%u CCPR1L=%u\r\n", CCP1CON, T2CON, PR2, CCPR1L);
        printf("is_running=%u setting_mode=%u debug_toggle=%u\r\n", is_running, setting_mode, debug_toggle_mode);
        printf("FOOT_SEN (AN0) ADC: %u\r\n", ADC_Read(0));
      } else if (debug_cmd == 'S' || debug_cmd == 's') {
        debug_toggle_mode = 0;
        RC0PPS = 0x00;
        LATC0 = 0;
        CCP1CON = 0x00;
        CCPR1L = 0;
        ODCONCbits.ODCC0 = 0;
        SLRCONCbits.SLRC0 = 0;
        printf("Buzzer (RC0) -> System Restored, Buzzer Silent\r\n");
      }
    }

    // UV LED 동작 상태 연동 스위칭
    LATA5 = is_running ? 0 : 1;

    // --- 3단계 FND 및 LED 디스플레이 동적 스캔 구동 ---
    Display_Process(setting_mode ? pwm_setting_value : minute);

    if (!setting_mode) {
      // --- 백그라운드 1초 타이머 연동 (다운 카운트) ---
      tick_ms += 3; // 1스캔 주기(약 3.2ms)마다 3ms 누적
      if (tick_ms >= 1000) {
        tick_ms -= 1000;

        // CLI 1초 토글 디버그 모드 처리
        if (debug_toggle_mode) {
          LATC0 = !LATC0;
          printf("[DBG] Toggle RC0 = %d\r\n", LATC0);
        }

        if (is_running) {
          if (minute > 0 || second > 0) {
            if (second == 0) {
              if (minute > 0) {
                minute--;
                second = 59;
              }
            } else {
              second--;
            }
          }

          // 시간 완료 시 정지 및 완료음 비블로킹 기동
          if (minute == 0 && second == 0) {
            is_running = 0;
            second = 0;
            Update_Heater_PWM();

            CCP1CON = 0x8F;
            T2CON = 0xE0;
            RC0PPS = 0x09;
            PR2 = 45;
            CCPR1L = 23;
            buzzer_mode = 0;
            buzzer_stage = 1;
            buzzer_timer = 450;
            buzzer_init_value = 450;
          } else {
            // 히터 출력 강도에 따른 풋 센서 AD 피드백 연동 계산 처리 (서브 모듈 위임)
            Heater_Feedback_Process();
          }
        }
      }

      // --- 키 입력 매트릭스 디바운스 및 상태 갱신 처리 ---
      Key_Process(&minute, &second, &tick_ms);

      // --- 리모콘 적외선 커맨드 해석 및 실행 처리 ---
      IR_Process_Command(&minute, &second, &tick_ms);

      // --- 비동기 부저 타이머 스케줄러 처리 ---
      Buzzer_Process();

      // 히터 PWM 동적 잠금 갱신
      Update_Heater_PWM();
    }
  }
}
