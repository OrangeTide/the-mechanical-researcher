# Inferno OS — Kernel System Calls

## Architecture

The Dis virtual machine runs on top of a kernel written mostly in C. Inferno does
not use traditional numbered syscalls like Unix. Instead, the Dis VM calls into
the kernel through a **module interface** — the built-in `Sys` module, defined in
Limbo and implemented in C.

```
Limbo program → imports Sys module → Dis VM dispatches → C implementation → kernel ops
```

### Key source files

- `module/sys.m` — Limbo interface declaration for the Sys module
- `emu/port/inferno.c` — C implementations of each `Sys_*` function (hosted/emulator build)
- `os/port/inferno.c` — same, for the native kernel build
- `libinterp/runt.c` — registers the Sys module with the Dis interpreter at startup
- `emu/port/sysfile.c` — low-level kernel file/channel operations (`kopen`, `kread`, `kwrite`, `kbind`, `kmount`, etc.)
- Generated header `sysmod.h` — built by `limbo -t Sys -I../module ../module/runt.m`

## System Calls (~37 functions)

### File I/O
- `open`, `create`, `read`, `readn`, `pread`, `write`, `pwrite`
- `seek`, `remove`, `dup`, `fildes`, `pipe`
- `fd2path`, `iounit`, `stream`, `dirread`

### File metadata
- `stat`, `fstat`, `wstat`, `fwstat`

### Namespace manipulation
- `bind`, `mount`, `unmount`, `chdir`, `export`, `pctl`

### Networking
- `dial`, `announce`, `listen`, `fauth`, `fversion`

### Process and time
- `sleep`, `millisec`

### Formatted I/O
- `print`, `fprint`, `sprint`, `aprint`

### String utilities
- `tokenize`, `utfbytes`, `byte2char`, `char2byte`

### IPC
- `file2chan` — creates a synthetic file backed by a Limbo channel

### Error handling
- `werrstr` — set the current error string

## Call pattern

Every syscall follows the same pattern: unpack the Dis stack frame, release the
VM lock (so other Dis threads can run), call the underlying kernel function, then
reacquire the lock:

```c
void Sys_open(void *fp) {
    F_Sys_open *f = fp;
    release();
    *f->ret = kopen(string2c(f->s), f->mode);
    acquire();
}
```

The `release()`/`acquire()` pair is how Inferno achieves concurrency — the VM is
cooperatively scheduled, but syscalls yield the processor while blocked on I/O.

## Design notes

Very much in the Plan 9 tradition: a small, orthogonal set of operations where
most resources (network, devices, even processes) are accessed as files through
the 9P protocol via `mount`/`bind`. The `pctl` syscall handles process control
(namespace isolation, fd groups, environment). `file2chan` is the mechanism for
writing user-space file servers in Limbo.

Two build targets share the same syscall set:
- **emu** (hosted) — runs as a user process on a host OS (Linux, macOS, Windows, etc.)
- **os** (native) — runs bare-metal or on minimal hardware
