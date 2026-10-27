#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmflash.h"
#include <errno.h>
#include <string.h>

#ifndef DMFLASH_MOCK
/* The steps below need the flash in RAM of the host port: DMOD_CPU_FAMILY=x86_64 */
DMOD_TEST_STEP(needs_the_host_port)
{
}
#else
#include "dmflash_mock.h"

/*
 * The core against a flash in RAM (the x86_64 port): the layout of an STM32F746 - 4 x 32 kB, 128 kB, 3 x 256 kB -
 * 1 MB. Sector 5 starts at 0x40000, sector 6 at 0x80000.
 */
#define LAYOUT      "4x32k,1x128k,3x256k"
#define FLASH_SIZE  0x100000u

static uintptr_t g_base;
static dmflash_t g_flash;

static dmflash_config_t config(uint32_t offset, uint32_t size, bool read_only)
{
    dmflash_config_t c = DMFLASH_CONFIG_DEFAULT;
    c.base = g_base;
    c.layout = LAYOUT;
    c.offset = offset;
    c.size = size;
    c.read_only = read_only;
    return c;
}

static dmflash_t open_flash(uint32_t offset, uint32_t size, bool read_only)
{
    dmflash_config_t c = config(offset, size, read_only);
    return dmflash_create(&c);
}

void dmod_test_setup(void)
{
    g_base = dmflash_port_mock_create(FLASH_SIZE);
    DMOD_TEST_EXPECT_TRUE(g_base != 0);
    g_flash = open_flash(0, 0, false);
}

void dmod_test_teardown(void)
{
    dmflash_destroy(g_flash);
    g_flash = NULL;
}

DMOD_TEST_STEP(opens_and_describes_the_whole_flash)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_flash);
    DMOD_TEST_EXPECT_TRUE(dmflash_is_valid(g_flash));
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(dmflash_get_info(g_flash, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.size, FLASH_SIZE);
    DMOD_TEST_EXPECT_EQ(info.sector_count, 8u);
    DMOD_TEST_EXPECT_EQ(info.min_sector_size, 0x8000u);
    DMOD_TEST_EXPECT_EQ(info.max_sector_size, 0x40000u);
    DMOD_TEST_EXPECT_EQ(info.program_unit, 4u);
    DMOD_TEST_EXPECT_EQ(info.address, g_base);
    DMOD_TEST_EXPECT_FALSE(info.read_only);
}

