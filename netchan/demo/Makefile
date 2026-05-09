CFLAGS = -Wall -W
ifneq (release,)
  # Release
  CFLAGS += -Os -flto
else
  # Debug
  CFLAGS += -Og -g
endif
CFLAGS += $(if $(PKGS),$(shell pkg-config --cflags $(PKGS)))
LDLIBS += $(if $(PKGS),$(shell pkg-config --libs $(PKGS)))
CPPFLAGS += -Iinclude/
RM ?= rm -f
all :: netchan_test
clean :: ; $(RM) netchan_test
netchan_test : netchan_test.c netchan.c netchan.h
	$(CC) $(CFLAGS) -o $@ netchan_test.c netchan.c
