CC = cc
CFLAGS = -Wall -Wextra -Os -std=c99

all: demo

sensor.h sensor.c: sensor.idl gen.sh
	./gen.sh sensor.idl sensor

demo: demo.c sensor.c sensor.h microser.h
	$(CC) $(CFLAGS) -o demo demo.c sensor.c

clean:
	rm -f demo sensor.h sensor.c

.PHONY: all clean
