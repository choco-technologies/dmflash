#ifndef DMFLASH_IOCTL_H
#define DMFLASH_IOCTL_H

#include <stdint.h>
#include <stdbool.h>
#include "dmdrvi_ioctl.h"

/*
 * The only interface of dmflash is its device node, /dev/dmflashx/<section of the .ini file>, made by dmdevfs from
 * a file of configs/ - nobody opens the flash by hand. The node is a block device: read and write at any offset,
 * DMDRVI_IOCTL_BLOCK_GET_INFO and _ERASE (erase_block_size is the smallest sector of the region; a file system
 * wants a region of sectors of one size). Flash can only clear bits: erase before write; the driver does not erase
 * by itself, and a write over data that is not erased fails with -EIO (it is read back).
 *
 * Driver-specific commands, numbered from DMDRVI_IOCTL_CUSTOM_BASE:
 */
#define DMFLASH_IOCTL_GET_INFO      (DMDRVI_IOCTL_CUSTOM_BASE + 0)  /**< arg: dmflash_info_t* */
#define DMFLASH_IOCTL_GET_SECTOR    (DMDRVI_IOCTL_CUSTOM_BASE + 1)  /**< arg: dmflash_sector_arg_t* (index in, sector out) */

/* A sector: where it starts (bytes from the start of the exposed region) and its size */
typedef struct
{
    uint32_t offset;
    uint32_t size;
} dmflash_sector_t;

/* What the exposed region looks like */
typedef struct
{
    uintptr_t   address;        /* Where its first byte is (readable directly) */
    uint32_t    size;
    uint32_t    sector_count;
    uint32_t    min_sector_size;
    uint32_t    max_sector_size;
    uint32_t    program_unit;   /* Bytes in one programming step: a write is padded to it with 0xFF */
    bool        read_only;
} dmflash_info_t;

typedef struct
{
    uint32_t            index;      /**< In: the sector's number in the exposed region */
    dmflash_sector_t    sector;     /**< Out: its offset in the region and its size */
} dmflash_sector_arg_t;

#endif // DMFLASH_IOCTL_H
