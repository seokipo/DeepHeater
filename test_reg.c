#include <xc.h>
void test(void) {
    PIE3bits.TXIE = 1;
    PIR3bits.TXIF = 0;
    TX1REG = 0;
}
void main(void) {}
