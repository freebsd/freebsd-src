#ifndef TESTS_SOCKADDRTEST_H
#define TESTS_SOCKADDRTEST_H

#include "g_libntptest.h"

extern "C" {
#include "ntp.h"
};

class sockaddrtest : public libntptest {
protected:
	::testing::AssertionResult IsEqual(const sockaddr_u &expected, const sockaddr_u &actual) {
		if (expected.sa.sa_family != actual.sa.sa_family) {
			return ::testing::AssertionFailure()
				<< "Expected sa_family: " << expected.sa.sa_family
				<< " but got: " << actual.sa.sa_family;
		}

		if (actual.sa.sa_family == AF_INET) { // IPv4
			if (expected.sa4.sin_port == actual.sa4.sin_port &&
				memcmp(&expected.sa4.sin_addr, &actual.sa4.sin_addr,
					   sizeof(in_addr)) == 0) {
				return ::testing::AssertionSuccess();
			} else {
				return ::testing::AssertionFailure()
					<< "IPv4 comparision failed, expected: "
					<< expected.sa4.sin_addr.s_addr
					<< "(" << socktoa(&expected) << ")"
					<< " but was: "
					<< actual.sa4.sin_addr.s_addr
					<< "(" << socktoa(&actual) << ")";
			}
		} else if (actual.sa.sa_family == AF_INET6) { //IPv6
			if (expected.sa6.sin6_port == actual.sa6.sin6_port &&
				memcmp(&expected.sa6.sin6_addr, &actual.sa6.sin6_addr,
					   sizeof(in6_addr)) == 0) {
				return ::testing::AssertionSuccess();
			} else {
				return ::testing::AssertionFailure()
					<< "IPv6 comparision failed";
			}
		} else { // Unknown family
			return ::testing::AssertionFailure()
				<< "Unknown sa_family: " << actual.sa.sa_family;
		}
	}

	sockaddr_u CreateSockaddr4(const char* address, unsigned int port) {
		sockaddr_u s;
		s.sa4.sin_family = AF_INET;
		s.sa4.sin_addr.s_addr = inet_addr(address);
		SET_PORT(&s, port);

		return s;
	}
};

#endif // TESTS_SOCKADDRTEST_H

