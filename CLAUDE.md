# Project instructions

## Reference CPU implementation

`src/cpu/cpu6502.h`/`cpu6502.cpp` (`CPU6502`, `CpuId::Mos6502`) is the
frozen, cycle-accurate reference implementation of the MOS 6502 ISA. A
future wider-bus, CISC-like CPU variant will be verified against it.

Do not modify `CPU6502` to accommodate a new CPU variant. Only change it
for verified bug fixes in its existing 6502 behavior. New variants
implement the `CPU` interface (`src/cpu/cpu.h`) in their own class/files,
with their own `CpuId` enumerator.
