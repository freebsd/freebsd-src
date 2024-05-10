#include "config.h"

#include "ntp_net.h"
#include "ntp_refclock.h"

#include "unity.h"


void setUp(void);
void test_LocalClock(void);
void test_UnknownId(void);


void
setUp(void)
{
	init_lib();

	return;
}


void
test_LocalClock(void) {
#ifdef REFCLOCK		/* clockname() is useless otherwise */
	/* We test with a refclock address of type LOCALCLOCK.
	 * with unit id 8
	 */
	const u_char unit = 8;
	u_int32 addr;
	char expected[100];
	sockaddr_u address;

	addr = REFCLOCK_ADDR;
	addr |= REFCLK_LOCALCLOCK << 8;
	addr |= unit;

	AF(&address) = AF_INET;
	NSRCADR(&address) = htonl(addr);
	snprintf(expected, sizeof(expected), "%s(%u)",
		 clockname(REFCLK_LOCALCLOCK), unit);

	TEST_ASSERT_EQUAL_STRING(expected, refnumtoa(&address));
#else
	TEST_IGNORE_MESSAGE("REFCLOCK NOT DEFINED, SKIPPING TEST");
#endif	/* REFCLOCK */
}

void
test_UnknownId(void) {
#ifdef REFCLOCK		/* refnumtoa() is useless otherwise */
	/* We test with a currently unused refclock ID */
	/* Might need to be updated if a new refclock gets this id. */
	const u_char UNUSED_REFCLOCK_ID = 250;
	const u_char unit = 4;
	u_int32 addr;
	char expected[100];
	sockaddr_u address;

	addr = REFCLOCK_ADDR;
	addr |= UNUSED_REFCLOCK_ID << 8;
	addr |= unit;

	AF(&address) = AF_INET;
	NSRCADR(&address) = htonl(addr);

	snprintf(expected, sizeof(expected), "REFCLK(%u,%u)",
		 UNUSED_REFCLOCK_ID, unit);

	TEST_ASSERT_EQUAL_STRING(expected, refnumtoa(&address));
#else
	TEST_IGNORE_MESSAGE("REFCLOCK NOT DEFINED, SKIPPING TEST");
#endif	/* REFCLOCK */
}
