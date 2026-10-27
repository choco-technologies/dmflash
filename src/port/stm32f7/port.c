#define DMOD_ENABLE_REGISTRATION    ON
#include "dmflash_port.h"
#include "dmod.h"

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("dmflash port module initialized (stm32f7)\n");
    return 0;
}

int dmod_deinit(void)
{
    Dmod_Printf("dmflash port module deinitialized (stm32f7)\n");
    return 0;
}

/* ---- API implementation ----
 *
 * Implement the dmod_dmflash_port_api_declaration(...) functions
 * declared in include/dmflash_port.h here. Register an interrupt
 * handler if needed, e.g.:
 *
 *   DMOD_IRQ_HANDLER(SOME_IRQn)
 *   {
 *       // handle interrupt
 *   }
 */
