#include "g_libntptest.h"

class modetoaTest : public libntptest {
};

TEST_F(modetoaTest, KnownMode) {
	const int MODE = 3; // Should be "client"

	EXPECT_STREQ("client", modetoa(MODE));
}

TEST_F(modetoaTest, UnknownMode) {
	const int MODE = 100;

	EXPECT_STREQ("mode#100", modetoa(MODE));
}
