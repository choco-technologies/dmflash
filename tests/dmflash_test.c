#define DMOD_ENABLE_REGISTRATION ON
#define ENABLE_DIF_REGISTRATIONS ON
#include "dmod_test.h"
#include "dmdrvi.h"
#include "dmflash_ioctl.h"
#include "dmini.h"
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
 * dmflash has no API of its own: dmdevfs makes a node of it from an .ini file. These steps do what dmdevfs does -
 * build the .ini, call the dmdrvi functions of the module - against a flash in RAM (the x86_64 port) with the layout
 * of an STM32F746: 4 x 32 kB, 128 kB, 3 x 256 kB, 1 MB. Sector 5 starts at 0x40000, sector 6 at 0x80000.
 */
#define LAYOUT      "4x32k,1x128k,3x256k"
#define FLASH_SIZE  0x100000u

static uintptr_t g_base;
static dmdrvi_context_t g_node;
static dmod_dmdrvi_create_t g_create;
static dmod_dmdrvi_free_t g_free;
static dmod_dmdrvi_read_t g_read;
static dmod_dmdrvi_write_t g_write;
static dmod_dmdrvi_ioctl_t g_ioctl;
static dmod_dmdrvi_stat_t g_stat;
static dmdrvi_dev_num_t g_number;

/* ---- A node from an .ini file, the way dmdevfs makes one ---- */

typedef struct
{
    const char* section;
    const char* layout;
    const char* offset;
    const char* size;
    const char* mode;
    const char* program_bits;
} node_options_t;

static void hex(char* out, uintptr_t value)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[2 * sizeof(uintptr_t)];
    int n = 0;
    do { tmp[n++] = digits[value & 15u]; value >>= 4; } while (value != 0);
    *out++ = '0';
    *out++ = 'x';
    while (n > 0)
        *out++ = tmp[--n];
    *out = '\0';
}

static const char* g_section = "flash";

static void put(dmini_context_t ini, const char* key, const char* value)
{
    if (value != NULL)
        dmini_set_string(ini, g_section, key, value);
}

static dmdrvi_context_t make_node(const node_options_t* o)
{
    dmini_context_t ini = dmini_create();
    char base[2 * sizeof(uintptr_t) + 3];
    hex(base, g_base);
    g_section = (o->section != NULL) ? o->section : "flash";
    put(ini, "driver_name", "dmflash");
    put(ini, "base", base);
    put(ini, "layout", o->layout != NULL ? o->layout : LAYOUT);
    put(ini, "offset", o->offset);
    put(ini, "size", o->size);
    put(ini, "mode", o->mode != NULL ? o->mode : "rw");
    put(ini, "program_bits", o->program_bits);
    memset(&g_number, 0, sizeof(g_number));
    dmdrvi_context_t node = g_create(ini, &g_number);
    dmini_destroy(ini);
    return node;
}

static dmdrvi_context_t make_default(void)
{
    node_options_t o = { 0 };
    return make_node(&o);
}

static dmdrvi_context_t make_region(const char* offset, const char* mode)
{
    node_options_t o = { .offset = offset, .mode = mode };
    return make_node(&o);
}

static dmdrvi_context_t make_layout(const char* layout)
{
    node_options_t o = { .layout = layout };
    return make_node(&o);
}

/* ---- The node ---- */

static dmdrvi_ssize_t rd(dmdrvi_context_t n, uint32_t offset, void* buf, size_t size)
{
    return g_read(n, NULL, buf, size, offset);
}

static dmdrvi_ssize_t wr(dmdrvi_context_t n, uint32_t offset, const void* buf, size_t size)
{
    return g_write(n, NULL, buf, size, offset);
}

static int er(dmdrvi_context_t n, uint32_t offset, uint32_t size)
{
    dmdrvi_block_range_t r = { .offset = offset, .length = size };
    return g_ioctl(n, NULL, DMDRVI_IOCTL_BLOCK_ERASE, &r);
}

static void close_node(dmdrvi_context_t* n)
{
    if (*n != NULL)
        g_free(*n);
    *n = NULL;
}

