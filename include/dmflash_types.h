#ifndef DMFLASH_TYPES_H
#define DMFLASH_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* The internal flash: sectors of different sizes, programmed in small units, erased by the whole sector. */
typedef struct dmflash* dmflash_t;

/* A sector: where it starts (bytes from the start of the flash or of the exposed region) and its size */
typedef struct
{
    uint32_t offset;
    uint32_t size;
} dmflash_sector_t;

/*
 * What the driver is told (the [flash] section of an .ini file, see docs/configuration.md). Nothing about the chip
 * is built into the code: its sectors are a string, so a part with another layout needs an .ini, not a port.
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

#endif // DMFLASH_TYPES_H
