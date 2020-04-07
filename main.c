#include "stm32f030.h"

#define QUARTZ_FREQ 18432000
#define PLLMUL 2
#define PLLDIV 5
#define SYS_FREQ (QUARTZ_FREQ * PLLMUL / PLLDIV)
#define UART_BAUD 9600
#define RADIO_CLK 32768

#define RADIO_I2C_ADDR 0x11
#define I2C_READ_ACK 0
#define I2C_READ_NACK 1
#define I2C_READ_ACK_ON_CTS 2

char hex[] = "0123456789ABCDEF";
short adc[8];

int respBytesExpected = 0;

void uartEnable() {
    REG_L(GPIOA_BASE, GPIO_MODER) |= (2 << (9 * 2)); // PA9 alternate function (TX)
    REG_L(GPIOA_BASE, GPIO_AFRH) |= (1 << ((9 - 8) * 4)); // PA9 alternate function 1
    REG_L(GPIOA_BASE, GPIO_MODER) |= (2 << (10 * 2)); // PA10 alternate function (RX)
    REG_L(GPIOA_BASE, GPIO_AFRH) |= (1 << ((10 - 8) * 4)); // PA10 alternate function 1
    REG_L(RCC_BASE, RCC_AHB2ENR) |= (1 << 14); // UART clock
    REG_L(USART_BASE, USART_BRR) |= SYS_FREQ / UART_BAUD;
    REG_L(USART_BASE, USART_CR1) |= 1; // UART enable
    REG_L(USART_BASE, USART_CR1) |= (1 << 2); // UART receive enable
    REG_L(USART_BASE, USART_CR1) |= (1 << 3); // UART transmit enable
    
}

void send(int c) {
    if (c == '\n') {
        send('\r');
    }
    REG_L(USART_BASE, USART_TDR) = c;
    while ((REG_L(USART_BASE, USART_ISR) & (1 << 6)) == 0);
}

void sends(char* s) {
    while (*s) {
        send(*(s++));
    }
}

int intDiv(int a, int b) {
	int res = 0;
	int power = 1;
	while (a - b >= b) {
		b <<= 1;
		power <<= 1;
	}
	while (power > 0) {
		if (a - b >= 0) {
			a -= b;
			res += power;
		}
		b >>= 1;
		power >>= 1;
	}
	return res;
}

void sendHex(int x, int d) {
    while (d-- > 0) {
        send(hex[(x >> (d * 4)) & 0xF]);
    }
}

void sendDec(int x) {
    static char s[10];
    int i, x1;
    i = 0;
    while (x > 0) {
        x1 = intDiv(x, 10);
        s[i++] = x - x1 * 10;
        x = x1;
    }
    if (i == 0) {
        s[i++] = 0;
    }
    while (i > 0) {
        send('0' + s[--i]);
    }
}

int readb() {
    if ((REG_L(USART_BASE, USART_ISR) & (1 << 5)) != 0) {
        return (REG_L(USART_BASE, USART_RDR) & 0xFF);
    } else {
        return -1;
    }
}

void clockSetup() {
    REG_L(RCC_BASE, RCC_CR) |= (1 << 16); // HSE on
    while ((REG_L(RCC_BASE, RCC_CR) & (1 << 17)) == 0); // HSE ready
    REG_L(RCC_BASE, RCC_CFGR) = (1 << 16); // PLL from HSE
    REG_L(RCC_BASE, RCC_CFGR) |= (PLLMUL - 2) << 18;
    REG_L(RCC_BASE, RCC_CFGR2) = (PLLDIV - 1);
    REG_L(RCC_BASE, RCC_CR) |= (1 << 24); // PLL on
    while ((REG_L(RCC_BASE, RCC_CR) & (1 << 25)) == 0); // PLL ready
    REG_L(RCC_BASE, RCC_CFGR) = 2; // clock from PLL
}

void pwm32khzSetup () {
    REG_L(RCC_BASE, RCC_AHB2ENR) |= (1 << 11); // timer1 clock enabled
    REG_L(TIM1_BASE, TIM1_CCMR1) = (6 << 4);
    int period = SYS_FREQ / RADIO_CLK;
    REG_L(TIM1_BASE, TIM1_ARR) = period - 1;
    REG_L(TIM1_BASE, TIM1_CCR1) = period / 2;
    REG_L(TIM1_BASE, TIM1_CCER) = 1 << 2; // enable oc1
    REG_L(TIM1_BASE, TIM1_BDTR) = (1 << 15); // master output enable
    REG_L(GPIOA_BASE, GPIO_MODER) |= 2 << (7 * 2); // pa7 alternate function
    REG_L(GPIOA_BASE, GPIO_AFRL) |= 2 << (7 * 4); // pa7 AF2
    REG_L(TIM1_BASE, TIM1_CR1) = 1; // counting enabled
}

void i2cDelay() {
    int n = 120;
    while(--n);
}

void i2cScl(char v) {
    if (v) {
        REG_L(GPIOA_BASE, GPIO_ODR) |= 0x20;
    } else {
        REG_L(GPIOA_BASE, GPIO_ODR) &= ~0x20;
    }
    i2cDelay();
}

void i2cSda(char v) {
    if (v) {
        REG_L(GPIOA_BASE, GPIO_ODR) |= 0x40;
    } else {
        REG_L(GPIOA_BASE, GPIO_ODR) &= ~0x40;
    }
    i2cDelay();
}