static bool load_functions(void)
{
    if (Dmod_LoadModuleByName("dmflash") == NULL || !Dmod_EnableModule("dmflash", false, NULL))
        return false;
    Dmod_Context_t* m = Dmod_GetModuleContext("dmflash");
    g_create = Dmod_GetDifFunction(m, dmod_dmdrvi_create_sig);
    g_free = Dmod_GetDifFunction(m, dmod_dmdrvi_free_sig);
    g_read = Dmod_GetDifFunction(m, dmod_dmdrvi_read_sig);
    g_write = Dmod_GetDifFunction(m, dmod_dmdrvi_write_sig);
    g_ioctl = Dmod_GetDifFunction(m, dmod_dmdrvi_ioctl_sig);
    g_stat = Dmod_GetDifFunction(m, dmod_dmdrvi_stat_sig);
    return g_create && g_free && g_read && g_write && g_ioctl && g_stat;
}

void dmod_test_setup(void)
{
    DMOD_TEST_EXPECT_TRUE(load_functions());
    g_base = dmflash_port_mock_create(FLASH_SIZE);
    DMOD_TEST_EXPECT_TRUE(g_base != 0);
    g_node = make_default();
}

void dmod_test_teardown(void)
{
    close_node(&g_node);
}

/* ---- Steps ---- */

DMOD_TEST_STEP(the_node_is_named_after_the_section)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_node);
    DMOD_TEST_EXPECT_TRUE((g_number.flags & DMDRVI_NUM_ALT_NAME) != 0);
    DMOD_TEST_EXPECT_EQ(strcmp(g_number.alt_name, "flash"), 0);
}

DMOD_TEST_STEP(describes_the_whole_flash)
{
    dmdrvi_block_info_t block;
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, DMDRVI_IOCTL_BLOCK_GET_INFO, &block), 0);
    DMOD_TEST_EXPECT_EQ(block.logical_block_size, 512u);
    DMOD_TEST_EXPECT_EQ(block.erase_block_size, 0x8000u);
    DMOD_TEST_EXPECT_EQ(block.block_count, FLASH_SIZE / 512u);
    DMOD_TEST_EXPECT_EQ(block.flags, (uint32_t)DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED);
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, DMFLASH_IOCTL_GET_INFO, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.size, FLASH_SIZE);
    DMOD_TEST_EXPECT_EQ(info.sector_count, 8u);
    DMOD_TEST_EXPECT_EQ(info.min_sector_size, 0x8000u);
    DMOD_TEST_EXPECT_EQ(info.max_sector_size, 0x40000u);
    DMOD_TEST_EXPECT_EQ(info.program_unit, 4u);
    DMOD_TEST_EXPECT_EQ(info.address, g_base);
    DMOD_TEST_EXPECT_FALSE(info.read_only);
    dmdrvi_stat_t stat;
    DMOD_TEST_EXPECT_EQ(g_stat(g_node, "", &stat), 0);
    DMOD_TEST_EXPECT_EQ(stat.size, FLASH_SIZE);
}

DMOD_TEST_STEP(sectors_are_numbered_from_the_start_of_the_region)
{
    close_node(&g_node);
    g_node = make_region("0x80000", "rw");                  /* Sectors 6 and 7 */
    DMOD_TEST_EXPECT_NOT_NULL(g_node);
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, DMFLASH_IOCTL_GET_INFO, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.size, 0x80000u);
    DMOD_TEST_EXPECT_EQ(info.sector_count, 2u);
    DMOD_TEST_EXPECT_EQ(info.address, g_base + 0x80000u);
    dmflash_sector_arg_t arg = { .index = 1 };
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, DMFLASH_IOCTL_GET_SECTOR, &arg), 0);
    DMOD_TEST_EXPECT_EQ(arg.sector.offset, 0x40000u);
    DMOD_TEST_EXPECT_EQ(arg.sector.size, 0x40000u);
    arg.index = 2;
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, DMFLASH_IOCTL_GET_SECTOR, &arg), -EINVAL);
}

DMOD_TEST_STEP(a_region_has_to_be_whole_sectors)
{
    close_node(&g_node);
    DMOD_TEST_EXPECT_NULL(make_region("0x1000", "rw"));                         /* Starts inside a sector */
    node_options_t ends_inside = { .size = "0x9000" };
    DMOD_TEST_EXPECT_NULL(make_node(&ends_inside));                             /* Ends inside one */
    node_options_t too_big = { .offset = "0x80000", .size = "0x100000" };
    DMOD_TEST_EXPECT_NULL(make_node(&too_big));                                 /* Past the end */
    DMOD_TEST_EXPECT_NULL(make_region("0x100000", "rw"));                       /* Starts at the end */
    node_options_t one_sector = { .offset = "0x20000", .size = "0x20000" };     /* The 128 kB sector: fine */
    dmdrvi_context_t n = make_node(&one_sector);
    DMOD_TEST_EXPECT_NOT_NULL(n);
    close_node(&n);
}

