#include "config.h"
#include "ntp_stdlib.h"
#include "sockaddrtest.h"

#include "unity.h"

void setUp(void);
extern void test_IPv4AddressOnly(void);
extern void test_IPv4AddressWithPort(void);
//#ifdef ISC_PLATFORM_HAVEIPV6
extern void test_IPv6AddressOnly(void);
extern void test_IPv6AddressWithPort(void);
//#endif /* ISC_PLATFORM_HAVEIPV6 */
extern void test_IllegalAddress(void);
extern void test_IllegalCharInPort(void);


void
setUp(void)
{
	init_lib();

	return;
}


void
test_IPv4AddressOnly(void) {
	const char *str = "192.0.2.1";
	sockaddr_u actual;

	sockaddr_u expected;
	expected.sa4.sin_family = AF_INET;
	expected.sa4.sin_addr.s_addr = inet_addr("192.0.2.1");
	SET_PORT(&expected, NTP_PORT);

	TEST_ASSERT_TRUE(decodenetnum(str, &actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void
test_IPv4AddressWithPort(void) {
	const char *str = "192.0.2.2:2000";
	sockaddr_u actual;

	sockaddr_u expected;
	expected.sa4.sin_family = AF_INET;
	expected.sa4.sin_addr.s_addr = inet_addr("192.0.2.2");
	SET_PORT(&expected, 2000);

	TEST_ASSERT_TRUE(decodenetnum(str, &actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}


void
test_IPv6AddressOnly(void) {

//#ifdef ISC_PLATFORM_HAVEIPV6 //looks like HAVEIPV6 checks if system has IPV6 capabilies. WANTIPV6 can be changed with build --disable-ipv6
#ifdef ISC_PLATFORM_WANTIPV6
	const struct in6_addr address = {
		0x20, 0x01, 0x0d, 0xb8,
        0x85, 0xa3, 0x08, 0xd3,
        0x13, 0x19, 0x8a, 0x2e,
        0x03, 0x70, 0x73, 0x34
	};

	const char *str = "2001:0db8:85a3:08d3:1319:8a2e:0370:7334";
	sockaddr_u actual;

	sockaddr_u expected;
	expected.sa6.sin6_family = AF_INET6;
	expected.sa6.sin6_addr = address;
	SET_PORT(&expected, NTP_PORT);

	TEST_ASSERT_TRUE(decodenetnum(str, &actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual));

#else
	TEST_IGNORE_MESSAGE("IPV6 disabled in build, skipping.");
#endif /* ISC_PLATFORM_HAVEIPV6 */


}



void
test_IPv6AddressWithPort(void) {

#ifdef ISC_PLATFORM_WANTIPV6

	const struct in6_addr address = {
		0x20, 0x01, 0x0d, 0xb8,
        0x85, 0xa3, 0x08, 0xd3,
        0x13, 0x19, 0x8a, 0x2e,
        0x03, 0x70, 0x73, 0x34
	};

	const char *str = "[2001:0db8:85a3:08d3:1319:8a2e:0370:7334]:3000";
	sockaddr_u actual;

	sockaddr_u expected;
	expected.sa6.sin6_family = AF_INET6;
	expected.sa6.sin6_addr = address;
	SET_PORT(&expected, 3000);

	TEST_ASSERT_TRUE(decodenetnum(str, &actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual));

#else
	TEST_IGNORE_MESSAGE("IPV6 disabled in build, skipping.");
#endif /* ISC_PLATFORM_HAVEIPV6 */
}


void
test_IllegalAddress(void) {
	const char *str = "192.0.2.270:2000";
	sockaddr_u actual;

	TEST_ASSERT_FALSE(decodenetnum(str, &actual));
}

void
test_IllegalCharInPort(void) {
	/* An illegal port does not make the decodenetnum fail, but instead
	 * makes it use the standard port.
	 */
	const char *str = "192.0.2.1:a700";
	sockaddr_u actual;

	sockaddr_u expected;
	expected.sa4.sin_family = AF_INET;
	expected.sa4.sin_addr.s_addr = inet_addr("192.0.2.1");
	SET_PORT(&expected, NTP_PORT);

	TEST_ASSERT_TRUE(decodenetnum(str, &actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}
