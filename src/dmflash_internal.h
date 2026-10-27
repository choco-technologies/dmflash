#ifndef DMFLASH_INTERNAL_H
#define DMFLASH_INTERNAL_H

#include "dmod.h"
#include "dmosi.h"
#include "dmini.h"
#include "dmflash_ioctl.h"
#include "dmdrvi.h"

#define DMFLASH_MAGIC           0x464C5348u     /* "FLSH" */
#define DMFLASH_MAX_SECTORS     1024u
#define DMFLASH_SCRATCH_SIZE    256u            /* A write goes to the port in pieces of this size at most */

/* The flash of one node */
typedef struct dmflash* dmflash_t;

/*
 * What the driver is told: the [flash] section of an .ini file (docs/configuration.md). Nothing about the chip is
 * built into the code - its sectors are a string - so a part with another layout needs an .ini, not a port.
 */
typedef struct
{
    uintptr_t   base;           /* Address of the first byte of the flash (it is memory mapped) */
    const char* layout;         /* The sectors of the whole chip: "4x32k,1x128k,3x256k" (borrowed while it is created) */
    uint32_t    offset;         /* The exposed region: where it starts (a sector boundary), ... */
    uint32_t    size;           /* ... and its size (0: up to the end of the flash) */
    bool        read_only;      /* Writes and erases are refused */
    uint32_t    program_bits;   /* The width of one programming step: 8, 16, 32 or 64 (the supply voltage decides) */
    uint32_t    timeout_ms;     /* The longest an erase or a program step may take */
} dmflash_config_t;

#define DMFLASH_CONFIG_DEFAULT  { 0x08000000u, NULL, 0, 0, true, 32, 5000 }

struct dmflash
{
    uint32_t            magic;
    struct dmflash*     next;           /* The nodes that share the controller */
    uint32_t            program_bits;
    uint32_t            timeout_ms;
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

/*
 * dmflash_core.c: the flash, all of it hardware independent (the port, dmflash_port.h, only erases a sector and
 * programs aligned bytes). Offsets are bytes from the start of the exposed region. Every function returns 0 or
 * a negative errno.
 */

/*
 * The controller is one, the nodes are many: a file system, a bank of the firmware, ... each a region of its own
 * (a [section] of an .ini file each). They share the port and one lock - an erase stops the CPU anyway - and have
 * to agree on the chip (base, layout, program_bits, timeout_ms) and not to overlap.
 */
int dmflash_core_init(void);                            /* (dmod_init) */
int dmflash_core_deinit(void);                          /* (dmod_deinit) -EBUSY while a node is open */

/* Opens the controller. NULL: a bad configuration (the layout, a region that is not whole sectors, program_bits) or the chip or the region does not agree with the nodes that are open */
dmflash_t dmflash_create(const dmflash_config_t* config);
void dmflash_destroy(dmflash_t flash);                  /* (safe with NULL) */
bool dmflash_is_valid(dmflash_t flash);
int dmflash_get_info(dmflash_t flash, dmflash_info_t* info);
int dmflash_get_sector(dmflash_t flash, uint32_t index, dmflash_sector_t* sector);
int dmflash_read(dmflash_t flash, uint32_t offset, void* buffer, size_t size);
/* -EROFS when the region is read only, -EIO when what was read back is not what was written */
int dmflash_write(dmflash_t flash, uint32_t offset, const void* buffer, size_t size);
/* The range has to be made of whole sectors (-EINVAL otherwise, nothing is erased) */
int dmflash_erase(dmflash_t flash, uint32_t offset, uint32_t size);

/* dmflash_layout.c */

/* "0x08000000", "32k", "1m", "4096": -EINVAL if it is not (all) a number, -ERANGE if it does not fit in 32 bits */
int dmflash_parse_number(const char* text, uint32_t* value);

/* The same for an address, which may need more than 32 bits (a host pointer in the tests) */
int dmflash_parse_address(const char* text, uintptr_t* value);

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