DMOD_TEST_STEP(a_bad_configuration_does_not_make_a_node)
{
    close_node(&g_node);
    node_options_t bits = { .program_bits = "12" };
    DMOD_TEST_EXPECT_NULL(make_node(&bits));
    node_options_t mode = { .mode = "writeable" };
    DMOD_TEST_EXPECT_NULL(make_node(&mode));
    node_options_t number = { .offset = "soon" };
    DMOD_TEST_EXPECT_NULL(make_node(&number));
    DMOD_TEST_EXPECT_NULL(g_create(NULL, &g_number));
    dmini_context_t ini = dmini_create();                                       /* No layout at all */
    dmini_set_string(ini, "flash", "driver_name", "dmflash");
    DMOD_TEST_EXPECT_NULL(g_create(ini, &g_number));
    dmini_destroy(ini);
}

/* (A table of strings is data with relocations, which a module cannot have: one call for each) */
static bool layout_opens(const char* layout)
{
    dmdrvi_context_t n = make_layout(layout);
    bool opened = n != NULL;
    close_node(&n);
    return opened;
}

DMOD_TEST_STEP(bad_layouts_do_not_make_a_node)
{
    close_node(&g_node);
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
}

DMOD_TEST_STEP(layouts_may_be_written_in_several_ways)
{
    close_node(&g_node);
    DMOD_TEST_EXPECT_TRUE(layout_opens(" 4 x 32K , 1x128k,3*256k "));
    DMOD_TEST_EXPECT_TRUE(layout_opens("0x8000"));                              /* One sector, in hex */
    dmdrvi_context_t n = make_layout("0x8000");
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(g_ioctl(n, NULL, DMFLASH_IOCTL_GET_INFO, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.sector_count, 1u);
    DMOD_TEST_EXPECT_EQ(info.size, 0x8000u);
    close_node(&n);
}

DMOD_TEST_STEP(a_second_node_may_not_overlap_the_first)
{
    DMOD_TEST_EXPECT_NULL(make_default());                                      /* (the whole flash is taken) */
    close_node(&g_node);
    g_node = make_default();
    DMOD_TEST_EXPECT_NOT_NULL(g_node);
}

DMOD_TEST_STEP(nodes_with_separate_regions_share_the_controller)
{
    close_node(&g_node);
    node_options_t fs = { .section = "fs", .offset = "0", .size = "0x20000" };         /* Sectors 0 to 3 */
    node_options_t bank = { .section = "bank_b", .offset = "0x80000" };                /* Sectors 6 and 7 */
    dmdrvi_context_t a = make_node(&fs);
    DMOD_TEST_EXPECT_NOT_NULL(a);
    DMOD_TEST_EXPECT_EQ(strcmp(g_number.alt_name, "fs"), 0);
    dmdrvi_context_t b = make_node(&bank);
    DMOD_TEST_EXPECT_NOT_NULL(b);
    DMOD_TEST_EXPECT_EQ(strcmp(g_number.alt_name, "bank_b"), 0);
    uint8_t one = 0x11, two = 0x22, back = 0;
    DMOD_TEST_EXPECT_EQ(wr(a, 0, &one, 1), 1);
    DMOD_TEST_EXPECT_EQ(wr(b, 0, &two, 1), 1);
    DMOD_TEST_EXPECT_EQ(rd(a, 0, &back, 1), 1);
    DMOD_TEST_EXPECT_EQ(back, 0x11);
    DMOD_TEST_EXPECT_EQ(rd(b, 0, &back, 1), 1);
    DMOD_TEST_EXPECT_EQ(back, 0x22);
    DMOD_TEST_EXPECT_EQ(er(b, 0, 0x40000), 0);                                  /* (an erase in one node ...) */
    DMOD_TEST_EXPECT_EQ(rd(a, 0, &back, 1), 1);
    DMOD_TEST_EXPECT_EQ(back, 0x11);                                            /* (... leaves the other alone) */
    DMOD_TEST_EXPECT_EQ(er(a, 0x40000, 0x8000), -EINVAL);                       /* Not its region */
    close_node(&a);                                                             /* The controller stays open for b */
    DMOD_TEST_EXPECT_EQ(wr(b, 0, &two, 1), 1);
    close_node(&b);
    a = make_node(&fs);                                                         /* ... and closes with the last */
    DMOD_TEST_EXPECT_NOT_NULL(a);
    close_node(&a);
}

DMOD_TEST_STEP(nodes_have_to_agree_on_the_chip)
{
    close_node(&g_node);
    node_options_t fs = { .section = "fs", .offset = "0", .size = "0x20000" };
    dmdrvi_context_t a = make_node(&fs);
    DMOD_TEST_EXPECT_NOT_NULL(a);
    node_options_t overlapping = { .section = "other", .offset = "0x18000", .size = "0x8000" };
    DMOD_TEST_EXPECT_NULL(make_node(&overlapping));
    node_options_t width = { .section = "other", .offset = "0x80000", .program_bits = "16" };
    DMOD_TEST_EXPECT_NULL(make_node(&width));
    node_options_t layout = { .section = "other", .layout = "1x1m", .offset = "0" , .size = "0x100000" };
    DMOD_TEST_EXPECT_NULL(make_node(&layout));
    node_options_t fine = { .section = "other", .offset = "0x20000", .size = "0x20000" };
    dmdrvi_context_t b = make_node(&fine);                                      /* (just next to it) */
    DMOD_TEST_EXPECT_NOT_NULL(b);
    close_node(&b);
    close_node(&a);
}

/* The text of a file as it is on the board: no base (the default), nothing but what the sections need */
DMOD_TEST_STEP(two_sections_of_a_board_file_make_two_nodes)
{
    close_node(&g_node);
    static const char text[] =
        "[bank_a]\ndriver_name=dmflash\ndriver_order=0\nlayout=4x32k,1x128k,3x256k\noffset=0x80000\nsize=0x40000\nmode=rw\n\n"
        "[bank_b]\ndriver_name=dmflash\ndriver_order=0\nlayout=4x32k,1x128k,3x256k\noffset=0xC0000\nsize=0x40000\nmode=rw\n";
    dmini_context_t ini = dmini_create();
    DMOD_TEST_EXPECT_TRUE(dmini_parse_string(ini, text) == 0);
    DMOD_TEST_EXPECT_EQ(dmini_set_active_section(ini, "bank_a", 0), 0);
    dmdrvi_context_t a = g_create(ini, &g_number);
    DMOD_TEST_EXPECT_NOT_NULL(a);
    DMOD_TEST_EXPECT_EQ(dmini_clear_active_section(ini, 0), 0);
    DMOD_TEST_EXPECT_EQ(dmini_set_active_section(ini, "bank_b", 0), 0);
    dmdrvi_context_t b = g_create(ini, &g_number);
    DMOD_TEST_EXPECT_NOT_NULL(b);
    DMOD_TEST_EXPECT_EQ(strcmp(g_number.alt_name, "bank_b"), 0);
    close_node(&a);
    close_node(&b);
    dmini_destroy(ini);
}

DMOD_TEST_STEP(a_file_of_several_sections_makes_a_node_of_each_active_section)
{
    close_node(&g_node);
    dmini_context_t ini = dmini_create();
    char base[2 * sizeof(uintptr_t) + 3];
    hex(base, g_base);
    dmini_set_string(ini, "fs", "layout", LAYOUT);
    dmini_set_string(ini, "fs", "base", base);
    dmini_set_string(ini, "fs", "size", "0x20000");
    dmini_set_string(ini, "bank_b", "layout", LAYOUT);
    dmini_set_string(ini, "bank_b", "base", base);
    dmini_set_string(ini, "bank_b", "offset", "0x80000");
    DMOD_TEST_EXPECT_EQ(dmini_set_active_section(ini, "bank_b", 0), 0);        /* (what dmdevfs does) */
    dmdrvi_context_t b = g_create(ini, &g_number);
    DMOD_TEST_EXPECT_NOT_NULL(b);
    DMOD_TEST_EXPECT_EQ(strcmp(g_number.alt_name, "bank_b"), 0);
    DMOD_TEST_EXPECT_EQ(dmini_set_active_section(ini, "fs", 0), 0);
    dmdrvi_context_t a = g_create(ini, &g_number);
    DMOD_TEST_EXPECT_NOT_NULL(a);
    DMOD_TEST_EXPECT_EQ(strcmp(g_number.alt_name, "fs"), 0);
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(g_ioctl(a, NULL, DMFLASH_IOCTL_GET_INFO, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.size, 0x20000u);
    DMOD_TEST_EXPECT_EQ(g_ioctl(b, NULL, DMFLASH_IOCTL_GET_INFO, &info), 0);
    DMOD_TEST_EXPECT_EQ(info.size, 0x80000u);
    close_node(&a);
    close_node(&b);
    dmini_destroy(ini);
}

DMOD_TEST_STEP(erased_flash_reads_as_ff)
{
    uint8_t buffer[16];
    DMOD_TEST_EXPECT_EQ(rd(g_node, 0x1234, buffer, sizeof(buffer)), (dmdrvi_ssize_t)sizeof(buffer));
    for (size_t i = 0; i < sizeof(buffer); i++)
        DMOD_TEST_EXPECT_EQ(buffer[i], 0xFFu);
}

DMOD_TEST_STEP(write_then_read_at_any_offset_and_size)
{
    uint8_t data[700], back[700];
    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = (uint8_t)(i * 7 + 1);
    uint32_t at = 0;
    for (uint32_t lead = 0; lead < 8; lead++)
    {
        for (uint32_t size = 1; size < sizeof(data); size = size * 2 + 1)
        {
            at += 0x400;                                /* (a new place every time: nothing is erased in between) */
            DMOD_TEST_EXPECT_EQ(wr(g_node, at + lead, data, size), (dmdrvi_ssize_t)size);
            memset(back, 0, sizeof(back));
            DMOD_TEST_EXPECT_EQ(rd(g_node, at + lead, back, size), (dmdrvi_ssize_t)size);
            DMOD_TEST_EXPECT_EQ(memcmp(back, data, size), 0);
        }
    }
}

DMOD_TEST_STEP(a_write_leaves_its_neighbours_alone)
{
    uint8_t data[5] = { 1, 2, 3, 4, 5 }, back[11];
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0x103, data, sizeof(data)), 5);
    DMOD_TEST_EXPECT_EQ(rd(g_node, 0x100, back, sizeof(back)), (dmdrvi_ssize_t)sizeof(back));
    static const uint8_t expected[11] = { 0xFF, 0xFF, 0xFF, 1, 2, 3, 4, 5, 0xFF, 0xFF, 0xFF };
    DMOD_TEST_EXPECT_EQ(memcmp(back, expected, sizeof(expected)), 0);
}

