.PHONY: all clean
all: jadeite_pins

jadeite_pins: jadeite_pins.c
	gcc jadeite_pins.c -o jadeite_pins -l bcm2835

clean:
	rm jadeite_pins
