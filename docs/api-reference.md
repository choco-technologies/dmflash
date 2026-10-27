# API reference

dmflash has **one interface: its device node**. There is no function to open the flash by hand - dmdevfs makes the
node from an `.ini` file ([configuration.md](configuration.md)), and everything else uses the node. The core
(`src/dmflash_core.c`) is internal.

Offsets are bytes from the start of the exposed region. Flash can only clear bits: **erase before write**; the driver
does not erase by itself, and a write over data that is not erased fails with `-EIO` (what was written is read back).

## The node (`dmflash_ioctl.h`)

`/dev/dmflashx/<section>` is a block device (`dmflash_device.c`): `read` / `write` at any offset (`-EINVAL` past the end of the region, `-EROFS` for a write to `mode=ro`, a write is padded to the programming step with `0xFF`),
`DMDRVI_IOCTL_BLOCK_GET_INFO` (`logical_block_size` 512, `erase_block_size` the smallest sector, `ERASE_SUPPORTED`,
`READ_ONLY` for `mode=ro`), `DMDRVI_IOCTL_BLOCK_ERASE` (a range of whole sectors, `-EINVAL` otherwise and nothing is erased; each sector is read back blank), and

| ioctl | arg |
| --- | --- |
| `DMFLASH_IOCTL_GET_INFO` | `dmflash_info_t*` |
| `DMFLASH_IOCTL_GET_SECTOR` | `dmflash_sector_arg_t*` (`index` in, `sector` out) |

## Port (`dmflash_port.h`)

The core calls these with its mutex held, one at a time; addresses are absolute.

| Function | |
| --- | --- |
| `int dmflash_port_open(const dmflash_port_config_t*)` | Takes the controller (`-EBUSY` if taken), `-ENOTSUP` for a width it cannot do. The core calls it once, for the first node, and `_close` for the last: the port does not know about nodes |
| `void dmflash_port_close(void)` | |
| `int dmflash_port_erase(uint32_t sector, uintptr_t address, uint32_t size)` | `sector` is the index in the layout of the whole chip |
| `int dmflash_port_program(uintptr_t address, const void* data, uint32_t size)` | Address and size multiples of `program_bits / 8`. Afterwards the data reads back as it is on the medium (the port invalidates the caches it knows of) |
