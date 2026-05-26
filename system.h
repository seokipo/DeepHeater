#ifndef SYSTEM_H
#define SYSTEM_H

#include <xc.h>

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
