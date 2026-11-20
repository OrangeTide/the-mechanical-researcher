# Build the GUI shared library, its bindings, and the demos.
#
#   make            build libgui.so, the C and Lua bindings, the native demo
#   make lib        just the shared library
#   make check      run the headless self-tests (Pascal, Python, Lua)
#   make clean      remove build products
#
# Prerequisites: fpc, a C compiler, libsdl2-dev. The Lua module also needs a
# Lua 5.x development package (pkg-config name lua5.4, lua5.3, or lua).

FPC      ?= fpc
CC       ?= cc
FPCFLAGS  = -Mobjfpc -Sh -Cg -O2 -vw
UNITS     = -Fucore -Fucanvas
BUILDDIR  = build

LIB       = libgui.so
HEADER    = abi/gui.h

# pick a Lua pkg-config module that exists, if any
LUA_PC   := $(firstword $(foreach m,lua5.4 lua5.3 lua5.2 lua5.1 lua,\
              $(shell pkg-config --exists $(m) && echo $(m))))

.PHONY: all lib bindings native check clean
all: lib native bindings

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# ---- the shared library ----
lib: $(LIB)
$(LIB): abi/guiabi.pas core/*.pas canvas/*.pas | $(BUILDDIR)
	$(FPC) $(FPCFLAGS) $(UNITS) -FU$(BUILDDIR) -FE. -o$(LIB) abi/guiabi.pas

# ---- native Pascal demo (uses the core directly, no FFI) ----
native: demo_native
demo_native: ext/demo_native.pas ext/richwidgets.pas core/*.pas canvas/*.pas | $(BUILDDIR)
	$(FPC) $(FPCFLAGS) $(UNITS) -Fuext -FU$(BUILDDIR) -FE. -odemo_native ext/demo_native.pas

# ---- bindings ----
bindings: $(LIB) bindings/c/example_c lua

bindings/c/example_c: bindings/c/example_c.c $(HEADER) $(LIB)
	$(CC) -O2 -Iabi -o $@ bindings/c/example_c.c -L. -lgui -Wl,-rpath,'$$ORIGIN/../..'

# Lua C module: gui.so loadable with require 'gui'
lua: $(LIB)
ifeq ($(LUA_PC),)
	@echo "note: no Lua dev package found via pkg-config; skipping Lua module"
else
	$(CC) -O2 -fPIC -shared -Iabi $(shell pkg-config --cflags $(LUA_PC)) \
	  -o bindings/lua/gui.so bindings/lua/gui_lua.c -L. -lgui -Wl,-rpath,'$$ORIGIN/../..'
endif

# ---- headless verification ----
# The core, the native Pascal demo, and the C binding are always checked. The
# Lua and Python checks run only when their runtimes are present, so the project
# is fully usable with neither installed.
check: lib native bindings
	@echo "== Pascal native selftest =="
	SDL_VIDEODRIVER=dummy ./demo_native --selftest
	@echo "== C selftest =="
	SDL_VIDEODRIVER=dummy ./bindings/c/example_c --selftest
	@echo "== Lua selftest =="
	@if [ -f bindings/lua/gui.so ] && command -v lua >/dev/null 2>&1; then \
	  cd bindings/lua && SDL_VIDEODRIVER=dummy lua demo.lua --selftest ; \
	else echo "(Lua not available; skipped)"; fi
	@echo "== Python selftest =="
	@if command -v python3 >/dev/null 2>&1; then \
	  cd bindings/python && SDL_VIDEODRIVER=dummy python3 demo.py --selftest ; \
	else echo "(Python not available; skipped)"; fi

clean:
	rm -rf $(BUILDDIR) $(LIB) demo_native bindings/c/example_c \
	  bindings/lua/gui.so bindings/python/__pycache__ *.bmp
