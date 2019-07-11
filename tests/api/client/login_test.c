#include <CUnit/Basic.h>
#include <ela_hive.h>
#include <config.h>

#include "test_context.h"
#include "test_helper.h"

static void test_login(void)
{
    int rc;

    rc = hive_client_login(test_ctx.client, open_authorization_url, NULL);
    CU_ASSERT_EQUAL(rc, HIVEOK);
}

static CU_TestInfo cases[] = {
    { "test_login", test_login },
    { NULL, NULL }
};

CU_TestInfo *login_test_get_cases(void)
{
    return cases;
}

int onedrive_login_test_suite_init(void)
{
    test_ctx.client = onedrive_client_new();
    if (!test_ctx.client) {
        CU_FAIL("Error: test suite initialize error");
        return -1;
    }

    return 0;
}

int onedrive_login_test_suite_cleanup(void)
{
    test_context_cleanup();

    return 0;
}

int ipfs_login_test_suite_init(void)
{
    test_ctx.client = ipfs_client_new();
    if (!test_ctx.client) {
        CU_FAIL("Error: test suite initialize error");
        return -1;
    }

    return 0;
}

int ipfs_login_test_suite_cleanup(void)
{
    test_context_cleanup();

    return 0;
}
