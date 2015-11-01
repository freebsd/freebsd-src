#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_fp.h"

#include "unity.h"

void test_LoopbackNetNonResolve(void);

void
test_LoopbackNetNonResolve(void) {
	/* A loopback address in 127.0.0.0/8 is chosen, and
	 * numtohost() should not try to resolve it unless
	 * it is 127.0.0.1
	 */

	const u_int32 input = 127*256*256*256 + 1*256 + 1; // 127.0.1.1
	
	TEST_ASSERT_EQUAL_STRING("127.0.1.1", numtohost(htonl(input)));
}
