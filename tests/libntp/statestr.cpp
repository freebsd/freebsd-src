#include "libntptest.h"

extern "C" {
#include "ntp.h" // Needed for MAX_MAC_LEN used in ntp_control.h
#include "ntp_control.h"
};

class statestrTest : public libntptest {
};

// eventstr()
TEST_F(statestrTest, PeerRestart) {
	EXPECT_STREQ("restart", eventstr(PEVNT_RESTART));
}

TEST_F(statestrTest, SysUnspecified) {
	EXPECT_STREQ("unspecified", eventstr(EVNT_UNSPEC));
}

// ceventstr()
TEST_F(statestrTest, ClockCodeExists) {
	EXPECT_STREQ("clk_unspec", ceventstr(CTL_CLK_OKAY));
}

TEST_F(statestrTest, ClockCodeUnknown) {
	EXPECT_STREQ("clk_-1", ceventstr(-1));
}
