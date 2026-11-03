#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmflash_port.h"
#include <errno.h>
#include <stdint.h>

/*
 * The flash of the ESP32-S3 cannot be erased or programmed by code that runs from it (or from the external RAM): the SPI that works
 * on the flash serves the caches of both too. The work needs the caches off, the interrupts off and the code, the data and the
 * stack in the internal RAM, which a module cannot promise (it is loaded wherever the heap has room, the external RAM first). So
 * the firmware does it (dmod-boot: src/arch/xtensa/lx7/flash.c) and offers it to the modules as the API "dmboot"; this port
 * translates the addresses of the memory window of the flash into offsets in the chip.
 */
DMOD_BUILTIN_OUTPUT_API( dmboot, 1.0, int, _flash_erase, ( uint32_t offset, uint32_t size ) );
DMOD_BUILTIN_OUTPUT_API( dmboot, 1.0, int, _flash_program, ( uint32_t offset, const void* data, uint32_t size ) );

#define SECTOR_SIZE     0x1000u     /* of the ROM's driver; the layouts of the configs are made of these or of larger ones */
#define FLASH_SIZE      0x1000000u

static bool opened;
static dmflash_port_config_t config;

static int offset_of( uintptr_t address, uint32_t size, uint32_t* offset )
{
    if( address < config.base || address - config.base > FLASH_SIZE || size > FLASH_SIZE - ( address - config.base ) )
        return -EINVAL;
    *offset = (uint32_t)( address - config.base );
    return 0;
}

dmod_dmflash_port_api_declaration(1.0, int, _open, ( const dmflash_port_config_t* cfg ))
{
    if( cfg == NULL )
        return -EINVAL;
    if( cfg->program_bits != 32 )
        return -ENOTSUP;
    if( opened )
        return -EBUSY;
    config = *cfg;
    opened = true;
    return 0;
}

dmod_dmflash_port_api_declaration(1.0, void, _close, ( void ))
{
    opened = false;
}

dmod_dmflash_port_api_declaration(1.0, int, _erase, ( uint32_t sector, uintptr_t address, uint32_t size ))
{
    (void)sector;
    uint32_t offset;
    if( !opened || ( size % SECTOR_SIZE ) != 0 )
        return -EINVAL;
    int rc = offset_of( address, size, &offset );
    return rc != 0 ? rc : dmboot_flash_erase( offset, size );
}

dmod_dmflash_port_api_declaration(1.0, int, _program, ( uintptr_t address, const void* data, uint32_t size ))
{
    uint32_t offset;
    if( !opened || data == NULL || ( address & 3u ) != 0 || ( size & 3u ) != 0 )
        return -EINVAL;
    int rc = offset_of( address, size, &offset );
    return rc != 0 ? rc : dmboot_flash_program( offset, data, size );
}

int dmod_init(const Dmod_Config_t* cfg) { (void)cfg; return 0; }
int dmod_deinit(void) { return 0; }
