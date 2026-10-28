#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmflash_port.h"
#include "../stm32_common/registers.h"
#include "../stm32_common/stm32_common.h"

/* The instruction and data caches of the flash interface keep what they read: reset them (RM0090, FLASH_ACR) */
#define FLASH_ACR_ICEN      (1u << 9)
#define FLASH_ACR_DCEN      (1u << 10)
#define FLASH_ACR_ICRST     (1u << 11)
#define FLASH_ACR_DCRST     (1u << 12)

void stm32_flash_reset_accelerator(void)
{
    uint32_t acr = FLASH_ACR;
    FLASH_ACR = acr & ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN);
    FLASH_ACR |= FLASH_ACR_ICRST | FLASH_ACR_DCRST;
    FLASH_ACR &= ~(FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    FLASH_ACR = acr;
}

int dmod_init(const Dmod_Config_t* config) { (void)config; return 0; }
int dmod_deinit(void) { return 0; }
