#ifndef DMFLASH_H
#define DMFLASH_H

#include "dmod_types.h"
#include "dmflash_types.h"
#include "dmflash_defs.h"

/*
 * dmflash - the internal flash of a microcontroller as a block device.
 *
 * All the logic is here, none in the port (dmflash_port.h): the sectors, the region that is exposed, the checks,
 * padding a write to the programming step, verifying what was written. The port knows only how to erase a sector
 * and to program some aligned bytes.
 *
 * Offsets are bytes from the start of the exposed region. Flash can only clear bits: erase before write; the driver
 * does not erase by itself (a write over data that is not erased fails with -EIO, it is read back).
 */

/* Opens the flash controller. NULL: bad configuration (-EINVAL), no controller to take, ... */
dmod_dmflash_api(1.0, dmflash_t, _create, ( const dmflash_config_t* config ));

/* Safe with NULL */
dmod_dmflash_api(1.0, void, _destroy, ( dmflash_t flash ));

dmod_dmflash_api(1.0, bool, _is_valid, ( dmflash_t flash ));

dmod_dmflash_api(1.0, int, _get_info, ( dmflash_t flash, dmflash_info_t* info ));

/* The index-th sector of the region (offset from the start of the region) */
dmod_dmflash_api(1.0, int, _get_sector, ( dmflash_t flash, uint32_t index, dmflash_sector_t* sector ));

dmod_dmflash_api(1.0, int, _read, ( dmflash_t flash, uint32_t offset, void* buffer, size_t size ));

/* -EROFS when the region is read only, -EIO when what was read back is not what was written */
dmod_dmflash_api(1.0, int, _write, ( dmflash_t flash, uint32_t offset, const void* buffer, size_t size ));

/* The range has to be made of whole sectors (-EINVAL otherwise) */
dmod_dmflash_api(1.0, int, _erase, ( dmflash_t flash, uint32_t offset, uint32_t size ));

#endif // DMFLASH_H