char i2cPeek() {
    return (REG_L(GPIOA_BASE, GPIO_IDR) >> 6) & 1;
}

void i2cStart() {
    i2cSda(0);
}

void i2cStop() {
    i2cSda(0);
    i2cScl(1);
    i2cSda(1);
}

void i2cRepStart() {
    i2cScl(1);
    i2cSda(0);
}

char i2cSend(int d, char n) {
    char ack;
    while (n) {
        n -= 1;
        i2cScl(0);
        i2cSda((d >> n) & 1);
        i2cScl(1);
    }
    i2cScl(0);
    i2cSda(1);
    i2cScl(1);
    ack = i2cPeek();
    i2cScl(0);
    return ack == 0;
}

void i2cReceiveAck(int res, char a) {
    int doAck = (a == I2C_READ_ACK)
            || (a == I2C_READ_ACK_ON_CTS && ((res & 0x80) != 0));
    if (doAck) {
        i2cSda(0);
    }
    i2cScl(1);
    i2cScl(0);
    if (doAck) {
        i2cSda(1);
    }
}

int i2cReceive(char n, char a) {
    int res = 0;
    while (n) {
        n -= 1;
        res = res * 2 + i2cPeek();
        i2cScl(1);
        i2cScl(0);
    }
    i2cReceiveAck(res, a);
    return res;
}

void radioStatus() {
    i2cStart();
    i2cSend((RADIO_I2C_ADDR << 1) | 1, 8);
    int status = i2cReceive(8,
        respBytesExpected > 0
            ? I2C_READ_ACK_ON_CTS
            : I2C_READ_NACK);
    sends("READ: ");
    sendHex(status, 2);
    if ((status & 0x80) != 0) {
        while (respBytesExpected > 0) {
            int b = i2cReceive(8,
                respBytesExpected > 1
                ? I2C_READ_ACK
                : I2C_READ_NACK);
            send('.');
            sendHex(b, 2);
            respBytesExpected -= 1;
        }
    }
    send('\n');
    i2cStop();
}

void radioCommand(int toSend, int toRead, unsigned char* data) {
    i2cStart();
    int res = i2cSend(RADIO_I2C_ADDR << 1, 8);
    send(res ? '+' : '-');
    for (int cnt = 0; cnt < toSend; cnt += 1) {
        res = i2cSend(data[cnt], 8);
        send(res ? '+' : '-');
    }
    send('\n');
    i2cStop();
    respBytesExpected = toRead;
}

void radioOn() {
    unsigned char b[] = {0x01, 0x00, 0x05};
    radioCommand(3, 0, b);
}

void radioVersion() {
    unsigned char b[] = {0x10};
    radioCommand(1, 8, b);
}

void radioOff() {
    unsigned char b[] = {0x11};
    radioCommand(1, 0, b);
}

void radioSeek(int up) {
    unsigned char b[] = {0x21, (up << 3) | 0x04};
    radioCommand(2, 0, b);
}

void radioGetInts() {
    unsigned char b[] = {0x14};
    radioCommand(1, 0, b);
}

void radioGetTune(int clear) {
    unsigned char b[] = {0x22, 0x00 | clear};
    radioCommand(2, 7, b);
}

void doCommand(int key) {
    switch (key) {
        case 's':
            radioStatus();
            break;
        case 'u':
            radioOn();
            break;
        case 'd':
            radioOff();
            break;
        case 'n':
            radioSeek(1);
            break;
        case 'p':
            radioSeek(0);
            break;
        case 'g':
            radioGetInts();
            break;
        case 'q':
            radioGetTune(0);
            break;
        case 'Q':
            radioGetTune(1);
            break;
        case 'v':
            radioVersion();
            break;
    }
}

int main() {
    int n;
    
    clockSetup();
    
    REG_L(RCC_BASE, RCC_AHBENR) |= (1 << 17); // port A clock
    
    REG_L(GPIOA_BASE, GPIO_MODER) |= 1 << (3 * 2); // pa3 output (rst)
    REG_L(GPIOA_BASE, GPIO_MODER) |= 1 << (4 * 2); // pa4 output (led)
    REG_L(GPIOA_BASE, GPIO_MODER) |= 1 << (5 * 2); // pa5 output (scl)
    REG_L(GPIOA_BASE, GPIO_MODER) |= 1 << (6 * 2); // pa6 output (sda)
    REG_L(GPIOA_BASE, GPIO_TYPER) |= (1 << 5) | (1 << 6); // pa5, pa6 open-drain
    
    uartEnable();
    
    pwm32khzSetup();
    
    REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << (16 + 3)); // reset low
    i2cScl(1);
    i2cSda(1);
    n = 1000; while(--n);
    REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << 3); // reset high
    n = 1000; while(--n);
    
    int addr = 0;
    n = 0;
    while(1) {
        /*i2cStart();
        int v = i2cSend(addr << 1, 8);
        i2cStop();*/
        /*sendHex(addr * 256 + v, 4);
        sends("\r\n");
        addr += 1;*/
        if (n == 0) {
            REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << 4);
            n = 150000;
        } else if (n == 125000) {
            REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << (16 + 4));
        }
        if ((n & 0x20) != 0) {
            int v = readb();
            if (v >= 0) {
                doCommand(v);
            }
        }
        n -= 1;
    }    
}