DMOD_TEST_STEP(writing_over_data_that_is_not_erased_fails_until_it_is_erased)
{
    uint8_t one[8] = { 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F };
    uint8_t other[8] = { 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0 };
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0x40, one, sizeof(one)), 8);
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0x40, other, sizeof(other)), -EIO);
    DMOD_TEST_EXPECT_EQ(er(g_node, 0, 0x8000), 0);
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0x40, other, sizeof(other)), 8);
}

DMOD_TEST_STEP(erase_takes_whole_sectors_only)
{
    DMOD_TEST_EXPECT_EQ(er(g_node, 0x10, 0x8000), -EINVAL);                     /* Starts inside */
    DMOD_TEST_EXPECT_EQ(er(g_node, 0, 0x4000), -EINVAL);                        /* Ends inside */
    DMOD_TEST_EXPECT_EQ(er(g_node, 0x18000, 0x10000), -EINVAL);                 /* Ends in the middle of the 128 kB sector */
    DMOD_TEST_EXPECT_EQ(er(g_node, 0, FLASH_SIZE + 1), -EINVAL);
    DMOD_TEST_EXPECT_EQ(er(g_node, 0x20000, 0), 0);                             /* Nothing to do */
    DMOD_TEST_EXPECT_EQ(dmflash_port_mock_erases(), 0u);                        /* (a refused range erases nothing) */
}

