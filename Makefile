#LDFLAGS=-lwiringPi
#CFLAGS=-Wall -Werror -Wextra
LDFLAGS=-lpthread

all: si_ctl si_flash

si_ctl: si_ctl.o si46xx.o si46xx_props.o spi.o i2c.o

si_flash: si_flash.o si46xx.o si46xx_props.o spi.o crc32.o i2c.o

.PHONY: clean

clean:
	rm -f si_flash si_ctl *.o
