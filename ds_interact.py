#!/usr/bin/env python3

'''
DS1302: RTC, 31 bytes of static RAM
'''

import sys
import time
import argparse

try:
    import RPi.GPIO as GPIO
    GPIO.setmode(GPIO.BCM)  # Use internal Broadcom numbers
except ModuleNotFoundError:
    GPIO = None

pin_clk_nr = 11
pin_ce_nr = 9
pin_io_nr = 10


def recognize_j_options():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write_once',
                        action='store_true', default=False,
                        help='Write some registers')
    parser.add_argument('--write_ram_in_burst',
                        action='store_true', default=False,
                        help='Write some registers')
    parser.add_argument('--read_clock',
                        action='store_true', default=False,
                        help='Read RTC')
    parser.add_argument('--loop_clock',
                        action='store_true', default=False,
                        help='Read RTC in loop')
    parser.add_argument('--read_ram',
                        action='store_true', default=False,
                        help='Read RAM')
    opt_bag = parser.parse_args()
    return parser, opt_bag


def work_as_out():
    GPIO.setup(pin_io_nr, GPIO.OUT)


def work_as_in():
    GPIO.setup(pin_io_nr, GPIO.IN)


def clock_high():
    GPIO.output(pin_clk_nr, GPIO.HIGH)


def clock_low():
    GPIO.output(pin_clk_nr, GPIO.LOW)


def ce_enable():
    GPIO.output(pin_ce_nr, GPIO.HIGH)


def ce_disable():
    GPIO.output(pin_ce_nr, GPIO.LOW)


def pins_begin():
    if GPIO is None:
        print('Skip pin configuration - no hardware detected')
    else:
        GPIO.setwarnings(False)
        GPIO.setup(pin_ce_nr, GPIO.OUT)
        ce_disable()
        work_as_in()
        GPIO.setup(pin_clk_nr, GPIO.OUT)
        clock_low()


def pins_end():
    if GPIO is None:
        print('Skip pin deactivation - no hardware detected')
    else:
        GPIO.setup(pin_ce_nr, GPIO.IN)
        work_as_in()
        GPIO.setup(pin_clk_nr, GPIO.IN)
        GPIO.cleanup()


def drive_data_bit(one_bit):
    if one_bit:
        new_value = GPIO.HIGH
    else:
        new_value = GPIO.LOW
    GPIO.output(pin_io_nr, new_value)


def emit_byte(one_value):
    '''
    LSB first - send data
    '''
    for i in range(8):
        one_bit = one_value & 1
        one_value >>= 1
        drive_data_bit(one_bit)
        clock_high()
        clock_low()


def fetch_input():
    return GPIO.input(pin_io_nr)


def get_received_byte():
    '''
    LSB first - receive data
    '''
    lcl_value = 0
    one_mask = 1
    for i in range(8):
        if fetch_input():
            lcl_value |= one_mask
        one_mask <<= 1
        clock_high()
        clock_low()
    return lcl_value


def do_poke(addr_byte, data_byte):
    ce_enable()
    emit_byte(addr_byte)
    emit_byte(data_byte)
    ce_disable()


def do_many_writes():
    work_as_out()
    do_poke(0x8E, 0x00)  # Set WP (Write-Protect) to zero - enable writing
    do_poke(0x80, 0x49)  # Set CH (Clock-Halt) to zero - enable clock changes, seconds: 49
    do_poke(0x82, 0x59)  # Minutes: 59
    do_poke(0x84, 0x23)  # Enable 24-hour mode, Hour: 23
    do_poke(0x86, 0x31)  # Date (day of month): 31
    do_poke(0x88, 0x03)  # Month: 3 (March)
    do_poke(0x8A, 0x06)  # Day (day of week): 6 (Saturday)
    do_poke(0x8C, 0x24)  # Year 2024
    do_poke(0xC0, 0x01)
    do_poke(0xC2, 0x03)
    do_poke(0xC4, 0x07)
    do_poke(0xC6, 0x0f)
    do_poke(0xC8, 0x1f)
    do_poke(0xCA, 0x3f)
    do_poke(0xCC, 0x7f)
    do_poke(0xCE, 0xFF)
    do_poke(0xD0, 0xFE)
    do_poke(0xD2, 0xFC)
    do_poke(0xD4, 0xF8)
    do_poke(0xD6, 0xF0)
    do_poke(0xD8, 0xE0)
    do_poke(0xDA, 0xC0)
    do_poke(0xDC, 0x80)
    work_as_in()


def do_massive_write_to_ram():
    ce_enable()
    work_as_out()
    out_ls = [
        0xFE,
        0x02, 0x03, 0x05, 0x07, 0x0b, 0x0d, 0x11, 0x13, 0x17, 0x1d, 0x1f,
        0x25, 0x29, 0x2b, 0x2f, 0x35, 0x3b, 0x3d, 0x43, 0x47, 0x49, 0x4f,
        0x53, 0x59, 0x61, 0x65, 0x67, 0x6b, 0x6d, 0x71, 0x7f,
        ]
    for one_byte in out_ls:
        emit_byte(one_byte)  # Go to the start of burst transfer
    work_as_in()
    ce_disable()


def take_peek(one_location):
    ce_enable()
    work_as_out()
    emit_byte(one_location)
    work_as_in()
    read_value = get_received_byte()
    ce_disable()
    return read_value


def do_read_burst(brst_addr, brst_size):
    out_ls = []
    ce_enable()
    work_as_out()
    emit_byte(brst_addr)  # Go to the start of burst transfer
    work_as_in()
    for idx_nr in range(brst_size):
        in_value = get_received_byte()
        out_ls.append('%02X' % in_value)
    ce_disable()
    print(' '.join(out_ls))


def read_ram_byte_by_byte(ram_size):
    out_ls = []
    for idx_nr in range(ram_size):
        rail_addr = 0xC1 + 2 * idx_nr
        in_value = take_peek(rail_addr)
        out_ls.append('%02X' % in_value)
    print(' '.join(out_ls))


def do_many_clock_bursts():
    for i in range(4 * 60):
        do_read_burst(0xBF, 8)  # Clock burst
        time.sleep(1)


if __name__ == '__main__':
    parser, opt_bag = recognize_j_options()
    option_done = 0
    error_occured = 1
    pins_begin()
    if opt_bag.write_once:
        do_many_writes()
        option_done = 1
    if opt_bag.write_ram_in_burst:
        do_massive_write_to_ram()
        option_done = 1
    if opt_bag.read_clock:
        do_read_burst(0xBF, 8)  # Clock burst
        option_done = 1
    if opt_bag.loop_clock:
        do_many_clock_bursts()
        option_done = 1
    if opt_bag.read_ram:
        ram_size = 31
        do_read_burst(0xFF, ram_size)  # RAM burst
        read_ram_byte_by_byte(ram_size)
        option_done = 1
    error_occured = 0
    pins_end()
    if not option_done:
        parser.print_help()
    sys.exit(error_occured)
