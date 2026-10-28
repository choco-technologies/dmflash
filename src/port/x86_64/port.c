#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmflash_port.h"
#include "dmflash_mock.h"
#include <errno.h>
#include <string.h>

static uint8_t* memory;
static uint32_t memory_size, unit, erases, programs;
static bool opened;
static int failure;

static int take_failure(void)
{
    int error = failure;
    failure = 0;
    return error;
}

static bool inside(uintptr_t address, uint32_t size)
{
    uintptr_t start = (uintptr_t)memory;
    return memory != NULL && address >= start && address - start <= memory_size && size <= memory_size - (address - start);
}

dmod_dmflash_port_api_declaration(1.0, uintptr_t, _mock_create, ( uint32_t size ))
{
    Dmod_Free(memory);
    memory = Dmod_Malloc(size);
    memory_size = memory != NULL ? size : 0;
    erases = programs = 0;
    failure = 0;
    if (memory != NULL)
        memset(memory, 0xFF, size);
    return (uintptr_t)memory;
}

dmod_dmflash_port_api_declaration(1.0, void, _mock_fail_set, ( int error )) { failure = error; }
dmod_dmflash_port_api_declaration(1.0, uint32_t, _mock_erases, ( void )) { return erases; }
dmod_dmflash_port_api_declaration(1.0, uint32_t, _mock_programs, ( void )) { return programs; }

dmod_dmflash_port_api_declaration(1.0, int, _open, ( const dmflash_port_config_t* config ))
{
    if (config == NULL || config->program_bits % 8 != 0 || config->program_bits == 0 || config->program_bits > 64)
        return -ENOTSUP;
    if (opened)
        return -EBUSY;
    unit = config->program_bits / 8;
    opened = true;
    return 0;
}

dmod_dmflash_port_api_declaration(1.0, void, _close, ( void )) { opened = false; }

dmod_dmflash_port_api_declaration(1.0, int, _erase, ( uint32_t sector, uintptr_t address, uint32_t size ))
{
    (void)sector;
    if (!opened || !inside(address, size))
        return -EINVAL;
    int error = take_failure();
    if (error != 0)
        return error;
    memset((void*)address, 0xFF, size);
    erases++;
    return 0;
}

dmod_dmflash_port_api_declaration(1.0, int, _program, ( uintptr_t address, const void* data, uint32_t size ))
{
    if (!opened || data == NULL || !inside(address, size) || (address % unit) != 0 || (size % unit) != 0)
        return -EINVAL;
    int error = take_failure();
    if (error != 0)
        return error;
    for (uint32_t i = 0; i < size; i++)
        ((uint8_t*)address)[i] &= ((const uint8_t*)data)[i];       /* (a flash can only clear bits) */
    programs += size / unit;
    return 0;
}

int dmod_init(const Dmod_Config_t* config) { (void)config; return 0; }
int dmod_deinit(void)
{
    Dmod_Free(memory);
    memory = NULL;
    return 0;
}
