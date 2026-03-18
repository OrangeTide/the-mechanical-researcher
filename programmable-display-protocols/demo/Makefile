CC ?= cc
CFLAGS ?= -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
PREFIX ?= /usr/local

all: quaoar-server notepad clock

server: quaoar-server

apps: notepad clock

libs: libquaoar.a

libquaoar.a: libquaoar.a(libquaoar.o)

quaoar-server: quaoar-server.c
	$(CC) $(CFLAGS) -o $@ $<

notepad: notepad.c libquaoar.a libquaoar.h
	$(CC) $(CFLAGS) -o $@ notepad.c libquaoar.a

clock: clock.c libquaoar.a libquaoar.h
	$(CC) $(CFLAGS) -o $@ clock.c libquaoar.a

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -d $(DESTDIR)$(PREFIX)/share/man/man3
	install -m 755 quaoar-server $(DESTDIR)$(PREFIX)/bin/
	install -m 755 notepad $(DESTDIR)$(PREFIX)/bin/
	install -m 755 clock $(DESTDIR)$(PREFIX)/bin/quaoar-clock
	install -m 644 quaoar-server.1 $(DESTDIR)$(PREFIX)/share/man/man1/
	install -m 644 libquaoar.3 $(DESTDIR)$(PREFIX)/share/man/man3/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/quaoar-server
	rm -f $(DESTDIR)$(PREFIX)/bin/notepad
	rm -f $(DESTDIR)$(PREFIX)/bin/quaoar-clock
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/quaoar-server.1
	rm -f $(DESTDIR)$(PREFIX)/share/man/man3/libquaoar.3

clean:
	rm -f quaoar-server notepad clock libquaoar.a

.PHONY: all clean install uninstall libs apps server
