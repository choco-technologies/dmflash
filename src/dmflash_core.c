#include "dmflash_internal.h"
#include "dmflash_port.h"
#include <errno.h>
#include <string.h>

static bool valid(dmflash_t flash)
{
    return flash != NULL && flash->magic == DMFLASH_MAGIC;
}

/* The bytes of the region as the CPU sees them */
static const uint8_t* region_address(dmflash_t flash, uint32_t offset)
{
    return (const uint8_t*)(flash->base + flash->region_offset + offset);
}

static bool in_range(dmflash_t flash, uint32_t offset, uint32_t size)
{
    return size <= flash->region_size && offset <= flash->region_size - size;
}

/* ---- Opening ---- */

static bool config_valid(const dmflash_config_t* c)
{
    bool bits = c->program_bits == 8 || c->program_bits == 16 || c->program_bits == 32 || c->program_bits == 64;
    return c->layout != NULL && c->base != 0 && bits && c->timeout_ms > 0 && c->timeout_ms <= 60000;
}

/* The exposed region has to be whole sectors: it starts at one and ends at the end of one */
static int resolve_region(dmflash_t flash, const dmflash_config_t* c)
{
    const dmflash_sector_t* last = &flash->sectors[flash->sector_count - 1];
    uint32_t total = last->offset + last->size;
    int first = dmflash_layout_find(flash->sectors, flash->sector_count, c->offset);
    if (first < 0 || c->offset >= total)
        return -EINVAL;
    uint32_t size = (c->size != 0) ? c->size : total - c->offset;
    if (size > total - c->offset)
        return -EINVAL;
    uint32_t end = c->offset + size, i = (uint32_t)first;
    while (i < flash->sector_count && flash->sectors[i].offset + flash->sectors[i].size < end)
        i++;
    if (i >= flash->sector_count || flash->sectors[i].offset + flash->sectors[i].size != end)
        return -EINVAL;
    flash->first = (uint32_t)first;
    flash->count = i - (uint32_t)first + 1;
    flash->region_offset = c->offset;
    flash->region_size = size;
    return 0;
}

static void release(dmflash_t flash)
{
    flash->magic = 0;
    if (flash->lock != NULL)
        dmosi_mutex_destroy(flash->lock);
    Dmod_Free(flash->sectors);
    Dmod_Free(flash->scratch);
    Dmod_Free(flash);
}

static int setup(dmflash_t flash, const dmflash_config_t* c)
{
    flash->base = c->base;
    flash->unit = c->program_bits / 8;
    flash->read_only = c->read_only;
    flash->lock = dmosi_mutex_create(false);
    flash->scratch = Dmod_Malloc(DMFLASH_SCRATCH_SIZE);
    if (flash->lock == NULL || flash->scratch == NULL)
        return -ENOMEM;
    int rc = dmflash_layout_parse(c->layout, &flash->sectors, &flash->sector_count);
    if (rc == 0)
        rc = resolve_region(flash, c);
    if (rc != 0)
        return rc;
    dmflash_port_config_t port = { c->base, c->program_bits, c->timeout_ms };
    return dmflash_port_open(&port);
}

dmflash_t dmflash_create( const dmflash_config_t* config )
{
    if (config == NULL || !config_valid(config))
        return NULL;
    dmflash_t flash = Dmod_Malloc(sizeof(*flash));
    if (flash == NULL)
        return NULL;
    memset(flash, 0, sizeof(*flash));
    int rc = setup(flash, config);
    if (rc != 0)
    {
        DMOD_LOG_ERROR("dmflash: cannot open the flash (%d)\n", rc);
        release(flash);
        return NULL;
    }
    flash->magic = DMFLASH_MAGIC;
    return flash;
}

void dmflash_destroy( dmflash_t flash )
{
    if (!valid(flash))
        return;
    dmosi_mutex_lock(flash->lock);
    dmflash_port_close();
    dmosi_mutex_unlock(flash->lock);
    release(flash);
}

bool dmflash_is_valid( dmflash_t flash )
{
    return valid(flash);
}

/* ---- Geometry ---- */

int dmflash_get_info( dmflash_t flash, dmflash_info_t* info )
{
    if (!valid(flash) || info == NULL)
        return -EINVAL;
    info->address = flash->base + flash->region_offset;
    info->size = flash->region_size;
    info->sector_count = flash->count;
    info->min_sector_size = UINT32_MAX;
    info->max_sector_size = 0;
    for (uint32_t i = 0; i < flash->count; i++)
    {
        uint32_t size = flash->sectors[flash->first + i].size;
        info->min_sector_size = (size < info->min_sector_size) ? size : info->min_sector_size;
        info->max_sector_size = (size > info->max_sector_size) ? size : info->max_sector_size;
    }
    info->program_unit = flash->unit;
    info->read_only = flash->read_only;
    return 0;
}

