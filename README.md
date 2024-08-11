# ds1302_rpi
Connect DS1302 Real-Time Clock (with 31 bytes of RAM) to Raspberry Pi 3 Model B+
Libraries:
- bcm2835
- pigpio (not made yet)
- Wiring Pi (not made yet)

DS1302 transmits bytes with LSB first whereas I2C and SPI transmit MSB first.
