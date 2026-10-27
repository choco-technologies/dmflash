#ifndef DMFLASH_INTERNAL_H
#define DMFLASH_INTERNAL_H

#include "dmod.h"
#include "dmosi.h"
#include "dmini.h"
#include "dmflash.h"
#include "dmdrvi.h"

#define DMFLASH_MAGIC           0x464C5348u     /* "FLSH" */
#define DMFLASH_MAX_SECTORS     1024u
#define DMFLASH_SCRATCH_SIZE    256u            /* A write goes to the port in pieces of this size at most */

struct dmflash
{
    uint32_t            magic;
    dmosi_mutex_t       lock;
    uintptr_t           base;
    dmflash_sector_t*   sectors;        /* The whole chip */
    uint32_t            sector_count;
    uint32_t            first;          /* The exposed region: the sectors first ... first + count - 1 */
    uint32_t            count;
    uint32_t            region_offset;  /* ... which start here, in bytes from the start of the chip */
    uint32_t            region_size;
    uint32_t            unit;           /* Bytes in one programming step (a power of two) */
    bool                read_only;
    uint8_t*            scratch;
};

/* dmflash_layout.c */

/* "0x08000000", "32k", "1m", "4096": -EINVAL if it is not (all) a number, -ERANGE if it does not fit in 32 bits */
int dmflash_parse_number(const char* text, uint32_t* value);

/* "4x32k,1x128k,3x256k" into a table from the heap (Dmod_Free it) with the offset of every sector */
int dmflash_layout_parse(const char* text, dmflash_sector_t** table, uint32_t* count);

/* The index of the sector that starts at `offset`, -ENOENT when none does */
int dmflash_layout_find(const dmflash_sector_t* table, uint32_t count, uint32_t offset);

/* dmflash_config.c: the [flash] section of an .ini file. `section` is where it was found. */
bool dmflash_config_read(dmini_context_t ini, dmflash_config_t* config, const char** section);

/* dmflash_device.c: the block device behind the dmdrvi functions of dmflash.c */
dmflash_t dmflash_device_create(dmini_context_t ini, dmdrvi_dev_num_t* dev_num);
int dmflash_device_ioctl(dmflash_t flash, int command, void* arg);
int dmflash_device_stat(dmflash_t flash, dmdrvi_stat_t* stat);

#endif // DMFLASH_INTERNAL_H
