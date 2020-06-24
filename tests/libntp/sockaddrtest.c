
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
		if (   expected.sa4.sin_port == actual.sa4.sin_port
		    && memcmp(&expected.sa4.sin_addr, &actual.sa4.sin_addr,
			      sizeof( in )) == 0) {
			return TRUE;
		} else {
			char buf[4][32];
			strlcpy(buf[0], inet_ntoa(expected.sa4.sin_addr), sizeof(buf[0]));
			strlcpy(buf[1], socktoa(&expected)              , sizeof(buf[1]));
			strlcpy(buf[2], inet_ntoa(actual.sa4.sin_addr)  , sizeof(buf[2]));
			strlcpy(buf[3], socktoa(&actual)                , sizeof(buf[3]));
			printf("IPv4 comparision failed, expected: %s(%s) but was: %s(%s)",
			       buf[0], buf[1], buf[2], buf[3]);
			return FALSE;
		}
	} else if (actual.sa.sa_family == AF_INET6) { //IPv6
		if (   expected.sa6.sin6_port == actual.sa6.sin6_port
		    && expected.sa6.sin6_scope_id == actual.sa6.sin6_scope_id
		    && memcmp(&expected.sa6.sin6_addr, &actual.sa6.sin6_addr,
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

