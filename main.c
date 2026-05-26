//* PIC16F18855 Configuration Bit Settings
// CONFIG1
#pragma config FEXTOSC =                                                       \
    OFF // External Oscillator mode selection bits (Oscillator not enabled)
#pragma config RSTOSC =                                                        \
    HFINT32 // Power-up default value for COSC bits (HFINTOSC (32MHz))
#pragma config CLKOUTEN = OFF // Clock Out Enable bit (CLKOUT function is
                              // disabled; I/O or oscillator function on OSC2)
#pragma config CSWEN =                                                         \
    ON // Clock Switch Enable bit (Writing to NOSC and NDIV is allowed)
#pragma config FCMEN = OFF // Fail-Safe Clock Monitor Enable bit (Fail-Safe
                           // Clock Monitor is disabled)

// CONFIG2
#pragma config MCLRE =                                                         \
    ON // Master Clear Enable bit (MCLR pin is Master Clear function)
#pragma config PWRTE = OFF   // Power-up Timer Enable bit (PWRT disabled)
#pragma config LPBOREN = OFF // Low-Power BOR enable bit (ULPBOR disabled)
#pragma config BOREN = ON    // Brown-out reset enable bits (Brown-out Reset
                             // Enabled, SBOREN bit is ignored)
#pragma config BORV = LO     // Brown-out Reset Voltage Selection bit (Brown-out
                             // Reset Voltage (VBOR) set to low trip point)
#pragma config ZCD = OFF     // Zero-Cross Detect disable bit (Zero-Cross Detect
                             // circuit is disabled at POR)
#pragma config PPS1WAY = OFF // Peripheral Pin Select one-way control (OFF로
                             // 설정하여 언제든 PPS 변경 허용)
#pragma config STVREN = ON   // Stack Overflow/Underflow Reset Enable bit (Stack
                             // Overflow or Underflow will cause a Reset)

// CONFIG3
#pragma config WDTCPS = WDTCPS_31 // WDT Period Select bits (Divider ratio
                                  // 1:65536; software control of WDTPS)
#pragma config WDTE =                                                          \
    OFF // WDT operating mode (WDT Disabled, SWDTEN is ignored)
#pragma config WDTCWS =                                                        \
    WDTCWS_7 // WDT Window Select bits (window always open (100%); software
             // control; keyed access not required)
#pragma config WDTCCS = SC // WDT input clock selector (Software Control)

// CONFIG4
// #pragma config WRTSAF = OFF     // Storage Area Flash Write Protect bit (SAF
// not write protected, V2.45 호환을 위해 주석 처리)
#pragma config LVP = ON // Low Voltage Programming Enable bit (Low Voltage
                        // programming enabled. MCLR/VPP pin function is MCLR.
                        // Mode-select register bit LVP is locked on.)

// CONFIG5
#pragma config CP = OFF // UserNVM Program memory code protection bit (UserNVM
                        // code protection disabled)

#define _XTAL_FREQ 32000000
#include <stdio.h>
#include <xc.h>

#define TX_BUF_SIZE 128
volatile char tx_buffer[TX_BUF_SIZE];
volatile unsigned char tx_head = 0;
volatile unsigned char tx_tail = 0;

// UART 수신(RX) CLI 디버깅용 전역 변수
volatile char rx_cmd = 0;
volatile unsigned char rx_ready = 0;
volatile unsigned char debug_toggle_mode = 0;

// 시스템 전역 설정 변수
volatile unsigned char pwm_setting_value = 45;

// 시스템 전역 동작 상태 변수
volatile unsigned char setting_mode = 0;  // 0: 일반 작동 모드, 1: 설정 모드
volatile unsigned char is_running = 0;    // 0: IDLE(대기), 1: RUN(작동)
volatile unsigned char current_level = 1; // 1~6 (Level 1~6)
volatile unsigned char is_hi_mode = 0;    // 0: Lo (LED7 ON), 1: Hi (LED8 ON)

// 시스템 전역 부저 제어 변수 (최적화 오작동 방지 volatile 선언)
volatile unsigned int buzzer_timer = 0;
volatile unsigned int buzzer_init_value = 1;
volatile unsigned char buzzer_mode = 0;
volatile unsigned char buzzer_stage = 0;

