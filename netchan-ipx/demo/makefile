# makefile - Open Watcom wmake, builds the 16-bit DOS target.
# Run with `wmake` (needs the WATCOM env var). GNU make uses GNUmakefile.
# Made by a machine. PUBLIC DOMAIN (CC0-1.0)

!ifndef %WATCOM
!error WATCOM env var not set
!endif

!ifdef __UNIX__
RM = rm -f
!else
RM = del
!endif

# Large model: far data across multiple 64 KB segments, each object < 64 KB.
MODEL = ml

CC = wcc
LD = wcl

COPTS = -q -bt=dos -0 -$(MODEL) -w4 -ox -i=$(%WATCOM)/h
LOPTS = -q -bt=dos -$(MODEL)

.ERASE

# DOS objects use .obj so they never collide with the host gcc build's .o
# files in this same directory.
.EXTENSIONS:
.EXTENSIONS: .exe .obj .c

.c.obj: .AUTODEPEND
	$(CC) $(COPTS) -fo=$^@ $[@

# DOS executables must obey the 8.3 filename limit.
all: ncdemo.exe ipxtest.exe thor.exe .SYMBOLIC

ncdemo.exe: netchan.obj ncdemo.obj
	$(LD) $(LOPTS) -fe=$@ netchan.obj ncdemo.obj

ipxtest.exe: netchan.obj nc_ipx.obj ncipxtest.obj
	$(LD) $(LOPTS) -fe=$@ netchan.obj nc_ipx.obj ncipxtest.obj

GAME_OBJS = thor.obj render.obj plat_dos.obj game.obj game_net.obj &
	netchan.obj nc_ipx.obj rng.obj

thor.exe: $(GAME_OBJS)
	$(LD) $(LOPTS) -fe=$@ $(GAME_OBJS)

clean: .SYMBOLIC
	$(RM) ncdemo.exe ipxtest.exe thor.exe *.obj
