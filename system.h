#ifndef SYSTEM_H
#define SYSTEM_H

#include <xc.h>

// 하드웨어 핀 제어 매크로 (V0.9.52 회로 스왑 대응)
// 70246 단자 오실장으로 인해 기존 RC2 핀이던 FAN을 RA0로 스왑하여 디지털 출력 제어
#define FAN_LAT   LATAbits.LATA0

// UART 수신(RX) CLI 디버깅용 전역 변수 extern 선언
extern volatile char rx_cmd;
extern volatile unsigned char rx_ready;
extern volatile unsigned char debug_toggle_mode;

// UART 송수신 링버퍼용 변수
#define TX_BUF_SIZE 128
extern volatile char tx_buffer[TX_BUF_SIZE];
extern volatile unsigned char tx_head;
extern volatile unsigned char tx_tail;

// 함수 프로토타입 선언
void System_Init(void);
void ADC_Init(void);
unsigned int ADC_Read(unsigned char channel);
void UART_Init(void);
void putch(char c);

#endif // SYSTEM_H
