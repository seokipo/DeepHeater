#ifndef MAIN_H
#define MAIN_H

#define _XTAL_FREQ 32000000

// 시스템 전역 동작 상태 변수 extern 선언
extern volatile unsigned char setting_mode;
extern volatile unsigned char is_running;
extern volatile unsigned char current_level;
extern volatile unsigned char is_hi_mode;
extern volatile unsigned char pwm_setting_value;

#endif // MAIN_H
