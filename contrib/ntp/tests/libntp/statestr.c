#include "config.h"

#include "ntp_stdlib.h"
#include "ntp.h" // needed for MAX_MAC_LEN used in ntp_control.h
#include "ntp_control.h"

#include "unity.h"

void test_PeerRestart(void);
void test_SysUnspecified(void);
void test_ClockCodeExists(void);
void test_ClockCodeUnknown(void);

// eventstr()
void
test_PeerRestart(void) {
	TEST_ASSERT_EQUAL_STRING("restart", eventstr(PEVNT_RESTART));
}


void
test_SysUnspecified(void) {
	TEST_ASSERT_EQUAL_STRING("unspecified", eventstr(EVNT_UNSPEC));
}


// ceventstr()
void
test_ClockCodeExists(void) {
	TEST_ASSERT_EQUAL_STRING("clk_unspec", ceventstr(CTL_CLK_OKAY));
}


void
test_ClockCodeUnknown(void) {
	TEST_ASSERT_EQUAL_STRING("clk_-1", ceventstr(-1));
}
