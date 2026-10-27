# dmflash

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmflash/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmflash/actions/workflows/ci.yml)

The **internal flash** of a microcontroller as a DMOD block device.

## Description

dmflash exposes a region of the on-chip flash - its sectors, however different in size - as a block device node
(`/dev/dmflashx/flash`): read at any offset, write, erase by sector, `DMDRVI_IOCTL_BLOCK_GET_INFO` and
`_ERASE`. It is made for what has to live in the internal flash and change: a second bank of the firmware, a
bootloader update, a small file system (littlefs, with `dmlittlefs`) for settings.

* **Everything is in the core, the port is four functions.** The sectors, the exposed region, the alignment, padding
  a write to the programming step, reading back what was written, the block device - all are hardware independent.
  The port knows only how to *erase a sector* and *program aligned bytes*.
* **The chip is a string, not code.** The sectors are `layout=4x32k,1x128k,3x256k` in an `.ini` file
  (`configs/`), so a part with another layout needs a file, not a port.
* **Safe by default.** `mode=ro` is the default; a region is whole sectors that you name (`offset`, `size`).
  Writes never erase by themselves, and what was written is read back (`-EIO` if it differs, e.g. over data that
  was not erased).

| Family | Port | State |
| --- | --- | --- |
| STM32F7 | `src/port/stm32f7` + `stm32_common` | validated on an STM32F746G-DISCO ([docs/validation](docs/validation/stm32f746g-disco.txt)) |
| STM32F4 | `src/port/stm32f4` + `stm32_common` | builds; the code is the F7's (the flash interface is the same) - not run on hardware |
| x86_64 | `src/port/x86_64` | a flash in RAM for the tests of the core |

Erasing or programming stops the CPU while it fetches from the flash: everything that runs from it, the
interrupts too, waits (a 256 kB sector of an STM32F7 takes up to a second). See [docs/README.md](docs/README.md).

## Using it

Put one of `configs/` as the driver configuration of the board (dmdevfs loads it) and ask for the `dmflash` module;
`dmf-get` brings `dmflash_port` and `dmini` with it.

```
# configs/board/stm32f746g-disco/flash.ini: sectors 6 and 7 (2 x 256 kB), read and write
[flash]
driver_name=dmflash
layout=4x32k,1x128k,3x256k
offset=0x80000
size=0x80000
mode=rw
```

```
flashtest /dev/dmflashx/flash     # on-board test of the node; saves and restores the last sector of the region
```

There is **no API to open the flash by hand**: dmdevfs makes the node from the `.ini` file, everyone else opens
`/dev/dmflashx/flash` (`Dmod_FileOpen`, `Dmod_Ioctl` with `DMDRVI_IOCTL_BLOCK_*` and `dmflash_ioctl.h`).

## Building

```bash
cmake -S . -B build -DDMOD_CPU_FAMILY=stm32f7      # or stm32f4; x86_64 builds the tests against the flash in RAM
cmake --build build
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout instead of fetching `develop`.

## Testing

```bash
cmake -S . -B build-host -DDMOD_CPU_FAMILY=x86_64 && cmake --build build-host
DMOD_DMF_DIR=$PWD/build-host/dmf ctest --test-dir build-host --output-on-failure
```

18 steps through the dmdrvi functions of the module - what dmdevfs calls - against a flash in RAM that behaves like one (erased bytes are `0xFF`, a program step can only clear bits).

## Project Structure

```
include/dmflash_ioctl.h    the node: ioctls, sector and region info (the only public interface)
include/dmflash_port.h     the port: open, close, erase, program
include/dmflash_mock.h     the flash in RAM of the host port (tests)
src/dmflash.c              the dmdrvi functions dmdevfs calls (the registration is enabled in this one file)
src/dmflash_core.c         the flash: region, padding, verifying, erasing - hardware independent
src/dmflash_layout.c       numbers and the layout string
src/dmflash_config.c       the [flash] section of an .ini
src/dmflash_device.c       block device ioctls
src/port/stm32_common/     the flash interface of the STM32F4 and F7
src/port/stm32f4, stm32f7  lifecycle and the reset of the accelerator
src/port/x86_64/           the flash in RAM (tests)
configs/                   mcu/<mcu>.ini, board/<board>/flash.ini
tools/flashtest/           on-board test
```