DMOD_TEST_STEP(sectors_are_numbered_from_the_start_of_the_region)
{
    dmflash_destroy(g_flash);
    g_flash = open_flash(0x80000, 0, false);          /* Sectors 6 and 7 */
    DMOD_TEST_EXPECT_NOT_NULL(g_flash);
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(dmflash_get_info(g_flash, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.size, 0x80000u);
    DMOD_TEST_EXPECT_EQ(info.sector_count, 2u);
    DMOD_TEST_EXPECT_EQ(info.address, g_base + 0x80000u);
    dmflash_sector_t s;
    DMOD_TEST_EXPECT_EQ(dmflash_get_sector(g_flash, 1, &s), 0);
    DMOD_TEST_EXPECT_EQ(s.offset, 0x40000u);
    DMOD_TEST_EXPECT_EQ(s.size, 0x40000u);
    DMOD_TEST_EXPECT_EQ(dmflash_get_sector(g_flash, 2, &s), -EINVAL);
}

DMOD_TEST_STEP(a_region_has_to_be_whole_sectors)
{
    dmflash_destroy(g_flash);
    g_flash = NULL;
    dmflash_t f = open_flash(0x1000, 0, false);       /* Starts inside a sector */
    DMOD_TEST_EXPECT_NULL(f);
    f = open_flash(0, 0x9000, false);                 /* Ends inside a sector */
    DMOD_TEST_EXPECT_NULL(f);
    f = open_flash(0x80000, 0x100000, false);         /* Past the end */
    DMOD_TEST_EXPECT_NULL(f);
    f = open_flash(0x100000, 0, false);               /* Starts at the end */
    DMOD_TEST_EXPECT_NULL(f);
    f = open_flash(0x20000, 0x20000, false);          /* The 128 kB sector: fine */
    DMOD_TEST_EXPECT_NOT_NULL(f);
    dmflash_destroy(f);
}

/* (A table of strings is data with relocations, which a module cannot have: one call for each) */
static bool layout_opens(const char* layout)
{
    dmflash_config_t c = config(0, 0, false);
    c.layout = layout;
    dmflash_t f = dmflash_create(&c);
    dmflash_destroy(f);
    return f != NULL;
}

DMOD_TEST_STEP(bad_layouts_and_configurations_do_not_open)
{
    dmflash_destroy(g_flash);
    g_flash = NULL;
    DMOD_TEST_EXPECT_FALSE(layout_opens(""));
    DMOD_TEST_EXPECT_FALSE(layout_opens("4x"));
    DMOD_TEST_EXPECT_FALSE(layout_opens("x32k"));
    DMOD_TEST_EXPECT_FALSE(layout_opens("0x32k"));
    DMOD_TEST_EXPECT_FALSE(layout_opens("4x32q"));
    DMOD_TEST_EXPECT_FALSE(layout_opens("4x32k;1x128k"));
    DMOD_TEST_EXPECT_FALSE(layout_opens("4x0"));
    DMOD_TEST_EXPECT_FALSE(layout_opens("2000x1k"));
    DMOD_TEST_EXPECT_FALSE(layout_opens("4294967295x4294967295"));
    DMOD_TEST_EXPECT_TRUE(layout_opens("4x32k,1x128k,3x256k"));
    dmflash_config_t c = config(0, 0, false);
    c.program_bits = 12;
    DMOD_TEST_EXPECT_NULL(dmflash_create(&c));
    c = config(0, 0, false);
    c.layout = NULL;
    DMOD_TEST_EXPECT_NULL(dmflash_create(&c));
    DMOD_TEST_EXPECT_NULL(dmflash_create(NULL));
}

DMOD_TEST_STEP(layouts_may_be_written_in_several_ways)
{
    dmflash_destroy(g_flash);
    g_flash = NULL;
    dmflash_config_t c = config(0, 0, false);
    c.layout = " 4 x 32K , 1x128k,3*256k ";
    dmflash_t f = dmflash_create(&c);
    DMOD_TEST_EXPECT_NOT_NULL(f);
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(dmflash_get_info(f, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.size, FLASH_SIZE);
    dmflash_destroy(f);
    c.layout = "0x8000";                                /* One sector, in hex */
    f = dmflash_create(&c);
    DMOD_TEST_EXPECT_NOT_NULL(f);
    DMOD_TEST_EXPECT_EQ(dmflash_get_info(f, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.sector_count, 1u);
    DMOD_TEST_EXPECT_EQ(info.size, 0x8000u);
    dmflash_destroy(f);
}

DMOD_TEST_STEP(only_one_flash_controller_is_taken_at_a_time)
{
    DMOD_TEST_EXPECT_NULL(open_flash(0, 0, false));
    dmflash_destroy(g_flash);
    g_flash = open_flash(0, 0, false);
    DMOD_TEST_EXPECT_NOT_NULL(g_flash);
}

DMOD_TEST_STEP(erased_flash_reads_as_ff)
{
    uint8_t buffer[16];
    DMOD_TEST_EXPECT_EQ(dmflash_read(g_flash, 0x1234, buffer, sizeof(buffer)), 0);
    for (size_t i = 0; i < sizeof(buffer); i++)
        DMOD_TEST_EXPECT_EQ(buffer[i], 0xFFu);
}

DMOD_TEST_STEP(write_then_read_at_any_offset_and_size)
{
    static const uint32_t offsets[] = { 0, 1, 2, 3, 4, 5, 255, 256, 257, 0x7FFF };
    static const uint32_t sizes[] = { 1, 2, 3, 4, 5, 7, 8, 255, 256, 257, 700 };
    uint8_t data[700], back[700];
    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = (uint8_t)(i * 7 + 1);
    uint32_t at = 0;
    for (size_t o = 0; o < sizeof(offsets) / sizeof(offsets[0]); o++)
    {
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
        {
            at += 0x400;                                /* (a new place every time: nothing is erased in between) */
            DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, at + offsets[o], data, sizes[s]), 0);
            memset(back, 0, sizeof(back));
            DMOD_TEST_EXPECT_EQ(dmflash_read(g_flash, at + offsets[o], back, sizes[s]), 0);
            DMOD_TEST_EXPECT_EQ(memcmp(back, data, sizes[s]), 0);
        }
    }
}

DMOD_TEST_STEP(a_write_leaves_its_neighbours_alone)
{
    uint8_t data[5] = { 1, 2, 3, 4, 5 }, back[16];
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0x103, data, sizeof(data)), 0);
    DMOD_TEST_EXPECT_EQ(dmflash_read(g_flash, 0x100, back, 11), 0);
    static const uint8_t expected[11] = { 0xFF, 0xFF, 0xFF, 1, 2, 3, 4, 5, 0xFF, 0xFF, 0xFF };
    DMOD_TEST_EXPECT_EQ(memcmp(back, expected, sizeof(expected)), 0);
}

