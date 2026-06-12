#ifndef PWM_CONTROL_H
#define PWM_CONTROL_H

#include <xc.h>

// 풋 센서 ADC 문턱 전압 정보 선언
extern const unsigned int FOOT_ADC_THRES_LO[6];
extern const unsigned int FOOT_ADC_THRES_HI[6];

// 시스템 전역 부저 제어 변수 extern 선언
extern volatile unsigned int buzzer_timer;
extern volatile unsigned int buzzer_init_value;
extern volatile unsigned char buzzer_mode;
extern volatile unsigned char buzzer_stage;
extern volatile unsigned int last_foot_adc_val;

// 함수 프로토타입 선언
void Buzzer_PWM_Init(void);
void Heater_PWM_Init(void);
void Update_Heater_PWM(void);
void Heater_Feedback_Process(void);
void Buzzer_Process(void);
void Fan_Control_Process(void);

#endif // PWM_CONTROL_H
