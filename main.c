#include "stm32f030.h"

char hex[] = "0123456789ABCDEF";
short adc[8];

void uartEnable() {
    REG_L(GPIOA_BASE, GPIO_MODER) &= ~(3 << (9 * 2));
    REG_L(GPIOA_BASE, GPIO_MODER) |= (2 << (9 * 2)); // PA9 alternate function (TX)
    REG_L(GPIOA_BASE, GPIO_AFRH) &= ~(0xF << ((9 - 8) * 4));
    REG_L(GPIOA_BASE, GPIO_AFRH) |= (1 << ((9 - 8) * 4)); // PA9 alternate function 1
    REG_L(RCC_BASE, RCC_AHB2ENR) |= (1 << 14); // UART clock
    REG_L(USART_BASE, USART_BRR) |= 768; // 9600 baud on 8MHz
    REG_L(USART_BASE, USART_CR1) |= 1; // UART enable
    //REG_L(USART_BASE, USART_CR1) |= (1 << 2); // UART receive enable
    REG_L(USART_BASE, USART_CR1) |= (1 << 3); // UART transmit enable
    
}

void send(int c) {
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

void clockSetup() {
    REG_L(RCC_BASE, RCC_CR) |= (1 << 16); // HSE on
    while ((REG_L(RCC_BASE, RCC_CR) & (1 << 17)) == 0); // HSE ready
    REG_L(RCC_BASE, RCC_CFGR) = (1 << 16); // PLL from HSE, mul 2
    REG_L(RCC_BASE, RCC_CFGR2) = 4; // PLL div 5
    REG_L(RCC_BASE, RCC_CR) |= (1 << 24); // PLL on
    while ((REG_L(RCC_BASE, RCC_CR) & (1 << 25)) == 0); // PLL ready
    REG_L(RCC_BASE, RCC_CFGR) = 2; // clock from PLL
}

void pwm32khzSetup () {
    REG_L(RCC_BASE, RCC_AHB2ENR) |= (1 << 11); // timer1 clock enabled
    REG_L(TIM1_BASE, TIM1_CCMR1) = (6 << 4);
    REG_L(TIM1_BASE, TIM1_ARR) = 224;
    REG_L(TIM1_BASE, TIM1_CCR1) = 112;
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

int i2cReceive(char n, char a) {
    int res = 0;
    while (n) {
        n -= 1;
        res = res * 2 + i2cPeek();
        i2cScl(1);
        i2cScl(0);
    }
    if (a)  {
        i2cSda(0);
    }
    i2cScl(1);
    i2cScl(0);
    if (a) {
        i2cSda(1);
    }
    return res;
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
    n=1000; while(--n);
    REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << 3); // reset high
    n=1000; while(--n);
    
    int addr = 0;
    while(1) {
        i2cStart();
        int v = i2cSend(addr << 1, 8);
        i2cStop();
        REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << 4);
        sendHex(addr * 256 + v, 4);
        sends("\r\n");
        addr += 1;
        n=250000; while(--n);
        REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << (16 + 4));
        n=1000000; while(--n);
    }    
}

