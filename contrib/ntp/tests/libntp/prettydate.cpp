#include "libntptest.h"

extern "C" {
#include "ntp_fp.h"
};

class prettydateTest : public libntptest {
protected:
	static const u_int32 HALF = 2147483648UL;
};

TEST_F(prettydateTest, ConstantDate) {
	l_fp time = {3485080800UL, HALF}; // 2010-06-09 14:00:00.5

	ASSERT_STREQ("cfba1ce0.80000000  Wed, Jun  9 2010 14:00:00.500", gmprettydate(&time));
}
