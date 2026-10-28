#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmflash_port.h"
#include "../stm32_common/registers.h"
#include "../stm32_common/stm32_common.h"

/* The ART accelerator keeps what it prefetched: reset it (RM0385, FLASH_ACR) */
#define FLASH_ACR_ARTEN     (1u << 9)
#define FLASH_ACR_ARTRST    (1u << 11)

void stm32_flash_reset_accelerator(void)
{
    uint32_t acr = FLASH_ACR;
    FLASH_ACR = acr & ~FLASH_ACR_ARTEN;
    FLASH_ACR |= FLASH_ACR_ARTRST;
    FLASH_ACR &= ~FLASH_ACR_ARTRST;
    FLASH_ACR = acr;
}

int dmod_init(const Dmod_Config_t* config) { (void)config; return 0; }
int dmod_deinit(void) { return 0; }
