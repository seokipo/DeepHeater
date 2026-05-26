#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// Define _XTAL_FREQ for __delay_ms and __delay_us
#define _XTAL_FREQ 16000000 // Based on OSCCON = 0x78 (16MHz)

// Configuration Bits
#pragma config FOSC = INTOSC    // Oscillator Selection
#pragma config WDTE = OFF       // Watchdog Timer Enable
#pragma config PWRTE = OFF      // Power-up Timer Enable
#pragma config MCLRE = OFF      // MCLR Pin Function Select
#pragma config CP = OFF         // Flash Program Memory Code Protection
#pragma config CPD = OFF        // Data Memory Code Protection
#pragma config BOREN = ON       // Brown-out Reset Enable
#pragma config CLKOUTEN = OFF   // Clock Out Enable
#pragma config IESO = ON        // Internal/External Switchover
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable
#pragma config WRT = OFF        // Flash Memory Self-Write Protection
#pragma config VCAPEN = OFF     // Voltage Regulator Capacitor Enable
#pragma config PLLEN = ON       // PLL Enable
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable
#pragma config BORV = LO        // Brown-out Reset Voltage Selection
#pragma config LVP = OFF        // Low-Voltage Programming Enable

#include "../modules/ds1307.h"
#include "../modules/lcd.h"
#include "../modules/switch.h"
#include "../modules/lunar.h"
#include "../modules/remote.h"

// States for Time Setting
typedef enum {
    MODE_NORMAL,
    MODE_SET_YEAR,
    MODE_SET_MONTH,
    MODE_SET_DATE,
    MODE_SET_HOUR,
    MODE_SET_MIN
} SystemMode;

// Interrupt Service Routine
volatile uint16_t remoteIntCount = 0; // Debug counter
bool systemPower = true;

void __interrupt() ISR(void) {
    if (CCP2IE && CCP2IF) {
        remoteIntCount++;
        extern void Remote_ProcessISR(void);
        Remote_ProcessISR();
        CCP2IF = 0;
    }
}

