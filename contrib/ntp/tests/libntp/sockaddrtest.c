
#include "config.h"
#include "ntp.h"
#include "ntp_stdlib.h"
#include "sockaddrtest.h"

sockaddr_u
CreateSockaddr4(const char* address, unsigned int port) {
	sockaddr_u s;
	s.sa4.sin_family = AF_INET;
	s.sa4.sin_addr.s_addr = inet_addr(address);
	SET_PORT(&s, port);

	return s;
}


int
IsEqual(const sockaddr_u expected, const sockaddr_u actual) {
	struct in_addr in;
	struct in6_addr in6;

	if (expected.sa.sa_family != actual.sa.sa_family) {
		printf("Expected sa_family: %d but got: %d", expected.sa.sa_family, actual.sa.sa_family);
		return FALSE;
	}

	if (actual.sa.sa_family == AF_INET) { // IPv4
		if (expected.sa4.sin_port == actual.sa4.sin_port &&
			memcmp(&expected.sa4.sin_addr, &actual.sa4.sin_addr,
				   sizeof( in )) == 0) {
			return TRUE;
		} else {
			printf("IPv4 comparision failed, expected: %s(%s) but was: %s(%s)",inet_ntoa(expected.sa4.sin_addr), socktoa(&expected), inet_ntoa(actual.sa4.sin_addr),socktoa(&actual));
			return FALSE;
		}
	} else if (actual.sa.sa_family == AF_INET6) { //IPv6
		if (expected.sa6.sin6_port == actual.sa6.sin6_port &&
			memcmp(&expected.sa6.sin6_addr, &actual.sa6.sin6_addr,
				   sizeof(in6)) == 0) {
			return TRUE;
		} else {
			printf("IPv6 comparision failed");
			return FALSE;
		}
	} else { // Unknown family
		printf("Unknown sa_family: %d",actual.sa.sa_family);
		return FALSE;
	}
}

