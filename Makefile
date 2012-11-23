ARDUINO_DIR = /usr/share/arduino
BOARD_TAG    = pro328
ARDUINO_PORT = /dev/ttyUSB*

SRC =  $(ARDUINOLIBS)/Wire/utility/twi.c

ARDUINO_LIBS = uOLED colors SPI LSM303 avr/sleep avr/power Wire Wire/utility

include /usr/share/arduino/Arduino.mk

