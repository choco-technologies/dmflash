#include "dmod.h"
#include "dmflash_ioctl.h"
#include <string.h>

/*
 * flashtest <node> - exercises the dmflash block device, e.g. /dev/dmflashx/flash.
 * Works on the LAST sector of the region: its content is saved first and restored at the end (a failure in between
 * can still leave it erased). Exit code = number of failed steps.
 */

#define MAX_SECTOR  (512u * 1024u)
#define PATTERN     300u
#define TEST_BYTES  4096u

static int g_failed;

static void check(bool ok, const char* what)
{
    Dmod_Printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        g_failed++;
}

static int node_read(void* fp, uint32_t offset, void* buf, uint32_t size)
{
    if (Dmod_FileSeek(fp, (Dmod_FileOffset_t)offset, 0) != 0)
        return -1;
    return Dmod_FileRead(buf, 1, size, fp) == size ? 0 : -1;
}

static int node_write(void* fp, uint32_t offset, const void* buf, uint32_t size)
{
    if (Dmod_FileSeek(fp, (Dmod_FileOffset_t)offset, 0) != 0)
        return -1;
    return Dmod_FileWrite(buf, 1, size, fp) == size ? 0 : -1;
}

static int node_erase(void* fp, uint32_t offset, uint32_t size)
{
    dmdrvi_block_range_t r = { .offset = offset, .length = size };
    return Dmod_Ioctl(fp, DMDRVI_IOCTL_BLOCK_ERASE, &r);
}

static bool all_ff(const uint8_t* p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (p[i] != 0xFF)
            return false;
    return true;
}

/* What the node says about itself; fills `info` and `sector` (the last one of the region) */
static bool test_info(void* fp, dmflash_info_t* info, dmflash_sector_t* sector)
{
    dmdrvi_block_info_t block;
    memset(&block, 0, sizeof(block));
    int ret = Dmod_Ioctl(fp, DMDRVI_IOCTL_BLOCK_GET_INFO, &block);
    Dmod_Printf("block info: ret=%d logical=%u erase=%u blocks=%u flags=0x%x\n", ret, (unsigned)block.logical_block_size,
                (unsigned)block.erase_block_size, (unsigned)block.block_count, (unsigned)block.flags);
    check(ret == 0 && block.erase_block_size != 0 && (block.flags & DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED) != 0 &&
          (block.flags & DMDRVI_BLOCK_FLAG_READ_ONLY) == 0, "block geometry, erase supported, not read only");
    ret = Dmod_Ioctl(fp, DMFLASH_IOCTL_GET_INFO, info);
    Dmod_Printf("flash info: ret=%d address=0x%x size=0x%x sectors=%u unit=%u\n", ret, (unsigned)info->address,
                (unsigned)info->size, (unsigned)info->sector_count, (unsigned)info->program_unit);
    check(ret == 0 && info->address != 0 && info->sector_count > 0, "flash description");
    if (ret != 0 || info->sector_count == 0)
        return false;
    dmflash_sector_arg_t arg = { .index = info->sector_count - 1 };
    check(Dmod_Ioctl(fp, DMFLASH_IOCTL_GET_SECTOR, &arg) == 0 && arg.sector.size != 0, "the last sector of the region");
    *sector = arg.sector;
    return arg.sector.size != 0 && arg.sector.size <= MAX_SECTOR;
}

static void test_erase_write(void* fp, const dmflash_info_t* info, const dmflash_sector_t* s, uint8_t* buf, uint8_t* buf2)
{
    check(node_erase(fp, s->offset, s->size) == 0, "erase the sector (block ioctl)");
    check(node_read(fp, s->offset, buf2, s->size) == 0 && all_ff(buf2, s->size), "erased sector reads 0xFF");
    check(node_erase(fp, s->offset + 1, s->size) != 0, "unaligned erase is refused");

    uint8_t pattern[PATTERN];
    for (unsigned i = 0; i < sizeof(pattern); i++)
        pattern[i] = (uint8_t)(i * 3u + 5u);
    check(node_write(fp, s->offset + 101, pattern, sizeof(pattern)) == 0, "write 300 bytes at an unaligned offset");
    check(node_read(fp, s->offset, buf2, 1024) == 0 && memcmp(buf2 + 101, pattern, sizeof(pattern)) == 0 &&
          all_ff(buf2, 101) && all_ff(buf2 + 101 + sizeof(pattern), 1024 - 101 - sizeof(pattern)),
          "read back: pattern in place, the rest still 0xFF");
    uint8_t other[4] = { 0xFF ^ pattern[0], 0, 0, 0 };
    check(node_write(fp, s->offset + 101, other, 4) != 0, "write over data that is not erased is refused");

    check(node_erase(fp, s->offset, s->size) == 0, "erase again");
    for (uint32_t i = 0; i < TEST_BYTES; i++)
        buf[i] = (uint8_t)(i * 7u + (i >> 8) + 1u);
    check(node_write(fp, s->offset, buf, TEST_BYTES) == 0, "write 4 kB");
    memset(buf2, 0, TEST_BYTES);
    check(node_read(fp, s->offset, buf2, TEST_BYTES) == 0 && memcmp(buf, buf2, TEST_BYTES) == 0, "read back through the node");
    check(memcmp((const void*)(info->address + s->offset), buf, TEST_BYTES) == 0, "the same bytes at the address of the flash");
    check(node_read(fp, info->size - 8u, buf2, 16) != 0, "a read past the end of the region is refused");
}

/* Puts back what the sector held before the test */
static bool restore(void* fp, const dmflash_sector_t* s, const uint8_t* saved, uint8_t* check_buf)
{
    return node_erase(fp, s->offset, s->size) == 0 && node_write(fp, s->offset, saved, s->size) == 0 &&
           node_read(fp, s->offset, check_buf, s->size) == 0 && memcmp(saved, check_buf, s->size) == 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        Dmod_Printf("usage: flashtest <node>\n");
        return 1;
    }
    void* fp = Dmod_FileOpen(argv[1], "r+");
    if (fp == NULL)
    {
        Dmod_Printf("flashtest: cannot open '%s'\n", argv[1]);
        return 1;
    }
    dmflash_info_t info;
    dmflash_sector_t sector = { 0, 0 };
    memset(&info, 0, sizeof(info));
    bool usable = test_info(fp, &info, &sector);
    uint8_t* saved = usable ? Dmod_Malloc(2u * sector.size + TEST_BYTES) : NULL;
    if (saved != NULL)
    {
        uint8_t* buf2 = saved + sector.size;
        uint8_t* buf = buf2 + sector.size;
        bool have_backup = node_read(fp, sector.offset, saved, sector.size) == 0;
        check(have_backup, "save the last sector");
        if (have_backup)
        {
            test_erase_write(fp, &info, &sector, buf, buf2);
            check(restore(fp, &sector, saved, buf2), "the saved sector content restored");
        }
        Dmod_Free(saved);
    }
    else if (usable)
        check(false, "memory for the copy of a sector");
    Dmod_Printf("flashtest: %d failed step(s)\n", g_failed);
    Dmod_FileClose(fp);
    return g_failed;
}
