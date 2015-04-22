#include "libntptest.h"

extern "C" {
#include "ntp_fp.h"
};

class uglydateTest : public libntptest {
protected:
	static const u_int32 HALF = 2147483648UL;
};

TEST_F(uglydateTest, ConstantDateTime) {
	l_fp time = {3485080800UL, HALF}; // 2010-06-09 14:00:00.5

	EXPECT_STREQ("3485080800.500000 10:159:14:00:00.500",
				 uglydate(&time));
}

