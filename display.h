#ifndef DISPLAY_H
#define DISPLAY_H

#include <xc.h>

// 디스플레이 FND 폰트 데이터 extern 선언
extern const unsigned char FND_Font[16];

// 디스플레이 점멸 및 깜빡임 연동용 변수 extern 선언
extern unsigned int blink_ticks;
extern unsigned char blink_state;

// 함수 프로토타입 선언
void Display_Process(unsigned char display_minute);

#endif // DISPLAY_H
