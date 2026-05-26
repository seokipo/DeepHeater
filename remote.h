#ifndef REMOTE_H
#define REMOTE_H

#include <xc.h>

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

// 리모콘 수신 디버깅 및 제어 변수 extern 선언
extern volatile unsigned char ir_cmd_ready;
extern volatile unsigned char ir_cmd_value;
extern volatile unsigned char ir_leader_detected;
extern volatile unsigned char ir_err_code;
extern volatile unsigned int ir_err_t;

// 함수 프로토타입 선언
void IR_Init(void);
void IR_Decode_Process(void);
void IR_Process_Command(unsigned char *p_minute, char *p_second, unsigned int *p_tick_ms);

#endif // REMOTE_H
