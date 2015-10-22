#include "config.h"

#include "ntp_net.h"
#include "ntp_refclock.h"

#include "unity.h"


/* Might need to be updated if a new refclock gets this id. */
static const int UNUSED_REFCLOCK_ID = 250;

void test_LocalClock(void);
void test_UnknownId(void);


void
test_LocalClock(void) {
#ifdef REFCLOCK		/* clockname() is useless otherwise */
	/* We test with a refclock address of type LOCALCLOCK.
	 * with id 8
	 */
	u_int32 addr = REFCLOCK_ADDR;
	addr |= REFCLK_LOCALCLOCK << 8;
	addr |= 0x8;

	sockaddr_u address;
	address.sa4.sin_family = AF_INET;
	address.sa4.sin_addr.s_addr = htonl(addr);
	
	char stringStart[100]= "";

	strcat(stringStart, clockname(REFCLK_LOCALCLOCK));
	strcat(stringStart, "(8)");

	char * expected = stringStart;

	TEST_ASSERT_EQUAL_STRING(expected, refnumtoa(&address));
#else	
	TEST_IGNORE_MESSAGE("REFCLOCK NOT DEFINED, SKIPPING TEST");
#endif	/* REFCLOCK */
}

void
test_UnknownId(void) {
#ifdef REFCLOCK		/* refnumtoa() is useless otherwise */
	/* We test with a currently unused refclock ID */
	u_int32 addr = REFCLOCK_ADDR;
	addr |= UNUSED_REFCLOCK_ID << 8;
	addr |= 0x4;

	sockaddr_u address;
	address.sa4.sin_family = AF_INET;
	address.sa4.sin_addr.s_addr = htonl(addr);
	
	char stringStart[100]= "REFCLK(";
	char value[100] ;	
	snprintf(value, sizeof(value), "%d", UNUSED_REFCLOCK_ID);
	strcat(stringStart,value);
	strcat(stringStart,",4)");
	char * expected = stringStart;

	TEST_ASSERT_EQUAL_STRING(expected, refnumtoa(&address));
#else 	
	TEST_IGNORE_MESSAGE("REFCLOCK NOT DEFINED, SKIPPING TEST");
#endif	/* REFCLOCK */
}