// =============================================================================
// NEC IR 리모콘 커맨드 정의 (Address: 0xEE11, NEC Extended 16-bit)
// =============================================================================
#define IR_DEVICE_ADDR 0xEE11U // 디바이스 16-bit 주소
#define IR_CMD_START 0x1A      // start/stop  → SW1 (START/STOP)
#define IR_CMD_LVL_UP 0x12     // level_up    → SW5 (LEVEL_UP)
#define IR_CMD_LVL_DN 0x14     // level_down  → SW6 (LEVEL_DN)
#define IR_CMD_LVL_HI 0x10     // Level_hi    → Lo/Hi → Hi 고정
#define IR_CMD_LVL_LO 0x42     // Level_low   → Lo/Hi → Lo 고정
#define IR_CMD_TIME_UP 0x18    // time_up     → SW3 (TIME_UP)
#define IR_CMD_TIME_DN 0x16    // time_down   → SW4 (TIME_DN)
#define IR_CMD_NUM1 0x2E       // Nun 1       → (미정, 플레이스홀더)
#define IR_CMD_NUM2 0x24       // Nun 2       → (미정, 플레이스홀더)
#define IR_CMD_NUM3 0x32       // Nun 3       → (미정, 플레이스홀더)
#define IR_CMD_NUM4 0x28       // Nun 4       → (미정, 플레이스홀더)

// NEC IR 디코더 전역 상태 변수 (ISR에서 접근하므로 volatile)
static volatile unsigned char ir_state =
    0; // 0:IDLE 1:LEADER_WAIT 2:DATA 3:ERROR_WAIT
static volatile unsigned long ir_raw_data =
    0; // 수신 중인 32비트 NEC 원시 데이터
static volatile unsigned char ir_bit_cnt = 0; // 비트 카운트 (0~31)
volatile unsigned char ir_cmd_ready = 0;      // 유효 커맨드 수신 완료 플래그
volatile unsigned char ir_cmd_value = 0;      // 수신된 커맨드 바이트

// 리모콘 수신 디버깅용 전역 변수
volatile unsigned char ir_leader_detected = 0;
volatile unsigned char ir_err_code =
    0; // 1:리더마크, 2:리더스페이스, 3:데이터마크, 4:데이터스페이스,
       // 5:체크섬/주소오류
volatile unsigned int ir_err_t = 0;

// ADC2 모듈 제어용 함수 프로토타입 선언
void ADC_Init(void);
unsigned int ADC_Read(unsigned char channel);

/**
 * @brief 시스템 초기화 함수
 * - 내부 오실레이터를 사용하여 32MHz 클럭 설정
 * - 디지털 I/O 방향 및 아날로그(ANSEL) 기능 제어
 * - 오동작 방지를 위한 포트 초기 출력값(LAT) 설정
 */
void System_Init(void) {
  // 1. 클럭 설정: HFINTOSC 32MHz
  // NOSC = HFINTOSC (110), NDIV = 1:1 (0000)
  OSCCON1 = 0x60;
  // HFFRQ = 32MHz (110)
  OSCFRQ = 0x06;

  // MFINTOSC 강제 활성화 (Timer4 동작 보장) 및 ADC용 오실레이터(ADOEN) 유지
  OSCEN = 0x24;

  // 2. 아날로그 기능 초기화
  // RA0/AN0(FOOT_SEN)만 아날로그 채널로 남겨두고 나머지는 모두 디지털 모드로
  // 설정
  ANSELA = 0x01;
  ANSELB = 0x00;
  ANSELC = 0x00;

  // 오픈 드레인 오동작 방지 및 드라이브 능력 최대화를 위해 명시적으로 설정
  ODCONC = 0x00;  // Open-Drain 완전 비활성화 (Push-Pull 모드 강제)
  SLRCONC = 0x00; // Slew Rate 제한 해제 (드라이브 강도 최대 확보)

  // 3. I/O 방향 설정 (TRIS)
  // RA0 (FOOT_SEN) - 입력 (1)
  // RA1 (DIGIT 1) - 출력 (0)
  // RA2 (DIGIT 2) - 출력 (0)
  // RA3 (LED DRV) - 출력 (0)
  // RA4 (REMOTE) - 입력 (1)
  // RA5 (LED UV) - 출력 (0)
  // RA6, RA7 - 출력 (0)
  TRISA = 0x11; // 0b00010001

  // RB0 ~ RB7 (FND A ~ DP) - FND/LED 캐소드 구동 출력이므로 모두 출력 (0)
  TRISB = 0x00;

  // RC0 (BUZZ) - 출력 (0)
  // RC1 (PWM) - 출력 (0)
  // RC2 (FAN) - 출력 (0)
  // RC3, RC4, RC5 - 출력 (0)
  // RC6 (232 TX) - 출력 (0)
  // RC7 (KEY IN) - 키 스캔 제어를 위해 출력 (0)
  // RC0 ~ RC7 - FND 디스플레이 및 스캔 구동용으로 모두 출력 (0)
  TRISC = 0x00; // 0b00000000

  // 4. 포트 초기 출력 설정 (LAT)
  // TR 구동 제어 핀(PNP형)은 켜지지 않도록 초기값 HIGH(1)로 설정
  // DIGIT 1(RA1)=1, DIGIT 2(RA2)=1, LED DRV(RA3)=1, LED UV(RA5)=1
  LATA = 0x2E; // 0b00101110

  // FND 및 LED 라인 캐소드를 모두 HIGH(1)로 해두어 완전 꺼짐 상태로 초기화
  LATB = 0xFF;

  // 부저(RC0), PWM(RC1), FAN(RC2) 등 구동 라인은 LOW(0)로 시작
  // RC7 (KEY IN)은 평상시 HIGH(1) 상태로 유지
  LATC = 0x80; // 0b10000000

  // 5. 외부 풀업 저항(R1~R6, R19)이 물리적으로 존재하므로 MCU 내부 풀업 전면
  // 비활성화 (잔상 억제)
  WPUA = 0x00; // 포트 A 내부 풀업 비활성화 (외부 풀업 R17이 존재하므로)
  WPUB = 0x00; // 0b00000000
  WPUC = 0x00; // 0b00000000

  // 6. RA4 입력 레벨을 TTL로 변경 (3.3V 리모콘 입력을 5V MCU에서 안정적으로
  // HIGH 인식하기 위함)
  INLVLAbits.INLVLA4 = 0;

  // 7. 시스템 부팅 시 모든 타이머와 모듈 완전 리셋
  CCP1CON = 0x00;
  PWM6CON = 0x00;
  PWM7CON = 0x00;
  T2CON = 0x00;
  T4CON = 0x00;
  T6CON = 0x00;
  T2HLT = 0x00;
  T4HLT = 0x00;
  T6HLT = 0x00;
  T2RST = 0x00;
  T4RST = 0x00;
  T6RST = 0x00;
  TMR2 = 0x00;
  TMR4 = 0x00;
  TMR6 = 0x00;
  MDCON0 = 0x00;
  MDSRC = 0x00;
  MDCARH = 0x00;
  MDCARL = 0x00;

  // UART 핀만 초기화 시 고정, 나머지 PWM 핀은 동적 할당
  RC6PPS = 0x10; // RC6 -> TX1
  RXPPS = 0x17;  // 임시 수동 디버깅을 위해 물리선이 연결된 RC7로 매핑 복원

  // 8. ADC 모듈 초기화 (FOOT_SEN 아날로그 리드용)
  ADC_Init();
}