DMOD_TEST_STEP(writing_over_data_that_is_not_erased_fails_until_it_is_erased)
{
    uint8_t one[8] = { 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F };
    uint8_t other[8] = { 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0 };
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0x40, one, sizeof(one)), 0);
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0x40, other, sizeof(other)), -EIO);
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0, 0x8000), 0);
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0x40, other, sizeof(other)), 0);
}

DMOD_TEST_STEP(erase_takes_whole_sectors_only)
{
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0x10, 0x8000), -EINVAL);        /* Starts inside */
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0, 0x4000), -EINVAL);           /* Ends inside */
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0x18000, 0x10000), -EINVAL);    /* Ends in the middle of the 128 kB sector */
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0, FLASH_SIZE + 1), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0x20000, 0), 0);                /* Nothing to do */
    DMOD_TEST_EXPECT_EQ(dmflash_port_mock_erases(), 0u);                       /* (a refused range erases nothing) */
}

DMOD_TEST_STEP(erase_clears_the_sectors_of_a_range)
{
    uint8_t data[4] = { 0, 0, 0, 0 }, back[4];
    uint32_t places[] = { 0x7FFC, 0x8000, 0x1FFFC, 0x20000, 0x3FFFC, 0x40000 };
    for (size_t i = 0; i < sizeof(places) / sizeof(places[0]); i++)
        DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, places[i], data, sizeof(data)), 0);
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0x8000, 0x38000), 0);           /* Sectors 1, 2, 3 and 4 (a 128 kB one) */
    DMOD_TEST_EXPECT_EQ(dmflash_port_mock_erases(), 4u);
    for (size_t i = 0; i < sizeof(places) / sizeof(places[0]); i++)
    {
        DMOD_TEST_EXPECT_EQ(dmflash_read(g_flash, places[i], back, sizeof(back)), 0);
        uint8_t expected = (places[i] == 0x7FFC || places[i] == 0x40000) ? 0x00 : 0xFF;      /* Sector 0 and 5 were not erased */
        DMOD_TEST_EXPECT_EQ(back[0], expected);
    }
}

DMOD_TEST_STEP(a_read_only_region_refuses_writes_and_erases)
{
    dmflash_destroy(g_flash);
    g_flash = open_flash(0x80000, 0, true);
    DMOD_TEST_EXPECT_NOT_NULL(g_flash);
    uint8_t byte = 0;
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0, &byte, 1), -EROFS);
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0, 0x40000), -EROFS);
    DMOD_TEST_EXPECT_EQ(dmflash_read(g_flash, 0, &byte, 1), 0);
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(dmflash_get_info(g_flash, &info), 0);
    DMOD_TEST_EXPECT_TRUE(info.read_only);
}

DMOD_TEST_STEP(nothing_outside_the_region_can_be_touched)
{
    dmflash_destroy(g_flash);
    g_flash = open_flash(0x80000, 0, false);
    uint8_t data[8] = { 0 };
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0x80000, data, 1), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0x7FFFF, data, 2), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmflash_read(g_flash, 0x80000, data, 1), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0x80000, 0x40000), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0x7FFFC, data, 4), 0);          /* The last bytes: fine */
    uint8_t below;
    memcpy(&below, (const void*)(g_base + 0x7FFFF), 1);
    DMOD_TEST_EXPECT_EQ(below, 0xFFu);                                         /* (the sector below the region) */
}

DMOD_TEST_STEP(the_port_failing_is_reported_and_the_next_write_works)
{
    uint8_t data[4] = { 1, 2, 3, 4 };
    dmflash_port_mock_fail_set(-ETIMEDOUT);
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0, data, sizeof(data)), -ETIMEDOUT);
    dmflash_port_mock_fail_set(-EIO);
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0, 0x8000), -EIO);
    DMOD_TEST_EXPECT_EQ(dmflash_erase(g_flash, 0, 0x8000), 0);
    DMOD_TEST_EXPECT_EQ(dmflash_write(g_flash, 0, data, sizeof(data)), 0);
}

DMOD_TEST_STEP(a_destroyed_or_foreign_handle_is_refused)
{
    uint8_t byte;
    DMOD_TEST_EXPECT_EQ(dmflash_read(NULL, 0, &byte, 1), -EINVAL);
    DMOD_TEST_EXPECT_FALSE(dmflash_is_valid(NULL));
    dmflash_destroy(NULL);                                                     /* (does not crash) */
    dmflash_destroy(g_flash);
    g_flash = NULL;
    dmflash_t again = open_flash(0, 0, false);                                 /* (the controller was released) */
    DMOD_TEST_EXPECT_NOT_NULL(again);
    dmflash_destroy(again);
}
#endif
