# SPDX-License-Identifier: 0BSD OR CC0-1.0
CC = cc
CFLAGS = -Wall -Wextra -Os -std=c99

all: demo

sensor.h sensor.c: sensor.idl microser-gen.sh
	./microser-gen.sh sensor.idl sensor

demo: demo.c sensor.c sensor.h microser.h
	$(CC) $(CFLAGS) -o demo demo.c sensor.c

clean:
	rm -f demo sensor.h sensor.c

.PHONY: all clean