/**
 * @brief ADC 모듈 초기화 함수
 * - ADCON0 설정: ADON=1, ADCS=1(FRC 클럭), ADFRM0=1(Right justified)
 */
void ADC_Init(void) {
  ADREF = 0x00;  // VREF+ -> VDD, VREF- -> VSS
  ADCON0 = 0x94; // ADON=1, ADCS=1(FRC), ADFRM=1(Right justified)
  ADCON1 = 0x00;
  ADCON2 = 0x00;
  ADCON3 = 0x00;
  ADACT = 0x00;
}

/**
 * @brief 특정 아날로그 채널의 10비트 ADC 값 판독 함수
 * @param channel 읽고자 하는 아날로그 채널 (AN0 = 0x00)
 * @return 10비트 ADC 변환 결과값 (0 ~ 1023)
 */
unsigned int ADC_Read(unsigned char channel) {
  ADPCH = channel & 0x3F; // 채널 선택
  unsigned long sum = 0;
  
  for (unsigned char i = 0; i < 16; i++) {
    __delay_us(5);          // 채널 전환 및 샘플링 커패시터 충전 지연
    ADCON0bits.ADGO = 1;    // 변환 기동
    while (ADCON0bits.ADGO)
      ; // 변환 완료 대기
    sum += ((unsigned int)ADRESH << 8) | ADRESL;
  }
  
  return (unsigned int)(sum >> 4); // 16회 누적 합산 평균값 (4비트 우측 시프트)
}

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

const unsigned int FOOT_ADC_THRES_LO[6] = {310, 465, 620, 713, 775, 806};
const unsigned int FOOT_ADC_THRES_HI[6] = {403, 651, 775, 837, 868, 868};

static unsigned int last_foot_adc_val = 0;

