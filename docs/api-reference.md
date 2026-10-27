# API reference

## Core (`dmflash.h`)

Offsets are bytes from the start of the exposed region. Flash can only clear bits: **erase before write**; the driver
does not erase by itself. Every function returns 0 or a negative errno.

| Function | |
| --- | --- |
| `dmflash_t dmflash_create(const dmflash_config_t*)` | Opens the controller; NULL for a bad configuration (`layout`, a region that is not whole sectors, `program_bits`) or when the controller is taken already (one at a time) |
| `void dmflash_destroy(dmflash_t)` | Safe with NULL |
| `bool dmflash_is_valid(dmflash_t)` | |
| `int dmflash_get_info(dmflash_t, dmflash_info_t*)` | Address, size, sector count, smallest and biggest sector, programming step, read only |
| `int dmflash_get_sector(dmflash_t, uint32_t index, dmflash_sector_t*)` | The index-th sector of the region (offset in the region, size) |
| `int dmflash_read(dmflash_t, offset, buffer, size)` | Any offset and size; `-EINVAL` past the end of the region |
| `int dmflash_write(dmflash_t, offset, buffer, size)` | Any offset and size, padded to the programming step with `0xFF`; `-EROFS`, `-EINVAL`, `-EIO` (read back differs: not erased) or the port's error |
| `int dmflash_erase(dmflash_t, offset, size)` | The range has to be whole sectors (`-EINVAL`, nothing is erased); each is read back blank |

## The node (`dmflash_ioctl.h`)

`/dev/dmflashx/<section>` is a block device (`dmflash_device.c`): `read` / `write` at any offset,
`DMDRVI_IOCTL_BLOCK_GET_INFO` (`logical_block_size` 512, `erase_block_size` the smallest sector, `ERASE_SUPPORTED`,
`READ_ONLY` for `mode=ro`), `DMDRVI_IOCTL_BLOCK_ERASE`, and

| ioctl | arg |
| --- | --- |
| `DMFLASH_IOCTL_GET_INFO` | `dmflash_info_t*` |
| `DMFLASH_IOCTL_GET_SECTOR` | `dmflash_sector_arg_t*` (`index` in, `sector` out) |

## Port (`dmflash_port.h`)

The core calls these with its mutex held, one at a time; addresses are absolute.

| Function | |
| --- | --- |
| `int dmflash_port_open(const dmflash_port_config_t*)` | Takes the controller (`-EBUSY` if taken), `-ENOTSUP` for a width it cannot do |
| `void dmflash_port_close(void)` | |
| `int dmflash_port_erase(uint32_t sector, uintptr_t address, uint32_t size)` | `sector` is the index in the layout of the whole chip |
| `int dmflash_port_program(uintptr_t address, const void* data, uint32_t size)` | Address and size multiples of `program_bits / 8`. Afterwards the data reads back as it is on the medium (the port invalidates the caches it knows of) |
