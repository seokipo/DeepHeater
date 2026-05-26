#ifndef KEY_H
#define KEY_H

#include <xc.h>

// 키 스캔/연속 입력 필터용 변수 extern 선언
extern unsigned char key_db_count[6];
extern unsigned int key_hold_count[6];
extern unsigned int key_repeat_count[6];
extern unsigned char key_active[6];

// 함수 프로토타입 선언
unsigned char Key_Scan(void);
void Key_Process(unsigned char *p_minute, char *p_second, unsigned int *p_tick_ms);

#endif // KEY_H
