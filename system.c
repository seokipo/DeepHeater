#include "system.h"
#include "main.h"

// UART 디버깅용 전역 변수 정의
volatile char rx_cmd = 0;
volatile unsigned char rx_ready = 0;
volatile unsigned char debug_toggle_mode = 0;

// UART 송수신 버퍼 정의
volatile char tx_buffer[TX_BUF_SIZE];
volatile unsigned char tx_head = 0;
volatile unsigned char tx_tail = 0;

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
  // RA0/AN0(FOOT_SEN)만 아날로그 채널로 남겨두고 나머지는 모두 디지털 모드로 설정
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

  // 5. 외부 풀업 저항(R1~R6, R19)이 물리적으로 존재하므로 MCU 내부 풀업 전면 비활성화 (잔상 억제)
  WPUA = 0x00; // 포트 A 내부 풀업 비활성화 (외부 풀업 R17이 존재하므로)
  WPUB = 0x00; // 0b00000000
  WPUC = 0x00; // 0b00000000

  // 6. RA4 입력 레벨을 TTL로 변경 (3.3V 리모콘 입력을 5V MCU에서 안정적으로 HIGH 인식하기 위함)
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
