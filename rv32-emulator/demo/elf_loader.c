/* elf_loader.c : minimal ELF32 little-endian loader for RV32 binaries */
/* Copyright (c) 2026 Jon Mayo - MIT-0 OR Public Domain */

#include "elf_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/****************************************************************
 * ELF32 constants
 ****************************************************************/

#define ET_EXEC     2
#define EM_RISCV    243
#define PT_LOAD     1
#define SHT_SYMTAB  2
#define ELFCLASS32  1
#define ELFDATA2LSB 1

/****************************************************************
 * Byte-order helpers
 ****************************************************************/

static uint16_t
rd16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t
rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/****************************************************************
 * File reading
 ****************************************************************/

/** Read the whole file and validate the ELF32 RISC-V header.
 *
 * Returns a malloc'd buffer, or NULL on error. *size receives the length.
 */
static uint8_t *
read_elf(const char *path, long *size)
{
    FILE *f;
    uint8_t *buf;
    long fsize;

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "elf_load: cannot open '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 52) {
        fprintf(stderr, "elf_load: file too small\n");
        fclose(f);
        return NULL;
    }
    buf = malloc(fsize);
    if (!buf) {
        fprintf(stderr, "elf_load: out of memory\n");
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, fsize, f) != (size_t)fsize) {
        fprintf(stderr, "elf_load: read error\n");
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);

    if (buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') {
        fprintf(stderr, "elf_load: bad ELF magic\n");
        free(buf);
        return NULL;
    }
    if (buf[4] != ELFCLASS32) {
        fprintf(stderr, "elf_load: not ELF32\n");
        free(buf);
        return NULL;
    }
    if (buf[5] != ELFDATA2LSB) {
        fprintf(stderr, "elf_load: not little-endian\n");
        free(buf);
        return NULL;
    }
    if (rd16le(buf + 16) != ET_EXEC) {
        fprintf(stderr, "elf_load: not executable\n");
        free(buf);
        return NULL;
    }
    if (rd16le(buf + 18) != EM_RISCV) {
        fprintf(stderr, "elf_load: not RISC-V (machine=%u)\n",
                rd16le(buf + 18));
        free(buf);
        return NULL;
    }

    *size = fsize;
    return buf;
}

/****************************************************************
 * Public interface
 ****************************************************************/

uint32_t
elf_load(const char *path, elf_write_fn write, void *ctx)
{
    uint8_t *buf;
    long fsize;
    uint32_t entry, phoff, j;
    uint16_t phnum, phentsize;
    int i;

    buf = read_elf(path, &fsize);
    if (!buf)
        return 0;

    entry     = rd32le(buf + 24);
    phoff     = rd32le(buf + 28);
    phentsize = rd16le(buf + 42);
    phnum     = rd16le(buf + 44);

    for (i = 0; i < phnum; i++) {
        const uint8_t *ph = buf + phoff + (uint32_t)i * phentsize;
        uint32_t p_type, p_offset, p_vaddr, p_filesz, p_memsz;

        if (ph + phentsize > buf + fsize)
            break;

        p_type   = rd32le(ph + 0);
        p_offset = rd32le(ph + 4);
        p_vaddr  = rd32le(ph + 8);
        p_filesz = rd32le(ph + 16);
        p_memsz  = rd32le(ph + 20);

        if (p_type != PT_LOAD)
            continue;
        if (p_offset + p_filesz > (uint32_t)fsize) {
            fprintf(stderr, "elf_load: segment %d exceeds file bounds\n", i);
            free(buf);
            return 0;
        }

        for (j = 0; j < p_filesz; j++)
            write(ctx, p_vaddr + j, buf[p_offset + j]);
        for (j = p_filesz; j < p_memsz; j++)
            write(ctx, p_vaddr + j, 0);
    }

    free(buf);
    return entry;
}

uint32_t
elf_symbol(const char *path, const char *name)
{
    uint8_t *buf;
    long fsize;
    uint32_t shoff, result = 0;
    uint16_t shnum, shentsize;
    int i;

    buf = read_elf(path, &fsize);
    if (!buf)
        return 0;

    shoff     = rd32le(buf + 32);
    shentsize = rd16le(buf + 46);
    shnum     = rd16le(buf + 48);

    for (i = 0; i < shnum && !result; i++) {
        const uint8_t *sh = buf + shoff + (uint32_t)i * shentsize;
        uint32_t type, off, size, link, entsize, k;
        const uint8_t *strtab;

        if (sh + shentsize > buf + fsize)
            break;
        type = rd32le(sh + 4);
        if (type != SHT_SYMTAB)
            continue;

        off     = rd32le(sh + 16);
        size    = rd32le(sh + 20);
        link    = rd32le(sh + 24);
        entsize = rd32le(sh + 36);
        if (entsize == 0 || link >= shnum)
            continue;

        strtab = buf + rd32le(buf + shoff + link * shentsize + 16);
        for (k = 0; k + entsize <= size; k += entsize) {
            const uint8_t *sym = buf + off + k;
            uint32_t nameoff = rd32le(sym + 0);

            if (strcmp((const char *)strtab + nameoff, name) == 0) {
                result = rd32le(sym + 4);
                break;
            }
        }
    }

    free(buf);
    return result;
}