static unsigned int feedback_duration_us = 500;
static unsigned int last_feedback_adc_val = 0;

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
    last_feedback_adc_val = ADC_Read(0);
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

  if (PWM7CONbits.PWM7OUT == 0) {
    last_foot_adc_val = ADC_Read(0);
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

/**
 * @brief 키 매트릭스 6개 스위치 스캔 함수
 * - FND 세그먼트 소등 상태(데드 타임)에서 수행하여 디스플레이 왜곡 방지
 * - RB0 ~ RB5 핀을 순차적으로 LOW로 구동한 뒤 RC7(KEY IN) 값을 읽어 눌림 확인
 * @return 눌린 키 번호 (1: START, 2: LEVEL, 3: TIME_UP, 4: TIME_DN, 5:
 * LEVEL_UP, 6: LEVEL_DN, 0: 없음)
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

  // 3. PORTB의 RB0~RB5를 일시적으로 입력 모드로 전환 (내부 풀업은 WPUB=0x00으로
  // 비활성 유지)
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

void UART_Init(void) {
  // EUSART1 제어 레지스터 초기화
  TX1STAbits.TXEN = 1; // Transmit Enable
  // 비동기 9600bps 설정 (Fosc=32MHz, BRGH=1, BRG16=1)
  TX1STA = 0x24;   // TXEN=1, BRGH=1
  BAUD1CON = 0x08; // BRG16=1
  SP1BRG = 832;    // 9600 bps @ 32MHz
  SP1BRGH = (832 >> 8);
  RC1STA = 0x90; // SPEN=1, CREN=1 (수신 활성화)

  PIE3bits.TXIE = 0; // 초기 상태 인터럽트 비활성
  PIE3bits.RCIE = 1; // UART RX 수신 인터럽트 활성화
}

void putch(char c) {
  unsigned char next_head = (tx_head + 1) & (TX_BUF_SIZE - 1);
  // 버퍼가 꽉 찼으면 디스플레이 블로킹을 막기 위해 무시(Drop)
  if (next_head != tx_tail) {
    tx_buffer[tx_head] = c;
    tx_head = next_head;
    PIE3bits.TXIE = 1; // TX 인터럽트 활성화하여 송신 개시
  }
}

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
 * @brief NEC IR 하강 엣지 전용 수신 인터럽트 서비스 루틴 (ISR)
 *
 * RA4의 하강 엣지 발생마다 Timer1 경과값으로 두 하강 엣지 간의 시간 간격을
 * 계측하여 디코딩. 상승 엣지의 뭉개짐(캐패시터 등)이나 링잉 노이즈 영향 없이
 * 날카로운 하강 엣지만으로 작동.
 *
 * 하강 엣지 간격 판정 기준 (단위: μs):
 *   리더 코드 구간 (리더마크 9ms + 리더스페이스 4.5ms) = 13.5ms (13500μs) →
 * 11500~15500 데이터 비트 "0" (마크 0.56ms + 스페이스 0.56ms) = 1.12ms (1125μs)
 * → 800~1500 데이터 비트 "1" (마크 0.56ms + 스페이스 1.69ms) = 2.25ms (2250μs)
 * → 1500~2700 총 33개의 하강 엣지가 들어올 때 32비트 데이터 수신 완료.
 */
void __interrupt() isr(void) {
  // --- 비동기 UART TX 인터럽트 처리 ---
  if (PIE3bits.TXIE && PIR3bits.TXIF) {
    if (tx_head != tx_tail) {
      TX1REG = tx_buffer[tx_tail];
      tx_tail = (tx_tail + 1) & (TX_BUF_SIZE - 1);
    } else {
      PIE3bits.TXIE = 0; // 보낼 데이터 없으면 인터럽트 끄기
    }
  }

  // --- 비동기 UART RX 인터럽트 처리 ---
  if (PIE3bits.RCIE && PIR3bits.RCIF) {
    rx_cmd = RC1REG;
    rx_ready = 1;
    // 오버런 에러 발생 시 수신 복구 처리
    if (RC1STAbits.OERR) {
      RC1STAbits.CREN = 0;
      RC1STAbits.CREN = 1;
    }
  }

  if (IOCAFbits.IOCAF4) {
    IOCAFbits.IOCAF4 = 0;
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
    // 단, 오버플로우 상태(t=30000)일 때는 노이즈 필터를 우회해야 함
    if (t < 400) {
      return; // TMR1을 리셋하지 않고 그대로 무시
    }

    TMR1H =
        0; // 유효한 엣지일 때만 다음 구간 계측 시작 (16비트 쓰기 버퍼 활성화)
    TMR1L = 0;

    // 25ms (25000us) 이상의 긴 유휴 간격이 감지되면 상태 머신을 IDLE로 강제
    // 동기화
    if (t > 25000) {
      ir_state = 1; // 새로운 리더의 첫 엣지로 계승
      return;       // 이 엣지 자체는 리더 시간 판정을 거치지 않고 즉시 리턴
    }

    switch (ir_state) {
    case 0: // IDLE: 최초 하강 엣지 수신 대기 (리더 마크의 시작점)
      ir_state = 1;
      break;

    case 1: // LEADER: 리더 코드 판정 (규격 13.5ms = 13500μs, 허용 마진 대폭
            // 확장: 6~22ms)
      if (t > 6000 && t < 22000) {
        ir_raw_data = 0;
        ir_bit_cnt = 0;
        ir_state = 2;
        ir_leader_detected = 1; // 리더코드 수신 성공 플래그
      } else {
        ir_err_code = 1; // 리더코드 오류 (E1)
        ir_err_t = t;
        ir_state = 3; // ERROR_WAIT 상태로 가 잔여 엣지 유입 무시
      }
      break;

    case 2: // DATA: 데이터 비트 디코딩 (비트 0: 1125μs / 비트 1: 2250μs)
      if (t >= 400 && t < 1500) {
        // 데이터 비트 '0' 수신
        ir_raw_data >>= 1;
      } else if (t >= 1500 && t < 3500) {
        // 데이터 비트 '1' 수신
        ir_raw_data >>= 1;
        ir_raw_data |= 0x80000000UL;
      } else {
        ir_err_code = 2; // 데이터 구간 오류 (E2)
        ir_err_t = t;
        ir_state = 3; // ERROR_WAIT 상태로 가 잔여 엣지 유입 무시
        break;
      }

      if (++ir_bit_cnt >= 32) {
        // 32비트 수신 완료: 주소 및 커맨드 무결성 검증
        unsigned int addr = (unsigned int)(ir_raw_data & 0xFFFF);
        unsigned char cmd = (unsigned char)((ir_raw_data >> 16) & 0xFF);
        unsigned char cmd_inv = (unsigned char)((ir_raw_data >> 24) & 0xFF);
        if (addr == IR_DEVICE_ADDR && (unsigned char)(cmd ^ cmd_inv) == 0xFF) {
          ir_cmd_value = cmd;
          ir_cmd_ready = 1;
          ir_state = 0; // 성공 완료 시 즉시 IDLE 복귀
        } else {
          ir_err_code = 5; // 주소 혹은 체크섬 에러 (E5)
          ir_err_t = addr;
          ir_state = 3; // 에러 대기
        }
      }
      break;

    case 3: // ERROR_WAIT: 잔여 노이즈 및 데이터 엣지 차단 대기
      // 엣지 간 간격 t가 프레임 차단 대기 임계값(8ms)을 초과할 때 비소로
      // IDLE/LEADER 대기 상태로 복귀
      if (t >= 8000) {
        ir_state =
            1; // 들어온 첫 번째 엣지를 새로운 프레임 리더의 첫 엣지로 계승
      }
      break;
    }
  }
}

void main(void) {
  // 시스템 초기화
  System_Init();

  // 히터 PWM(PWM6 & PWM7) 초기화
  Heater_PWM_Init();

  // 피에조 부저용 PWM(CCP1) 초기화
  Buzzer_PWM_Init();

  // 비동기 UART 초기화 (9600bps)
  UART_Init();

  // NEC IR 리모콘 수신기 초기화
  IR_Init();

  // UV LED 비활성화 (Active LOW이므로 HIGH(1) 상태로 꺼둠)
  LATA5 = 1;

  // FND 및 LED DRV 초기 상태 OFF (Active LOW이므로 HIGH(1))
  LATA1 = 1;
  LATA2 = 1;
  LATA3 = 1;

  // 기동 메시지 (비동기 송신 테스트)
  printf("Deep Heater Board is ALIVE (Async UART)!\r\n");

  // ===================================================================
  // 부저 하드웨어 진단 테스트 (부팅 직후 1회 실행)
  // ===================================================================

  // [자가진단] CCP1 하드웨어 PWM → 11번 핀(RC0) 부저 출력 확인
  printf("[BOOT] Buzzer Diagnostic Start...\r\n");
  CCPTMRS0 = 0x01; // CCP1을 TMR2에 연결 (RC0PPS=0x09는 Buzzer_PWM_Init에서 고정)
  CCP1CON = 0x8F;  // CCP1 활성화
  T2CON = 0xE0;    // Timer2 활성화
  PR2 = 45;        // ~2.7kHz
  CCPR1L = 23;     // 50% 듀티
  __delay_ms(200); // 200ms 기동음
  CCPR1L = 0;      // 무음
  CCP1CON = 0x00;  // CCP1 비활성화 (RC0PPS는 0x09 고정 유지)
  T2CON = 0x00;    // Timer2 비활성화
  LATC0 = 0;       // 출력 강제 소등
  printf("[BOOT] Buzzer Diagnostic Done\r\n");
  __delay_ms(100);

  // === 부팅 직후 핵심 레지스터 덤프 (UART 디버그) ===
  printf("=== REG DUMP ===\r\n");
  printf("CCP1=%02X T2=%02X PR2=%u D=%u\r\n", CCP1CON, T2CON, PR2, CCPR1L);
  printf("TMS0=%02X TMS1=%02X\r\n", CCPTMRS0, CCPTMRS1);
  printf("T2CLK=%02X T4CLK=%02X\r\n", T2CLKCON, T4CLKCON);
  __delay_ms(150); // TX 버퍼 비움 대기 (128바이트 @9600bps ≈ 130ms)
  printf("PWM6=%02X T4=%02X PR4=%u\r\n", PWM6CON, T4CON, PR4);
  printf("RC0=%02X RC6=%02X\r\n", RC0PPS, RC6PPS);
  printf("TRISC=%02X LATC=%02X ANS=%02X\r\n", TRISC, LATC, ANSELC);
  printf("OSCEN=%02X STAT=%02X\r\n", OSCEN, OSCSTAT);
  printf("===END===\r\n");

  // --- EEPROM 설정값 로드 및 유효성 검사 ---
  pwm_setting_value = eeprom_read(0x00);
  if (pwm_setting_value < 20 || pwm_setting_value > 65) {
    pwm_setting_value = 45;
  }

  // 타이머 및 LED 레벨 데이터 제어 변수 선언
  unsigned char minute = 29;
  char second = 0; // 대기 시 00초 상태
  unsigned int tick_ms = 0;
  unsigned char level_led_data = 0xFF; // 개별 LED 데이터 (모두 OFF)

  // 디바운스 필터 및 상태 관리를 위한 변수 선언
  unsigned char key_db_count[6] = {0, 0, 0, 0, 0, 0};
  unsigned int key_hold_count[6] = {0, 0, 0, 0, 0, 0};
  unsigned int key_repeat_count[6] = {0, 0, 0, 0, 0, 0};
  unsigned char key_active[6] = {0, 0, 0, 0, 0, 0};
  unsigned char raw_key = 0;
  unsigned int blink_ticks = 0;  // 대기 중 점멸 주기 카운터 (루프당 약 3.2ms)
  unsigned char blink_state = 1; // 1: 점등, 0: 소등

  // --- 설정 모드 진입 판별 (SW1 + SW2 동시 누름 상태로 부팅) ---
  TRISCbits.TRISC7 = 0;
  LATCbits.LATC7 = 0; // 스캔 핀 강제 LOW
  TRISB = 0x3F;       // PORTB 입력 모드
  __delay_us(10);
  if ((PORTB & 0x03) == 0x00) { // SW1(RB0)과 SW2(RB1)가 모두 LOW(눌림)
    setting_mode = 1;
    printf("Entering Setting Mode...\r\n");
    printf("PWM Set: %d\r\n", pwm_setting_value);

    // 진입 알림음 (특별 부저음)
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
  TRISCbits.TRISC7 = 1; // 포트 원상복구

  // 초기 상태 반영하여 PWM 차단 보장
  Update_Heater_PWM();

  while (1) {
    // --- UART CLI 디버깅 수신 명령어 처리 ---
    if (rx_ready) {
      rx_ready = 0;
      char debug_cmd = rx_cmd;
      printf("CLI Cmd: %c\r\n", debug_cmd);

      if (debug_cmd == 'H' || debug_cmd == 'h') {
        debug_toggle_mode = 0;
        RC0PPS = 0x00;         // 부저 PPS 해제 (일반 GPIO)
        ODCONCbits.ODCC0 = 0;  // 오픈 드레인 강제 비활성화 (Push-Pull 강제)
        SLRCONCbits.SLRC0 = 0; // Slew Rate 제한 해제 (드라이브 능력 최대화)
        LATCbits.LATC0 = 1;    // HIGH 출력 고정
        printf("Buzzer (RC0) -> HIGH (5V) [LATC=%02X TRISC=%02X RC0PPS=%02X "
               "ODCONC=%02X SLRCONC=%02X]\r\n",
               LATC, TRISC, RC0PPS, ODCONC, SLRCONC);
      } else if (debug_cmd == 'L' || debug_cmd == 'l') {
        debug_toggle_mode = 0;
        RC0PPS = 0x00;         // 부저 PPS 해제 (일반 GPIO)
        ODCONCbits.ODCC0 = 0;  // 오픈 드레인 강제 비활성화 (Push-Pull 강제)
        SLRCONCbits.SLRC0 = 0; // Slew Rate 제한 해제 (드라이브 능력 최대화)
        LATCbits.LATC0 = 0;    // LOW 출력 고정
        printf("Buzzer (RC0) -> LOW (0V) [LATC=%02X TRISC=%02X RC0PPS=%02X "
               "ODCONC=%02X SLRCONC=%02X]\r\n",
               LATC, TRISC, RC0PPS, ODCONC, SLRCONC);
      } else if (debug_cmd == 'T' || debug_cmd == 't') {
        debug_toggle_mode = !debug_toggle_mode;
        if (debug_toggle_mode) {
          RC0PPS = 0x00;         // 부저 PPS 해제 (일반 GPIO)
          ODCONCbits.ODCC0 = 0;  // 오픈 드레인 강제 비활성화
          SLRCONCbits.SLRC0 = 0; // Slew Rate 제한 해제
          LATCbits.LATC0 = 1;    // HIGH 상태에서 토글 시작
          printf("Buzzer (RC0) -> 1s Toggle Mode ON [LATC=%02X TRISC=%02X "
                 "RC0PPS=%02X ODCONC=%02X]\r\n",
                 LATC, TRISC, RC0PPS, ODCONC);
        } else {
          LATCbits.LATC0 = 0;
          printf("Buzzer (RC0) -> 1s Toggle Mode OFF (LOW) [LATC=%02X "
                 "TRISC=%02X RC0PPS=%02X ODCONC=%02X]\r\n",
                 LATC, TRISC, RC0PPS, ODCONC);
        }
      } else if (debug_cmd == 'P' || debug_cmd == 'p') {
        debug_toggle_mode = 0;
        CCP1CON = 0x8F;
        T2CON = 0xE0;
        RC0PPS = 0x09;         // CCP1 출력을 RC0에 할당 (PWM)
        PR2 = 45;              // 기본 2.7kHz
        CCPR1L = 23;           // 50% 듀티
        ODCONCbits.ODCC0 = 0;  // 오픈 드레인 강제 비활성화
        SLRCONCbits.SLRC0 = 0; // Slew Rate 제한 해제
        printf("Buzzer (RC0) -> PWM Continuous Sound ON (2.7kHz) [ODCONC=%02X "
               "SLRCONC=%02X]\r\n",
               ODCONC, SLRCONC);
      } else if (debug_cmd == 'R' || debug_cmd == 'r') {
        printf("=== REG DUMP ===\r\n");
        printf("LATC=%02X TRISC=%02X RC0PPS=%02X ODCONC=%02X SLRCONC=%02X\r\n",
               LATC, TRISC, RC0PPS, ODCONC, SLRCONC);
        printf("CCP1CON=%02X T2CON=%02X PR2=%u CCPR1L=%u\r\n", CCP1CON, T2CON,
               PR2, CCPR1L);
        printf("is_running=%u setting_mode=%u debug_toggle=%u\r\n", is_running,
               setting_mode, debug_toggle_mode);
        printf("FOOT_SEN (AN0) ADC: %u\r\n", ADC_Read(0));
      } else if (debug_cmd == 'S' || debug_cmd == 's') {
        debug_toggle_mode = 0;
        RC0PPS = 0x00;
        LATC0 = 0;
        CCP1CON = 0x00; // CCP1 비활성화
        CCPR1L = 0;
        ODCONCbits.ODCC0 = 0;  // 오픈 드레인 강제 비활성화
        SLRCONCbits.SLRC0 = 0; // Slew Rate 제한 해제
        printf("Buzzer (RC0) -> System Restored, Buzzer Silent [ODCONC=%02X "
               "SLRCONC=%02X]\r\n",
               ODCONC, SLRCONC);
      }
    }

    // --- IR 리모콘 디버깅 상태 비동기 UART 출력 (FND 깜빡임 없음) ---
    if (ir_leader_detected) {
      ir_leader_detected = 0;
      printf("IR LDR\r\n");
    }
    if (ir_err_code) {
      printf("IR ERR %d, %u\r\n", ir_err_code, ir_err_t);
      ir_err_code = 0;
    }

    // UV LED 자동 연동 제어 (작동 상태일 때 LOW(0)로 활성화, 대기/정지 시
    // HIGH(1)로 비활성화)
    LATA5 = is_running ? 0 : 1;

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
    __delay_us(
        50); // 하드웨어 PNP TR Turn-off 스위칭 지연 시간 확보 (Dead Time)
    if (setting_mode || blink_state) {
      unsigned char disp_val = setting_mode ? pwm_setting_value : minute;
      LATB = FND_Font[disp_val % 10]; // 일의 자리 데이터 선출력
      LATA1 = 0;                      // Digit 1 ON (Active LOW)
    }
    __delay_ms(1); // 1ms 지연
    LATA1 = 1;     // 소등

    // --- 2단계: 십의 자리 출력 (Digit 2 - 하드웨어 좌측 디스플레이) ---
    LATB = 0xFF; // 고스트 방지 (모든 세그먼트 소등)
    LATA1 = 1;   // Digit 1 OFF (우측 디지트 차단)
    LATA3 = 1;   // LED DRV OFF (개별 LED 차단)
    __delay_us(
        50); // 하드웨어 PNP TR Turn-off 스위칭 지연 시간 확보 (Dead Time)
    if (setting_mode || blink_state) {
      unsigned char disp_val = setting_mode ? pwm_setting_value : minute;
      LATB = FND_Font[disp_val / 10]; // 십의 자리 데이터 선출력
      LATA2 = 0;                      // Digit 2 ON (Active LOW)
    }
    __delay_ms(1); // 1ms 지연
    LATA2 = 1;     // 소등

    // --- 3단계: 개별 LED 출력 (LED DRV - Level Lo 등 상태 표시) ---
    LATB = 0xFF; // 고스트 방지 (모든 세그먼트 소등)
    LATA1 = 1;   // Digit 1 OFF (우측 디지트 차단)
    LATA2 = 1;   // Digit 2 OFF (좌측 디지트 차단)
    __delay_us(
        50); // 하드웨어 PNP TR Turn-off 스위칭 지연 시간 확보 (Dead Time)
    if (!setting_mode && is_running) {
      // 작동 중일 때만 선택된 레벨 LED 및 LV_Lo/Hi 켜기
      unsigned char level_mask = 0;
      // 1~6 레벨에 맞춰 LED1~6을 채워나가는(바 그래프) 방식으로 마스크 비트
      // 켜기
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
      // blink_state == 0이면 LATB=0xFF, LATA3=1 (소등 상태) 그대로 유지
    }
    __delay_ms(1); // 1ms 지연
    LATA3 = 1;     // LED DRV OFF

    if (!setting_mode) {
      // --- 백그라운드 1초 타이머 연동 (다운 카운트) ---
      tick_ms += 3; // 1루프 주기(약 3.2ms) 기준 3ms 누적
      if (tick_ms >= 1000) {
        tick_ms -= 1000;

        // 디버그 1초 토글 모드 작동
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

          // 0분 0초 완료 시 자동 정지 처리
          if (minute == 0 && second == 0) {
            is_running = 0;
            second = 0;
            Update_Heater_PWM();

            // 완료음 발생: 약 1.5초 동안 (약 450회 루프)
            CCP1CON = 0x8F;
            T2CON = 0xE0;
            RC0PPS = 0x09;
            PR2 = 45;        // 기본 피치 (2.7kHz)
            CCPR1L = 23;     // 50% 듀티
            buzzer_mode = 0; // 일반 부저
            buzzer_stage = 1;
            buzzer_timer = 450;
            buzzer_init_value = 450;
          } else {
            // 동작이 계속 실행 중일 때 1초마다 A/D 센서 리드 피드백 업데이트 수행
            unsigned int max_duration_us = 0;
            switch (current_level) {
            case 1: max_duration_us = is_hi_mode ? 750 : 500; break;
            case 2: max_duration_us = is_hi_mode ? 1500 : 1000; break;
            case 3: max_duration_us = is_hi_mode ? 2250 : 1500; break;
            case 4: max_duration_us = is_hi_mode ? 3000 : 2000; break;
            case 5: max_duration_us = is_hi_mode ? 3750 : 2500; break;
            case 6: max_duration_us = is_hi_mode ? 4500 : 3000; break;
            default: max_duration_us = 0; break;
            }

            if (max_duration_us > 500) {
              unsigned int cur_adc = last_foot_adc_val; // PWM7OUT이 0일 때 읽어둔 캐싱 ADC 값
              // 1초 전보다 20% 이상 증가 시 출력을 20% 상승
              if (cur_adc >= (unsigned int)((unsigned long)last_feedback_adc_val * 12 / 10)) {
                feedback_duration_us = (unsigned int)((unsigned long)feedback_duration_us * 12 / 10);
                if (feedback_duration_us > max_duration_us) {
                  feedback_duration_us = max_duration_us;
                }
              }
              // 1초 전보다 20% 이상 감소 시 출력을 20% 감소
              else if (cur_adc < (unsigned int)((unsigned long)last_feedback_adc_val * 8 / 10)) {
                feedback_duration_us = (unsigned int)((unsigned long)feedback_duration_us * 8 / 10);
                if (feedback_duration_us < 500) {
                  feedback_duration_us = 500;
                }
              }

              // 다음 비교를 위해 ADC 값 기록
              last_feedback_adc_val = cur_adc;
            } else {
              feedback_duration_us = 500;
            }

            // 1초 단위 계산값을 하드웨정에 즉시 반영
            Update_Heater_PWM();
          }
        }
      }

      // --- 키 매트릭스 스캔 및 디바운스 / 연속 조작 처리 ---
      // CLI 테스트 모드 동작 중(디버그 토글 활성화, 부저 핀 강제 제어 등)이
      // 아닐 때만 키 매트릭스 스캔 동작을 수행
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
                    second = 0;
                    tick_ms = 0;
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
                    second = 0;
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
                  if (minute < 30) {
                    minute++;
                    do_buzz = 1;
                  }
                } else if (k == 3) { // SW4 (TIME_DN) - 최소(0분) 아닐 때만
                  if (minute > 0) {
                    minute--;
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
                    if (minute < 30)
                      minute++;
                  } else if (k == 3) {
                    if (minute > 0)
                      minute--;
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
    } // end of if(!setting_mode)

    // --- NEC IR 리모콘 커맨드 처리 ---
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
              second = 0;
              tick_ms = 0;
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
              second = 0;
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
          if (minute < 30) {
            minute++;
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
          if (minute > 0) {
            minute--;
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

    // --- 비동기 부저 소프트웨어 타이머 처리 ---
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
          PR2 = 45;      // 기본 중간음 복원
        }
      } else {
        // 현재 단일 단계 내에서 비례 감쇄 연산 (감쇄음 효과)
        CCPR1L = (unsigned char)(((unsigned long)buzzer_timer * max_duty) /
                                 buzzer_init_value);
      }
    }

    // dbg_disp_timer 감쇄 및 교차 표시 로직 제거 (FND 항시 시간 출력 보장)
    Update_Heater_PWM(); // 런타임 간섭 대응을 위해 매 루프마다 방향성 및 PPS
                         // 잠금 갱신
  }
}
