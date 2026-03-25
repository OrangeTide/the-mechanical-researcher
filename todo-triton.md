# Triton Game System by Vertex

## Part 1: ColdFire Emulator Article

- [ ] demo/README.md — setup guide (prerequisites, build, run)
- [ ] QEMU validation — run `qemu-m68k test_program.elf`, compare results
- [ ] Valgrind clean — `make valgrind`
- [ ] Compare to Musashi (https://github.com/kstenerud/Musashi) — brief paragraph in article
- [ ] coldfire-emulator/index.md — the article (written last, after LOC counts are final)
- [ ] Update top-level README.md with link to new topic

## Part 2+: Fantasy Console (Future)

### Propaganda

- Alternate history articles to establish the Vertex/Triton narrative:
  - Press release for the founding of Vertex (1999)
  - Industry publication article about the unreleased Triton console
  - Include Vertex and Triton in a 2001 E3 report
  - Period-correct HTML styling; include text body for quoting

### Technical

- Support .BIN/.CUE and .ISO formats (audio and data CDs)
- Look into using PC style PS/2 keyboard and mouse. This would have been the most cost-effective choice in 2001.

### Design

- Look at Magic Cap (https://en.wikipedia.org/wiki/Magic_Cap) for UI ideas
- Console specification document for third-party implementers
- Validation test suite for CPU and graphics (smoke test is a start)
- FreePascal cross-compilation for 68K targets
  (https://downloads.freepascal.org/fpc/snapshot/trunk/m68k-linux/)

### Open Questions

- Period-correct operating system — likely custom and minimal, in flash:
  - Simple Unix clone with syscalls like Coherent Unix?
    (https://en.wikipedia.org/wiki/Coherent_%28operating_system%29)
    Drop multi-user, protection, virtual memory.
  - Custom like Human68k on X68000?
  - Custom but functionally equivalent to OS-9?
  - High-level abstraction like Palm OS?