DMOD_TEST_STEP(erase_clears_the_sectors_of_a_range)
{
    uint8_t data[4] = { 0, 0, 0, 0 }, back[4];
    uint32_t places[6];
    places[0] = 0x7FFC; places[1] = 0x8000; places[2] = 0x1FFFC; places[3] = 0x20000; places[4] = 0x3FFFC; places[5] = 0x40000;
    for (size_t i = 0; i < 6; i++)
        DMOD_TEST_EXPECT_EQ(wr(g_node, places[i], data, sizeof(data)), 4);
    DMOD_TEST_EXPECT_EQ(er(g_node, 0x8000, 0x38000), 0);                        /* Sectors 1 to 4 (the last is the 128 kB one) */
    DMOD_TEST_EXPECT_EQ(dmflash_port_mock_erases(), 4u);
    for (size_t i = 0; i < 6; i++)
    {
        DMOD_TEST_EXPECT_EQ(rd(g_node, places[i], back, sizeof(back)), 4);
        uint8_t expected = (places[i] == 0x7FFC || places[i] == 0x40000) ? 0x00 : 0xFF;      /* Sectors 0 and 5 were not erased */
        DMOD_TEST_EXPECT_EQ(back[0], expected);
    }
}

DMOD_TEST_STEP(a_read_only_region_refuses_writes_and_erases)
{
    close_node(&g_node);
    g_node = make_region("0x80000", "ro");
    DMOD_TEST_EXPECT_NOT_NULL(g_node);
    uint8_t byte = 0;
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0, &byte, 1), -EROFS);
    DMOD_TEST_EXPECT_EQ(er(g_node, 0, 0x40000), -EROFS);
    DMOD_TEST_EXPECT_EQ(rd(g_node, 0, &byte, 1), 1);
    dmdrvi_block_info_t block;
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, DMDRVI_IOCTL_BLOCK_GET_INFO, &block), 0);
    DMOD_TEST_EXPECT_TRUE((block.flags & DMDRVI_BLOCK_FLAG_READ_ONLY) != 0);
}

