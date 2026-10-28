#ifndef DMFLASH_PORT_H
#define DMFLASH_PORT_H

#include "dmod_types.h"
#include "dmflash_port_defs.h"
#include <stdint.h>

/*
 * The port: what a flash controller does, nothing more. The sectors, the region, the alignment, padding and
 * verifying are the core's (src/dmflash*.c), so a new architecture is these four functions - or, if its
 * controller is an STM32F4/F7's, an .ini file.
 *
 * The core calls the port with its mutex held, one call at a time. An address is absolute (the flash is memory
 * mapped). When a function returns 0 the data at that address reads back as it is on the medium (the port
 * invalidates the caches it knows of).
 */

typedef struct
{
    uintptr_t   base;           /* Address of the first byte of the flash */
    uint32_t    program_bits;   /* 8, 16, 32 or 64: the port refuses a width it cannot do (-ENOTSUP) */
    uint32_t    timeout_ms;
} dmflash_port_config_t;

/* Takes the controller (-EBUSY when taken already) and unlocks nothing yet: it is unlocked per operation */
dmod_dmflash_port_api(1.0, int, _open, ( const dmflash_port_config_t* config ));

dmod_dmflash_port_api(1.0, void, _close, ( void ));

/* Erases the sector: `sector` is its number in the layout of the whole chip, `address` and `size` where it is */
dmod_dmflash_port_api(1.0, int, _erase, ( uint32_t sector, uintptr_t address, uint32_t size ));

/* Programs `size` bytes, both a multiple of program_bits / 8 and the address aligned to it */
dmod_dmflash_port_api(1.0, int, _program, ( uintptr_t address, const void* data, uint32_t size ));

#endif // DMFLASH_PORT_H
