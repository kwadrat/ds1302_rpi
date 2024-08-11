#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bcm2835.h>

/* For Raspberry Pi 3 B+, DS1302 */
#define pin_clk_nr RPI_BPLUS_GPIO_J8_23
#define pin_ce_nr RPI_BPLUS_GPIO_J8_21
#define pin_io_nr RPI_BPLUS_GPIO_J8_19

/* Test burst RAM write using a table of 31 prime numbers */
#define OUT_LEN 32
int out_ls[OUT_LEN] = {
    0xFE, /* Burst RAM write address  */
    0x02, 0x03, 0x05, 0x07, 0x0b, 0x0d, 0x11, 0x13, 0x17, 0x1d, 0x1f,
    0x25, 0x29, 0x2b, 0x2f, 0x35, 0x3b, 0x3d, 0x43, 0x47, 0x49, 0x4f,
    0x53, 0x59, 0x61, 0x65, 0x67, 0x6b, 0x6d, 0x71, 0x7f,
};

#define ENABLE_CLOCK_DELAY 1 /* C is faster than Python - some delay is needed for clock */
#define SMALL_DELAY 1 /* in microseconds */

void work_as_out(void)
{
    bcm2835_gpio_fsel(pin_io_nr, BCM2835_GPIO_FSEL_OUTP);
}

void work_as_in(void)
{
    bcm2835_gpio_fsel(pin_io_nr, BCM2835_GPIO_FSEL_INPT);
}

void clock_high(void)
{
    bcm2835_gpio_write(pin_clk_nr, HIGH);
    #if ENABLE_CLOCK_DELAY
        bcm2835_delayMicroseconds(SMALL_DELAY);
    #endif // ENABLE_CLOCK_DELAY

}

void clock_low(void)
{
    bcm2835_gpio_write(pin_clk_nr, LOW);
    #if ENABLE_CLOCK_DELAY
        bcm2835_delayMicroseconds(SMALL_DELAY);
    #endif // ENABLE_CLOCK_DELAY
}

void ce_enable(void)
{
    bcm2835_gpio_write(pin_ce_nr, HIGH);
}

void ce_disable(void)
{
    bcm2835_gpio_write(pin_ce_nr, LOW);
}

void pins_begin(void)
{
    bcm2835_gpio_fsel(pin_ce_nr, BCM2835_GPIO_FSEL_OUTP);
    ce_disable();
    work_as_in();
    bcm2835_gpio_fsel(pin_clk_nr, BCM2835_GPIO_FSEL_OUTP);
    clock_low();
}

