#include "libntptest.h"

class numtohostTest : public libntptest {
};

TEST_F(numtohostTest, LoopbackNetNonResolve) {
	/* A loopback address in 127.0.0.0/8 is chosen, and
	 * numtohost() should not try to resolve it unless
	 * it is 127.0.0.1
	 */

	u_int32 input = 127*256*256*256 + 1*256 + 1; // 127.0.1.1
	
	EXPECT_STREQ("127.0.1.1", numtohost(htonl(input)));
}
