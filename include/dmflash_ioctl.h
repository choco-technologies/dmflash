#ifndef DMFLASH_IOCTL_H
#define DMFLASH_IOCTL_H

#include "dmdrvi_ioctl.h"
#include "dmflash_types.h"

/*
 * The node of the flash (/dev/dmflashx/<section>) is a block device: DMDRVI_IOCTL_BLOCK_GET_INFO and _ERASE, read and
 * write at any offset. erase_block_size is the smallest sector of the region; a file system wants a region of
 * sectors of one size. Driver-specific commands, numbered from DMDRVI_IOCTL_CUSTOM_BASE:
 */
#define DMFLASH_IOCTL_GET_INFO      (DMDRVI_IOCTL_CUSTOM_BASE + 0)  /**< arg: dmflash_info_t* */
#define DMFLASH_IOCTL_GET_SECTOR    (DMDRVI_IOCTL_CUSTOM_BASE + 1)  /**< arg: dmflash_sector_arg_t* (index in, sector out) */

typedef struct
{
    uint32_t            index;      /**< In: the sector's number in the exposed region */
    dmflash_sector_t    sector;     /**< Out: its offset in the region and its size */
} dmflash_sector_arg_t;

#endif // DMFLASH_IOCTL_H
