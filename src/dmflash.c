#define DMOD_ENABLE_REGISTRATION ON
#include "dmflash_internal.h"
#include <errno.h>

/*
 * The only interface of the module is the dmdrvi device, configured by dmdevfs from an .ini file (configs/): there
 * is no API to open the flash by hand. These are the functions dmdevfs calls - the registration of a module is
 * enabled in one file only, this one; the work is in dmflash_core.c (the flash) and dmflash_device.c (the node).
 */

/* ---- The device (dmdrvi): see dmflash_device.c ---- */

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, dmdrvi_context_t, _create, ( dmini_context_t ini, dmdrvi_dev_num_t* dev_num ))
{
    return (dmdrvi_context_t)dmflash_device_create(ini, dev_num);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, void, _free, ( dmdrvi_context_t context ))
{
    dmflash_destroy((dmflash_t)context);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, void*, _open, ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t* dev_num ))
{
    (void)flags;
    (void)dev_num;
    return dmflash_is_valid((dmflash_t)context) ? context : NULL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    (void)context;
    (void)handle;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, dmdrvi_ssize_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    (void)handle;
    int rc = (offset < 0 || offset > UINT32_MAX) ? -EINVAL : dmflash_read((dmflash_t)context, (uint32_t)offset, buffer, size);
    return (rc == 0) ? (dmdrvi_ssize_t)size : rc;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, dmdrvi_ssize_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    (void)handle;
    int rc = (offset < 0 || offset > UINT32_MAX) ? -EINVAL : dmflash_write((dmflash_t)context, (uint32_t)offset, buffer, size);
    return (rc == 0) ? (dmdrvi_ssize_t)size : rc;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    (void)handle;
    return dmflash_device_ioctl((dmflash_t)context, command, arg);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    (void)handle;
    return dmflash_is_valid((dmflash_t)context) ? 0 : -EINVAL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmflash, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{
    (void)path;
    return dmflash_device_stat((dmflash_t)context, stat);
}

int dmod_init(const Dmod_Config_t* config)
{
    (void)config;
    return dmflash_core_init();
}

int dmod_deinit(void)
{
    return dmflash_core_deinit();
}
