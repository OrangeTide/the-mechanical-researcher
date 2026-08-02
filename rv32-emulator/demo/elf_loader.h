/* elf_loader.h : minimal ELF32 little-endian loader for RV32 binaries */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>

/* Load an ELF32 little-endian RISC-V executable via a write callback.
 * Returns the entry point on success, 0 on failure. The callback is
 * invoked once per byte of loadable segment data. */
typedef void (*elf_write_fn)(void *ctx, uint32_t addr, uint8_t byte);

uint32_t elf_load(const char *path, elf_write_fn write, void *ctx);

/* Look up a symbol address in the ELF symbol table. Returns 0 if absent. */
uint32_t elf_symbol(const char *path, const char *name);

#endif /* ELF_LOADER_H */
