#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"

#include "unity.h"
#include "sockaddrtest.h"


void setUp(void);
void test_IPv4AddressWithPort(void);
void test_IPv6AddressWithPort(void);
void test_IgnoreIPv6Fields(void);
void test_ScopedIPv6AddressWithPort(void);
void test_HashEqual(void);
void test_HashNotEqual(void);


void
setUp(void)
{
	init_lib();
}


void 
test_IPv4AddressWithPort(void)
{
	sockaddr_u input = CreateSockaddr4("192.0.2.10", 123);

	TEST_ASSERT_EQUAL_STRING("192.0.2.10", socktoa(&input));
	TEST_ASSERT_EQUAL_STRING("192.0.2.10:123", sockporttoa(&input));
}


void 
test_IPv6AddressWithPort(void)
{
#ifdef ISC_PLATFORM_WANTIPV6

	const struct in6_addr address = {
		0x20, 0x01, 0x0d, 0xb8,
		0x85, 0xa3, 0x08, 0xd3, 
		0x13, 0x19, 0x8a, 0x2e,
		0x03, 0x70, 0x73, 0x34
	};

	const char* expected =
		"2001:db8:85a3:8d3:1319:8a2e:370:7334";
	const char* expected_port = 
		"[2001:db8:85a3:8d3:1319:8a2e:370:7334]:123";

	sockaddr_u input;
	memset(&input, 0, sizeof(input));
	AF(&input) = AF_INET6;
	SET_ADDR6N(&input, address);
	SET_PORT(&input, 123);

	TEST_ASSERT_EQUAL_STRING(expected, socktoa(&input));
	TEST_ASSERT_EQUAL_STRING(expected_port, sockporttoa(&input));

#else

	TEST_IGNORE_MESSAGE("IPV6 disabled in build, skipping.");

#endif /* ISC_PLATFORM_HAVEIPV6 */
}


void 
test_ScopedIPv6AddressWithPort(void)
{
#ifdef ISC_PLATFORM_HAVESCOPEID
    
	const struct in6_addr address = { { {
		0xfe, 0x80, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x02, 0x12, 0x3f, 0xff, 
		0xfe, 0x29, 0xff, 0xfa
	} } };

	const char* expected =
		"fe80::212:3fff:fe29:fffa%5";
	const char* expected_port = 
		"[fe80::212:3fff:fe29:fffa%5]:123";

	sockaddr_u input;
	memset(&input, 0, sizeof(input));
	AF(&input) = AF_INET6;
	SET_ADDR6N(&input, address);
	SET_PORT(&input, 123);
	SCOPE_VAR(&input) = 5;

	TEST_ASSERT_EQUAL_STRING(expected, socktoa(&input));
	TEST_ASSERT_EQUAL_STRING(expected_port, sockporttoa(&input));
#else
	
	TEST_IGNORE_MESSAGE("Skipping because ISC_PLATFORM does not have Scope ID");
	
#endif
}


void 
test_HashEqual(void)
{
	sockaddr_u input1 = CreateSockaddr4("192.00.2.2", 123);
	sockaddr_u input2 = CreateSockaddr4("192.0.2.2", 123);

	TEST_ASSERT_TRUE(IsEqual(input1, input2));
	TEST_ASSERT_EQUAL(sock_hash(&input1), sock_hash(&input2));
}


void 
test_HashNotEqual(void)
{
	/* These two addresses should not generate the same hash. */
	sockaddr_u input1 = CreateSockaddr4("192.0.2.1", 123);
	sockaddr_u input2 = CreateSockaddr4("192.0.2.2", 123);

	TEST_ASSERT_FALSE(IsEqual(input1, input2));
	TEST_ASSERT_FALSE(sock_hash(&input1) == sock_hash(&input2)); 
}


void 
test_IgnoreIPv6Fields(void)
{
#ifdef ISC_PLATFORM_WANTIPV6

	const struct in6_addr address = {
		0x20, 0x01, 0x0d, 0xb8,
		0x85, 0xa3, 0x08, 0xd3, 
		0x13, 0x19, 0x8a, 0x2e,
		0x03, 0x70, 0x73, 0x34
	};

	sockaddr_u input1, input2;

	input1.sa6.sin6_family = AF_INET6;
	input1.sa6.sin6_addr = address;
	input1.sa6.sin6_flowinfo = 30L; // This value differs from input2.
	SET_PORT(&input1, NTP_PORT);

	input2.sa6.sin6_family = AF_INET6;
	input2.sa6.sin6_addr = address;
	input2.sa6.sin6_flowinfo = 10L; // This value differs from input1.
	SET_PORT(&input2, NTP_PORT);

	TEST_ASSERT_EQUAL(sock_hash(&input1), sock_hash(&input2));

#else

	TEST_IGNORE_MESSAGE("IPV6 disabled in build, skipping.");

#endif /* ISC_PLATFORM_HAVEIPV6 */
}
