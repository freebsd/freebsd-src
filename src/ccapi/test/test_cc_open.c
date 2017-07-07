#include <stdio.h>
#include <limits.h>

#include "test_ccapi_check.h"
#include "test_ccapi_constants.h"
#include "test_ccapi_v2.h"

int main (int argc, const char * argv[]) {

    cc_int32 err = ccNoError;
    T_CCAPI_INIT;
    err = check_cc_open();
    return err;
}
