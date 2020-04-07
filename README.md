# stm32f030-si4735

Attempt to launch Si4735 radio receiver at SW using stm32f030

### Wiring

For TSSOP-20 package

- pin 16 (VDD) to +3.3
- pin 5  (VDDA) to +3.3
- pin 15 (GND) to GND
- pin 1  (BOOT0) to +3.3
- pin 4  (RESET) to GND, temporarily
- pin 8  (USART1_TX) to RX of the FTDI-cable
- pin 9  (USART1_RX) to TX of the FTDI-cable
- pin 6  (PA4) to LED
- pin 5  (PA3) to RST of Si4735
- pin 11 (PA5) to SCL of Si4735
- pin 12 (PA6) to SDA of Si4735
- pin 13 (PA7) to RCLK of Si4735