int dmflash_get_sector( dmflash_t flash, uint32_t index, dmflash_sector_t* sector )
{
    if (!valid(flash) || sector == NULL || index >= flash->count)
        return -EINVAL;
    sector->offset = flash->sectors[flash->first + index].offset - flash->region_offset;
    sector->size = flash->sectors[flash->first + index].size;
    return 0;
}

/* ---- Reading ---- */

int dmflash_read( dmflash_t flash, uint32_t offset, void* buffer, size_t size )
{
    if (!valid(flash) || (buffer == NULL && size != 0) || size > UINT32_MAX || !in_range(flash, offset, (uint32_t)size))
        return -EINVAL;
    dmosi_mutex_lock(flash->lock);
    memcpy(buffer, region_address(flash, offset), size);
    dmosi_mutex_unlock(flash->lock);
    return 0;
}

/* ---- Writing ---- */

/*
 * Up to DMFLASH_SCRATCH_SIZE bytes: padded with 0xFF to whole programming steps (a bit that stays 1 does not change
 * what is there), programmed, and read back - a write over data that is not erased shows as a difference
 */
static int write_piece(dmflash_t flash, uint32_t offset, const uint8_t* data, uint32_t size)
{
    uint32_t chip_offset = flash->region_offset + offset;
    uint32_t start = chip_offset & ~(flash->unit - 1u);
    uint32_t lead = chip_offset - start;
    uint32_t total = (lead + size + flash->unit - 1u) & ~(flash->unit - 1u);
    memset(flash->scratch, 0xFF, total);
    memcpy(flash->scratch + lead, data, size);
    int rc = dmflash_port_program(flash->base + start, flash->scratch, total);
    if (rc != 0)
        return rc;
    return memcmp(region_address(flash, offset), data, size) == 0 ? 0 : -EIO;
}

static int write_locked(dmflash_t flash, uint32_t offset, const uint8_t* data, uint32_t size)
{
    while (size > 0)
    {
        uint32_t lead = (flash->region_offset + offset) & (flash->unit - 1u);
        uint32_t piece = DMFLASH_SCRATCH_SIZE - lead;
        piece = (piece < size) ? piece : size;
        int rc = write_piece(flash, offset, data, piece);
        if (rc != 0)
            return rc;
        offset += piece;
        data += piece;
        size -= piece;
    }
    return 0;
}

int dmflash_write( dmflash_t flash, uint32_t offset, const void* buffer, size_t size )
{
    if (!valid(flash) || (buffer == NULL && size != 0) || size > UINT32_MAX || !in_range(flash, offset, (uint32_t)size))
        return -EINVAL;
    if (flash->read_only)
        return -EROFS;
    dmosi_mutex_lock(flash->lock);
    int rc = write_locked(flash, offset, buffer, (uint32_t)size);
    dmosi_mutex_unlock(flash->lock);
    return rc;
}

/* ---- Erasing ---- */

/* The sectors the range covers: it has to start and end on sector boundaries */
static int locate_span(dmflash_t flash, uint32_t offset, uint32_t size, uint32_t* first, uint32_t* count)
{
    int index = dmflash_layout_find(flash->sectors, flash->sector_count, flash->region_offset + offset);
    if (index < 0 || (uint32_t)index < flash->first)
        return -EINVAL;
    uint32_t i = (uint32_t)index, remaining = size;
    while (remaining > 0)
    {
        if (i >= flash->first + flash->count || flash->sectors[i].size > remaining)
            return -EINVAL;
        remaining -= flash->sectors[i++].size;
    }
    *first = (uint32_t)index;
    *count = i - (uint32_t)index;
    return 0;
}

static bool blank(const uint8_t* p, uint32_t size)
{
    while (size-- > 0)
    {
        if (*p++ != 0xFF)
            return false;
    }
    return true;
}

static int erase_sector(dmflash_t flash, uint32_t index)
{
    const dmflash_sector_t* s = &flash->sectors[index];
    int rc = dmflash_port_erase(index, flash->base + s->offset, s->size);
    if (rc != 0)
        return rc;
    return blank((const uint8_t*)(flash->base + s->offset), s->size) ? 0 : -EIO;
}

int dmflash_erase( dmflash_t flash, uint32_t offset, uint32_t size )
{
    if (!valid(flash) || !in_range(flash, offset, size))
        return -EINVAL;
    if (flash->read_only)
        return -EROFS;
    uint32_t first = 0, count = 0;
    dmosi_mutex_lock(flash->lock);
    int rc = (size == 0) ? 0 : locate_span(flash, offset, size, &first, &count);
    for (uint32_t i = 0; rc == 0 && i < count; i++)
        rc = erase_sector(flash, first + i);
    dmosi_mutex_unlock(flash->lock);
    return rc;
}

