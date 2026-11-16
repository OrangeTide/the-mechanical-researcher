# SPDX-License-Identifier: 0BSD OR CC0-1.0
FPC ?= fpc
FPCFLAGS ?= -O2 -Mobjfpc

SOURCES = demo.pas gui.pas guisdl.pas sdl2min.pas rpgext.pas

demo: $(SOURCES)
	$(FPC) $(FPCFLAGS) demo.pas

# Render one frame headless to a BMP (no display needed)
shot: demo
	./demo --shot demo.bmp

clean:
	rm -f demo *.o *.ppu *.bmp link*.res ppas*.sh

.PHONY: shot clean
