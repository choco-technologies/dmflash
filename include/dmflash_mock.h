#ifndef DMFLASH_MOCK_H
#define DMFLASH_MOCK_H

/*
 * The port of the host (x86_64, for the tests): a flash in RAM that behaves like one - erased bytes are 0xFF,
 * a program step can only clear bits. Build the tests against it with DMOD_CPU_FAMILY=x86_64.
 */

#include "dmflash_port.h"

/* Makes a flash of `size` bytes (erased) and returns the address to give as dmflash_config_t.base; 0: no memory */
dmod_dmflash_port_api(1.0, uintptr_t, _mock_create, ( uint32_t size ));

/* The next erase or program fails with this error (once) */
dmod_dmflash_port_api(1.0, void, _mock_fail_set, ( int error ));

/* How many sectors were erased and how many program steps made since the flash was created */
dmod_dmflash_port_api(1.0, uint32_t, _mock_erases, ( void ));
dmod_dmflash_port_api(1.0, uint32_t, _mock_programs, ( void ));

#endif // DMFLASH_MOCK_H
