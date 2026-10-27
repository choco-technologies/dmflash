# Configuration

One `[flash]` section (the section's name is the node's name: `/dev/dmflashx/<section>`; `[dmflash]` works too). The
`.ini` files of `configs/` are installed with the module (`dmflash.dmr`): `configs/mcu/<mcu>.ini` is the whole chip,
read only; `configs/board/<board>/flash.ini` is the chip with a region you may write.

| Key | Default | Meaning |
| --- | --- | --- |
| `driver_name` | | `dmflash` |
| `driver_order` | 0 | There is no pin and no other driver to wait for |
| `layout` | *required* | The sectors of the **whole chip**, from its first byte: `4x32k,1x128k,3x256k`. An item is `<count>x<size>` (or just `<size>`); a size is decimal, `0x` hex, or with `k` / `m` (x 1024) |
| `base` | `0x08000000` | Address of the first byte of the flash (it is memory mapped: reading is a `memcpy`) |
| `offset` | 0 | Where the exposed region starts, bytes from the start of the chip; has to be the start of a sector |
| `size` | to the end | Size of the region; it has to end at the end of a sector |
| `mode` | `ro` | `ro` refuses writes and erases (`-EROFS`), `rw` allows them |
| `program_bits` | 32 | Width of one programming step: 32 at a supply of 2.7-3.6 V, 16 at 2.1-2.7 V, 8 at 1.8-2.1 V (64 needs an external VPP and was not tried) |
| `timeout_ms` | 5000 | The longest an erase or a program step may take (the controller hung) |

## Choosing a region

The driver cannot know what the sectors below the region hold: **start it after the firmware** and keep the image
from growing into it. The board files do so (an STM32F746G-DISCO image is ~400 kB, the region starts at 512 kB).
A file system wants a region of sectors of **one size** (the node reports the smallest as `erase_block_size`): two
256 kB sectors of an STM32F7 are fine, 32 kB + 128 kB are not.

A part with two banks numbers its sectors differently in `FLASH_CR` (bank 2 starts at 16): not handled yet - the
sector number given to the port is the index in `layout`.
