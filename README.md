# ds1302_rpi
Connect DS1302 Real-Time Clock (with 31 bytes of RAM) to Raspberry Pi
Libraries:
- bcm2835
- pigpio
- Wiring Pi

DS1302 transmits bytes with LSB first whereas I2C and SPI transmit MSB first.
