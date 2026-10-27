#include "dmflash_internal.h"
#include "dmflash_ioctl.h"
#include "dmdrvi.h"
#include <errno.h>
#include <string.h>

/*
 * The dmdrvi node of the flash. The functions the loader calls (dmod_dmdrvi_dif_api_declaration) are in dmflash.c -
 * only one file of a module may enable the registration - and are a line each: the work is here.
 */

#define LOGICAL_BLOCK_SIZE  512u

/* The node is /dev/dmflashx/<section of the .ini> */
dmflash_t dmflash_device_create(dmini_context_t ini, dmdrvi_dev_num_t* dev_num)
{
    dmflash_config_t config = DMFLASH_CONFIG_DEFAULT;
    const char* section = NULL;
    if (dev_num == NULL || !dmflash_config_read(ini, &config, &section))
        return NULL;
    size_t length = strlen(section);
    dmflash_t flash = (length <= DMDRVI_ALT_NAME_MAX_LEN) ? dmflash_create(&config) : NULL;
    if (flash == NULL)
        return NULL;
    memset(dev_num, 0, sizeof(*dev_num));
    dev_num->flags = DMDRVI_NUM_MINOR | DMDRVI_NUM_ALT_NAME;
    memcpy(dev_num->alt_name, section, length + 1);
    return flash;
}

static int block_info(dmflash_t flash, dmdrvi_block_info_t* out)
{
    dmflash_info_t info;
    int rc = dmflash_get_info(flash, &info);
    if (rc != 0)
        return rc;
    out->logical_block_size = LOGICAL_BLOCK_SIZE;
    out->erase_block_size = info.min_sector_size;
    out->block_count = info.size / LOGICAL_BLOCK_SIZE;
    out->flags = DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED | (info.read_only ? DMDRVI_BLOCK_FLAG_READ_ONLY : 0);
    return 0;
}

static int block_erase(dmflash_t flash, const dmdrvi_block_range_t* range)
{
    if (range->offset < 0 || range->offset > UINT32_MAX || range->length > UINT32_MAX)
        return -EINVAL;
    return dmflash_erase(flash, (uint32_t)range->offset, (uint32_t)range->length);
}

int dmflash_device_ioctl(dmflash_t flash, int command, void* arg)
{
    switch (command)
    {
        case DMDRVI_IOCTL_BLOCK_GET_INFO:   return arg ? block_info(flash, arg) : -EINVAL;
        case DMDRVI_IOCTL_BLOCK_ERASE:      return arg ? block_erase(flash, arg) : -EINVAL;
        case DMFLASH_IOCTL_GET_INFO:        return arg ? dmflash_get_info(flash, arg) : -EINVAL;
        case DMFLASH_IOCTL_GET_SECTOR:
        {
            dmflash_sector_arg_t* a = arg;
            return a ? dmflash_get_sector(flash, a->index, &a->sector) : -EINVAL;
        }
        default:                            return -ENOTTY;     /* (also the probes dmdevfs sends to every node) */
    }
}

int dmflash_device_stat(dmflash_t flash, dmdrvi_stat_t* stat)
{
    dmflash_info_t info;
    if (stat == NULL || dmflash_get_info(flash, &info) != 0)
        return -EINVAL;
    stat->size = info.size;
    stat->mode = info.read_only ? 0444 : 0666;
    return 0;
}
