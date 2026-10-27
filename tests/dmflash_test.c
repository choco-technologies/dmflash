#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmflash.h"

static dmflash_t g_handle = NULL;

void dmod_test_setup(void)
{
    g_handle = dmflash_create();
}

void dmod_test_teardown(void)
{
    dmflash_destroy(g_handle);
    g_handle = NULL;
}

DMOD_TEST_STEP(dmflash_create)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_handle);
}

DMOD_TEST_STEP(dmflash_is_valid)
{
    DMOD_TEST_EXPECT_TRUE(dmflash_is_valid(g_handle));
}

DMOD_TEST_STEP(dmflash_destroy_null)
{
    /* Destroying NULL must not crash. */
    dmflash_destroy(NULL);
}
