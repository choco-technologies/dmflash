# Adding an MCU or an architecture

`dmflash_port` is four functions (`dmflash_port.h`): `_open`, `_close`, `_erase(sector, address, size)`,
`_program(address, data, size)`. The core does the rest - the sector table, the region, alignment and padding,
verifying, the block device - so a port is only the controller's registers.

## Another STM32F4 or F7 part: an .ini file

The flash interface (`FLASH_KEYR`, `SR`, `CR`; sectors erased with `SER` + `SNB`, programmed with `PG` and `PSIZE`) is
the same in every STM32F4 and F7, and `stm32_common` implements it. Add `configs/mcu/<mcu>.ini` with the `layout` of
the part (its reference manual lists the sectors) and, for a board, `configs/board/<board>/flash.ini` with the region.
Nothing else changes.

## A family whose flash interface is the same: a few lines

`src/port/<family>/port.c` is `dmod_init` / `dmod_deinit` and `stm32_flash_reset_accelerator()` - the one thing that
differs between the F4 (it resets its instruction and data caches) and the F7 (its ART accelerator). Add the family to
`COMMON_SOURCES` in `src/port/CMakeLists.txt` and create `config.cmake` (`DMOD_TOOLS_NAME`).

## A different controller: four functions

Write `src/port/<family>/port.c` with the four `dmod_dmflash_port_api_declaration(...)` functions (and
`dmod_init` / `dmod_deinit`); `src/port/x86_64/port.c` is the shortest example. What the core guarantees: a call at a
time, `address` and `size` aligned to `program_bits / 8`, an erase address that is the start of the sector. What the
port guarantees: when it returns 0, a read of the address gives what is on the medium - clean or invalidate the
caches it knows of (the STM32 port invalidates the Cortex-M7 caches by address when they are on).

Select the family with `-DDMOD_CPU_FAMILY=<family>`; do not add a variable of your own.

## Notes

* A controller that cannot fetch code from the flash while it erases (the STM32F4 and F7 can not) stalls the CPU for
  as long as it takes - the port does not hide it.
* The STM32F4 port was not run on hardware. It is the same code as the F7 port, which was
  ([validation](validation/stm32f746g-disco.txt)); the F4 differs in the accelerator reset (RM0090, `FLASH_ACR`) and in
  the sector layout (the `.ini`).
