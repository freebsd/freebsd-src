#pragma once

/* Definitions mirrored from contrib/musl public headers. */

#include <sys/types.h>

#include <stdint.h>

#define MUSL_AF_UNSPEC	     0
#define MUSL_AF_INET	     2
#define MUSL_AF_INET6	     10
#define MUSL_SOCK_STREAM     1
#define MUSL_IPPROTO_TCP     6
#define MUSL_SOL_SOCKET	     1
#define MUSL_SO_ERROR	     4
#define MUSL_CLOCK_MONOTONIC 1
#define MUSL_EINTR	     4
#define MUSL_EAGAIN	     11
#define MUSL_EISCONN	     106
#define MUSL_EALREADY	     114
#define MUSL_EINPROGRESS     115

#define MUSL_POLLIN	     0x001
#define MUSL_POLLOUT	     0x004
#define MUSL_POLLERR	     0x008
#define MUSL_POLLHUP	     0x010
#define MUSL_POLLNVAL	     0x020

#define MUSL_F_GETFL	     3
#define MUSL_F_SETFL	     4
#define MUSL_O_NONBLOCK	     04000

typedef unsigned int musl_socklen_t;
typedef unsigned short musl_sa_family_t;
typedef uint16_t musl_in_port_t;
typedef uint32_t musl_in_addr_t;
typedef unsigned long musl_nfds_t;

typedef struct musl_in_addr {
	musl_in_addr_t s_addr;
} musl_in_addr;

typedef struct musl_in6_addr {
	union {
		uint8_t __s6_addr[16];
		uint16_t __s6_addr16[8];
		uint32_t __s6_addr32[4];
	} __in6_union;
} musl_in6_addr;

typedef struct musl_sockaddr {
	musl_sa_family_t sa_family;
	char sa_data[14];
} musl_sockaddr;

typedef struct musl_sockaddr_in {
	musl_sa_family_t sin_family;
	musl_in_port_t sin_port;
	musl_in_addr sin_addr;
	uint8_t sin_zero[8];
} musl_sockaddr_in;

typedef struct musl_sockaddr_in6 {
	musl_sa_family_t sin6_family;
	musl_in_port_t sin6_port;
	uint32_t sin6_flowinfo;
	musl_in6_addr sin6_addr;
	uint32_t sin6_scope_id;
} musl_sockaddr_in6;

typedef struct musl_addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	musl_socklen_t ai_addrlen;
	musl_sockaddr *ai_addr;
	char *ai_canonname;
	struct musl_addrinfo *ai_next;
} musl_addrinfo;

struct musl_pollfd {
	int fd;
	short events;
	short revents;
};

struct timespec;

const char *musl_gai_strerror(int);
int musl_socket(int, int, int);
int musl_connect(int, const musl_sockaddr *, musl_socklen_t);
int musl_clock_gettime(int, struct timespec *);
int musl_poll(struct musl_pollfd *, musl_nfds_t, int);
ssize_t musl_recv(int, void *, size_t, int);
ssize_t musl_send(int, const void *, size_t, int);
int musl_fcntl(int, int, long);
int musl_getsockopt(int, int, int, void *, musl_socklen_t *);
int musl_getaddrinfo(const char *, const char *, const musl_addrinfo *,
    musl_addrinfo **);
void musl_freeaddrinfo(musl_addrinfo *);

/* BearSSL's sysrng.c calls getentropy() directly. */
int getentropy(void *, size_t);