void main(void) {
    OSCCON = 0x78; // 16MHz

    TRISCbits.TRISC3 = 1; // SCL
    TRISCbits.TRISC4 = 1; // SDA
    
    SW_Init(); // Initialize Switches (RB0, RB5, RB6, RB7)
    RTC_Init();
    LCD_Init();
    Remote_Init();
    
    PEIE = 1;
    GIE = 1;
    
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_String("  Kipo F/A System   ");
    
    __delay_ms(2000); 
    LCD_Backlight(1); // Standard On/Off 1:On

    RTC_Time currentTime;
    char timeStr[32]; // Increased from 21 to 32 to prevent stack overflow
    
    SystemMode mode = MODE_NORMAL;
    uint8_t blinkTimer = 0;
    bool blinkState = true;
    uint16_t timeoutCounter = 0;
    
    uint8_t lastSec = 99;
    uint8_t lastDate = 99;
    LunarDate lunarCache = {0, 0, false};
    uint8_t dowCache = 0; // Initialize clearly
    
    uint8_t lcdPage = 1; 
    uint8_t lastLcdPage = 0; // Trigger full refresh on start
    bool lastSystemPower = false;
    uint16_t forceUpdate = 0;
    
    static uint16_t remoDisplayTimer = 0;
    static char lastKeyName[16] = "";
    static uint16_t powerDebounce = 0;

    RTC_GetTime(&currentTime);

    while(1) {
        KeyCode key = SW_Scan();
        uint8_t remoKey = Remote_GetKey();
        
        if (powerDebounce > 0) powerDebounce--;
        if (remoDisplayTimer > 0) remoDisplayTimer--;
        if (forceUpdate > 0) forceUpdate--;

        // 0. Page Switch Logic
        if (key == KEY_PAGE) {
            lcdPage++;
            if (lcdPage > 2) lcdPage = 1;
        }

        if (remoKey != 0) {
            const char* keyName = "Unknown";
            switch(remoKey) {
                case KEY_REMO_POWER: keyName = "POWER"; break;
                case KEY_REMO_MODE:  keyName = "MODE "; break;
                case KEY_REMO_WIND:  keyName = "WIND "; break;
                case KEY_REMO_CLEAN: keyName = "CLEAN"; break;
                case KEY_REMO_LIGHT: keyName = "LIGHT"; break;
                case KEY_REMO_SLEEP: keyName = "SLEEP"; break;
            }
            sprintf(lastKeyName, "%-7s", keyName);
            remoDisplayTimer = 100;

            if (remoKey == KEY_REMO_POWER && powerDebounce == 0) {
                systemPower = !systemPower;
                powerDebounce = 10;
                if (systemPower) LCD_Backlight(1); else LCD_Backlight(0);
            }
        }

        // 1. Full Refresh Detection (Page change or Power change)
        bool fullRefreshTask = false;
        if (lcdPage != lastLcdPage || systemPower != lastSystemPower) {
            LCD_Clear();
            if (systemPower) {
                if (lcdPage == 1) {
                    LCD_SetCursor(0, 0); 
                    LCD_String("  Kipo F/A System   "); // 2 spaces prefix
                } else {
                    LCD_SetCursor(0, 0); 
                    LCD_String(" Remocon Tester  2/2"); // 1 space prefix, fits 20 chars
                }
            } else {
                LCD_SetCursor(0, 0); LCD_String("       STANDBY      ");
                LCD_SetCursor(1, 0); LCD_String("   Push POWER key   ");
            }
            lastLcdPage = lcdPage;
            lastSystemPower = systemPower;
            fullRefreshTask = true;
            lastSec = 99; // Force dynamic update
        }

        if (!systemPower) {
            __delay_ms(20); 
            continue; 
        }

        if (mode == MODE_NORMAL) {
            RTC_GetTime(&currentTime);
            
            // 2. Dynamic Update (Time, Keys, Lunar)
            // Update every second, or if forced (by key press or page change)
            if (currentTime.sec != lastSec || fullRefreshTask || (remoKey != 0) || forceUpdate == 0) {
                lastSec = currentTime.sec;
                if (forceUpdate == 0) forceUpdate = 250; 

                // Line 1: Time (ISO)
                sprintf(timeStr, "20%02d-%02d-%02d  %02d:%02d:%02d ", 
                        currentTime.year, currentTime.month, currentTime.date,
                        currentTime.hour, currentTime.min, currentTime.sec);
                LCD_SetCursor(1, 0);
                LCD_String(timeStr);

                if (lcdPage == 1) {
                    // Line 2: Lunar & DOW
                    if (currentTime.date != lastDate || fullRefreshTask) {
                        lastDate = currentTime.date;
                        SolarToLunar(currentTime.year, currentTime.month, currentTime.date, &lunarCache);
                        dowCache = GetDayOfWeek(currentTime.year, currentTime.month, currentTime.date);
                    }
                    const char* dowStr[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                    char lunarStr[32]; // Increased from 21 to 32
                    sprintf(lunarStr, "[%s] Lunar %02d-%02d   ", dowStr[dowCache % 7], lunarCache.month, lunarCache.date);
                    LCD_SetCursor(2, 0);
                    LCD_String(lunarStr);
                    
                    // Line 3: Info
                    LCD_SetCursor(3, 0);
                    LCD_String("Page: 1/2 (SW0)    ");
                } 
                else {
                    // Page 2 Line 2: Protocol Name (Full name support)
                    LCD_SetCursor(2, 0);
                    char chipInfo[32];
                    sprintf(chipInfo, "  Chip: %-12s", Remote_GetProtocolName());
                    LCD_String(chipInfo);

                    // Page 2 Line 3: Remocon Data
                    uint32_t fullData = Remote_GetRawData();
                    Remote_Protocol currentP = Remote_GetCurrentProtocol();
                    uint16_t outId = 0;
                    uint8_t outCmd = 0;
                    
                    if (currentP == PROTOCOL_SHARP) {
                        // Sharp: 5 bit Addr (0-4), 8 bit Cmd (5-12), 2 bit Exp (13-14)
                        outId = (uint16_t)(fullData & 0x1F);
                        outCmd = (uint8_t)((fullData >> 5) & 0xFF);
                    } else {
                        // NEC/Samsung: 16 bit Addr (0-15), 8 bit Cmd (16-23)
                        outId = (uint16_t)(fullData & 0xFFFF);
                        outCmd = (uint8_t)((fullData >> 16) & 0xFF);
                    }

                    char keyInfo[32];
                    if (remoDisplayTimer > 0) {
                        sprintf(keyInfo, "Id:%04X C:%02X %s", outId, outCmd, lastKeyName);
                    } else {
                        sprintf(keyInfo, "Id:%04X C:%02X         ", outId, outCmd);
                    }
                    LCD_SetCursor(3, 0);
                    LCD_String(keyInfo);
                }
            }
            
            if (key == KEY_SET_LONG) {
                mode = MODE_SET_YEAR;
                timeoutCounter = 0;
                LCD_Clear();
                LCD_SetCursor(0, 0); LCD_String("Setup Time...");
            }
        } 
        else {
            // SETTING MODE - Keep it simple, it's temporary
            timeoutCounter++;
            if (timeoutCounter > 500) { mode = MODE_NORMAL; lastLcdPage = 0; }
            if (key != KEY_NONE) timeoutCounter = 0;

            blinkTimer++;
            if (blinkTimer > 10) { blinkState = !blinkState; blinkTimer = 0; }

            LCD_SetCursor(1, 0);
            char yearStr[5], monthStr[3], dateStr[3], hourStr[3], minStr[3];
            if (mode == MODE_SET_YEAR && !blinkState) sprintf(yearStr, "  "); else sprintf(yearStr, "%02d", currentTime.year);
            if (mode == MODE_SET_MONTH && !blinkState) sprintf(monthStr, "  "); else sprintf(monthStr, "%02d", currentTime.month);
            if (mode == MODE_SET_DATE && !blinkState) sprintf(dateStr, "  "); else sprintf(dateStr, "%02d", currentTime.date);
            if (mode == MODE_SET_HOUR && !blinkState) sprintf(hourStr, "  "); else sprintf(hourStr, "%02d", currentTime.hour);
            if (mode == MODE_SET_MIN && !blinkState) sprintf(minStr, "  "); else sprintf(minStr, "%02d", currentTime.min);
            
            sprintf(timeStr, "20%s-%s-%s %s:%s    ", yearStr, monthStr, dateStr, hourStr, minStr);
            LCD_String(timeStr);

            switch(key) {
                case KEY_UP:
                    if (mode == MODE_SET_YEAR)  { currentTime.year++; if (currentTime.year > 99) currentTime.year = 0; }
                    if (mode == MODE_SET_MONTH) { currentTime.month++; if (currentTime.month > 12) currentTime.month = 1; }
                    if (mode == MODE_SET_DATE)  { currentTime.date++; if (currentTime.date > 31) currentTime.date = 1; }
                    if (mode == MODE_SET_HOUR)  { currentTime.hour++; if (currentTime.hour > 23) currentTime.hour = 0; }
                    if (mode == MODE_SET_MIN)   { currentTime.min++; if (currentTime.min > 59) currentTime.min = 0; }
                    break;
                case KEY_DOWN:
                    if (mode == MODE_SET_YEAR)  { if (currentTime.year == 0) currentTime.year = 99; else currentTime.year--; }
                    if (mode == MODE_SET_MONTH) { if (currentTime.month == 1) currentTime.month = 12; else currentTime.month--; }
                    if (mode == MODE_SET_DATE)  { if (currentTime.date == 1) currentTime.date = 31; else currentTime.date--; }
                    if (mode == MODE_SET_HOUR)  { if (currentTime.hour == 0) currentTime.hour = 23; else currentTime.hour--; }
                    if (mode == MODE_SET_MIN)   { if (currentTime.min == 0) currentTime.min = 59; else currentTime.min--; }
                    break;
                case KEY_SET:
                    mode++;
                    if (mode > MODE_SET_MIN) {
                        currentTime.sec = 0; RTC_SetTime(&currentTime);
                        mode = MODE_NORMAL; lastLcdPage = 0; // Trigger refresh
                    }
                    break;
                default: break;
            }
        }
        __delay_ms(20);
    }
}