DMOD_TEST_STEP(nothing_outside_the_region_can_be_touched)
{
    close_node(&g_node);
    g_node = make_region("0x80000", "rw");
    uint8_t data[8] = { 0 };
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0x80000, data, 1), -EINVAL);
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0x7FFFF, data, 2), -EINVAL);
    DMOD_TEST_EXPECT_EQ(rd(g_node, 0x80000, data, 1), -EINVAL);
    DMOD_TEST_EXPECT_EQ(er(g_node, 0x80000, 0x40000), -EINVAL);
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0x7FFFC, data, 4), 4);                       /* The last bytes: fine */
    uint8_t below;
    memcpy(&below, (const void*)(g_base + 0x7FFFF), 1);
    DMOD_TEST_EXPECT_EQ(below, 0xFFu);                                          /* (the sector below the region) */
}

DMOD_TEST_STEP(the_port_failing_is_reported_and_the_next_write_works)
{
    uint8_t data[4] = { 1, 2, 3, 4 };
    dmflash_port_mock_fail_set(-ETIMEDOUT);
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0, data, sizeof(data)), -ETIMEDOUT);
    dmflash_port_mock_fail_set(-EIO);
    DMOD_TEST_EXPECT_EQ(er(g_node, 0, 0x8000), -EIO);
    DMOD_TEST_EXPECT_EQ(er(g_node, 0, 0x8000), 0);
    DMOD_TEST_EXPECT_EQ(wr(g_node, 0, data, sizeof(data)), 4);
}

DMOD_TEST_STEP(a_foreign_context_and_unknown_commands_are_refused)
{
    uint8_t byte;
    DMOD_TEST_EXPECT_EQ(rd(NULL, 0, &byte, 1), -EINVAL);
    dmflash_info_t info;
    DMOD_TEST_EXPECT_EQ(g_ioctl(NULL, NULL, DMFLASH_IOCTL_GET_INFO, &info), -EINVAL);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, DMFLASH_IOCTL_GET_INFO, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, 0x7777, &info), -ENOTTY);
    DMOD_TEST_EXPECT_EQ(rd(g_node, 0xFFFFFFFFu, &byte, 1), -EINVAL);
    close_node(&g_node);
    g_node = make_default();                                                    /* (the controller was released) */
    DMOD_TEST_EXPECT_NOT_NULL(g_node);
}
#endif
