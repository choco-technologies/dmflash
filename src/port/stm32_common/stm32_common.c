#include "dmod.h"
#include "dmflash_port.h"
#include "registers.h"
#include "stm32_common.h"
#include <errno.h>
#include <string.h>

static bool opened;
static dmflash_port_config_t config;

#if defined(__arm__)
#define BARRIER()   __asm volatile("dsb\n\tisb" ::: "memory")
#else
#define BARRIER()   __asm volatile("" ::: "memory")
#endif

/* PSIZE: 0 - x8, 1 - x16, 2 - x32, 3 - x64 */
static uint32_t psize_bits(void)
{
    uint32_t units = config.program_bits / 8u;
    return (units == 1u ? 0u : units == 2u ? 1u : units == 4u ? 2u : 3u) << FLASH_CR_PSIZE_SHIFT;
}

/*
 * While the flash erases or programs, the CPU cannot fetch from it - anything that runs from the flash waits, the
 * interrupts too. BSY is a register, so this loop is not what stalls. A timeout is only a hung controller: the
 * time spent in a stall counts, so BSY is read once more before giving up.
 */
static int wait_ready(void)
{
    uint64_t start = Dmod_GetUptime();
    while (FLASH_SR & FLASH_SR_BSY)
    {
        if (Dmod_GetUptime() - start > config.timeout_ms && (FLASH_SR & FLASH_SR_BSY))
            return -ETIMEDOUT;
    }
    return 0;
}

/* Collects (and clears) the error flags of the operation that just ended */
static int take_errors(void)
{
    uint32_t sr = FLASH_SR;
    FLASH_SR = sr & (FLASH_SR_ERRORS | FLASH_SR_EOP);
    if (sr & FLASH_SR_WRPERR)
        return -EACCES;
    return (sr & FLASH_SR_ERRORS) ? -EIO : 0;
}

static int unlock(void)
{
    if (FLASH_CR & FLASH_CR_LOCK)
    {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
    return (FLASH_CR & FLASH_CR_LOCK) ? -EIO : 0;
}

static void lock(void)
{
    FLASH_CR |= FLASH_CR_LOCK;
}

/* What was in the data cache and the instruction cache of the core (a Cortex-M7 only) may be older than the flash */
static void invalidate_core_caches(uintptr_t address, uint32_t size)
{
    if (SCB_CCR & SCB_CCR_DC)
    {
        for (uintptr_t line = address & ~(uintptr_t)(CACHE_LINE - 1u); line < address + size; line += CACHE_LINE)
            SCB_DCIMVAC = (uint32_t)line;
    }
    if (SCB_CCR & SCB_CCR_IC)
        SCB_ICIALLU = 0;
    BARRIER();
}

static void finish(uintptr_t address, uint32_t size)
{
    lock();
    stm32_flash_reset_accelerator();
    invalidate_core_caches(address, size);
}

/* Takes the flash for one operation: idle, no old errors, unlocked */
static int begin(void)
{
    int rc = wait_ready();
    if (rc != 0)
        return rc;
    (void)take_errors();
    return unlock();
}

dmod_dmflash_port_api_declaration(1.0, int, _open, ( const dmflash_port_config_t* cfg ))
{
    if (cfg == NULL || cfg->timeout_ms == 0)
        return -EINVAL;
    if (cfg->program_bits != 8 && cfg->program_bits != 16 && cfg->program_bits != 32 && cfg->program_bits != 64)
        return -ENOTSUP;
    if (opened)
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
    if (!opened || sector > (FLASH_CR_SNB_MASK >> FLASH_CR_SNB_SHIFT))
        return -EINVAL;
    int rc = begin();
    if (rc != 0)
        return rc;
    FLASH_CR = FLASH_CR_SER | (sector << FLASH_CR_SNB_SHIFT) | psize_bits();
    BARRIER();
    FLASH_CR |= FLASH_CR_STRT;
    rc = wait_ready();
    int errors = take_errors();
    FLASH_CR &= ~(FLASH_CR_SER | FLASH_CR_SNB_MASK);
    finish(address, size);
    return rc != 0 ? rc : errors;
}

/* One programming step of the configured width */
static void program_unit(uintptr_t address, const uint8_t* data, uint32_t unit)
{
    switch (unit)
    {
        case 1: *(volatile uint8_t*)address = *data; break;
        case 2: { uint16_t v; memcpy(&v, data, 2); *(volatile uint16_t*)address = v; break; }
        case 4: { uint32_t v; memcpy(&v, data, 4); *(volatile uint32_t*)address = v; break; }
        default: { uint64_t v; memcpy(&v, data, 8); *(volatile uint64_t*)address = v; break; }
    }
}

dmod_dmflash_port_api_declaration(1.0, int, _program, ( uintptr_t address, const void* data, uint32_t size ))
{
    uint32_t unit = config.program_bits / 8u;
    if (!opened || data == NULL || (address & (unit - 1u)) != 0 || (size & (unit - 1u)) != 0)
        return -EINVAL;
    int rc = begin();
    if (rc != 0)
        return rc;
    FLASH_CR = FLASH_CR_PG | psize_bits();
    for (uint32_t done = 0; rc == 0 && done < size; done += unit)
    {
        program_unit(address + done, (const uint8_t*)data + done, unit);
        BARRIER();
        rc = wait_ready();
        int errors = take_errors();
        rc = (rc != 0) ? rc : errors;
    }
    FLASH_CR &= ~FLASH_CR_PG;
    finish(address, size);
    return rc;
}
