#include <stdio.h>
#include <limits.h>

#include "test_ccapi_check.h"
#include "test_ccapi_constants.h"
#include "test_ccapi_context.h"
#include "test_ccapi_ccache.h"

int main (int argc, const char * argv[]) {

	cc_int32 err = ccNoError;
	T_CCAPI_INIT;
	err = check_constants();
    return err;
}