void pins_end(void)
{
    bcm2835_gpio_fsel(pin_clk_nr, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(pin_ce_nr, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(pin_io_nr, BCM2835_GPIO_FSEL_INPT);
}

void drive_data_bit(int one_bit)
{
    int new_value;
    if(one_bit)
    {
        new_value = HIGH;
    }
    else
    {
        new_value = LOW;
    }
    bcm2835_gpio_write(pin_io_nr, new_value);
}

void emit_byte(int one_value)
{
    int i;
    int one_bit;
    for(i = 0; i < 8; i++)
    {
        one_bit = one_value & 1;
        one_value >>= 1;
        drive_data_bit(one_bit);
        clock_high();
        clock_low();
    }
}

int fetch_input(void)
{
    return bcm2835_gpio_lev(pin_io_nr);
}

int get_received_byte(void)
{
    int lcl_value = 0;
    int one_mask = 1;
    int i;
    for(i = 0; i < 8; i++)
    {
        if(fetch_input())
        {
            lcl_value |= one_mask;
        }
        one_mask <<= 1;
        clock_high();
        clock_low();
    }
    return lcl_value;
}

void do_poke(int addr_byte, int data_byte)
{
    ce_enable();
    emit_byte(addr_byte);
    emit_byte(data_byte);
    ce_disable();
}

void do_many_writes(void)
{
    work_as_out();
    do_poke(0x8E, 0x00); /* Set WP (Write-Protect) to zero - enable writing */
    do_poke(0x80, 0x49); /* Set CH (Clock-Halt) to zero - enable clock changes, seconds: 49 */
    do_poke(0x82, 0x59); /* Minutes: 59 */
    do_poke(0x84, 0x23); /* Enable 24-hour mode, Hour: 23 */
    do_poke(0x86, 0x31); /* Date (day of month): 31 */
    do_poke(0x88, 0x03); /* Month: 3 (March) */
    do_poke(0x8A, 0x06); /* Day (day of week): 6 (Saturday) */
    do_poke(0x8C, 0x24); /* Year 2024 */
    do_poke(0xC0, 0x01);
    do_poke(0xC2, 0x03);
    do_poke(0xC4, 0x07);
    do_poke(0xC6, 0x0f);
    do_poke(0xC8, 0x1f);
    do_poke(0xCA, 0x3f);
    do_poke(0xCC, 0x7f);
    do_poke(0xCE, 0xFF);
    do_poke(0xD0, 0xFE);
    do_poke(0xD2, 0xFC);
    do_poke(0xD4, 0xF8);
    do_poke(0xD6, 0xF0);
    do_poke(0xD8, 0xE0);
    do_poke(0xDA, 0xC0);
    do_poke(0xDC, 0x80);
    work_as_in();
}

void do_massive_write_to_ram(void)
{
    int i;
    int one_byte;
    ce_enable();
    work_as_out();
    for(i = 0; i < OUT_LEN; i++)
    {
        one_byte = out_ls[i];
        emit_byte(one_byte);
    }
    work_as_in();
    ce_disable();
}

int take_peek(int one_location)
{
    int read_value;
    ce_enable();
    work_as_out();
    emit_byte(one_location);
    work_as_in();
    read_value = get_received_byte();
    ce_disable();
    return read_value;
}

int do_read_burst(int brst_addr, int brst_size)
{
    int in_value;
    int idx_nr;
    ce_enable();
    work_as_out();
    emit_byte(brst_addr); /* Go to the start of burst transfer */
    work_as_in();
    for(idx_nr = 0; idx_nr < brst_size; idx_nr++)
    {
        in_value = get_received_byte();
        printf("%02X ", in_value);
    }
    printf("\n");
    ce_disable();
}

void read_ram_byte_by_byte(int ram_size)
{
    int in_value;
    int idx_nr;
    int rail_addr;
    for(idx_nr = 0; idx_nr < ram_size; idx_nr++)
    {
        rail_addr = 0xC1 + 2 * idx_nr;
        in_value = take_peek(rail_addr);
        printf("%02X ", in_value);
    }
    printf("\n");
}

void do_many_clock_bursts(void)
{
    int i;
    for(i = 0; i < 4 * 60; i++)
    {
        do_read_burst(0xBF, 8); /* Clock burst */
        bcm2835_delay(1000); /* in milliseconds */
    }
}

int main(int argc, char * argv[])
{
    int error_occured = 0;
    int option_done = 0;
    int has_command = (argc >= 2);
    if(bcm2835_init())
    {
        pins_begin();
        if(has_command && ! strcmp(argv[1], "--write_once"))
        {
            do_many_writes();
            option_done = 1;
        }
        if(has_command && ! strcmp(argv[1], "--write_ram_in_burst"))
        {
            do_massive_write_to_ram();
            option_done = 1;
        }
        if(has_command && ! strcmp(argv[1], "--read_clock"))
        {
            do_read_burst(0xBF, 8); /* Clock burst */
            option_done = 1;
        }
        if(has_command && ! strcmp(argv[1], "--loop_clock"))
        {
            do_many_clock_bursts();
            option_done = 1;
        }
        if(has_command && ! strcmp(argv[1], "--read_ram"))
        {
            int ram_size = 31;
            do_read_burst(0xFF, ram_size); /* RAM burst */
            read_ram_byte_by_byte(ram_size);
            option_done = 1;
        }
        pins_end();
        bcm2835_close();
        if( ! option_done)
        {
            printf("No command specified\n");
            error_occured = 1;
        }
    }
    else
    {
        printf("Init of BCM2835 failed\n");
        error_occured = 1; /* Hardware init failed */
    }
    return error_occured;
}
