/*
 * Public domain
 *
 * BSD socket emulation code for Winsock2
 * Brent Cook <bcook@openbsd.org>
 */

#ifndef LIBCRYPTOCOMPAT_WIN32NETCOMPAT_H
#define LIBCRYPTOCOMPAT_WIN32NETCOMPAT_H

#ifdef _WIN32

#include <ws2tcpip.h>

#define SHUT_RDWR SD_BOTH
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND

#include <errno.h>
#include <unistd.h>

int posix_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int posix_close(int fd);
ssize_t posix_read(int fd, void *buf, size_t count);

ssize_t posix_write(int fd, const void *buf, size_t count);

int posix_getsockopt(int sockfd, int level, int optname,
	void *optval, socklen_t *optlen);

int posix_setsockopt(int sockfd, int level, int optname,
	const void *optval, socklen_t optlen);

#ifndef NO_REDEF_POSIX_FUNCTIONS
#define connect(sockfd, addr, addrlen) posix_connect(sockfd, addr, addrlen)
#define close(fd) posix_close(fd)
#define read(fd, buf, count) posix_read(fd, buf, count)
#define write(fd, buf, count) posix_write(fd, buf, count)
#define getsockopt(sockfd, level, optname, optval, optlen) \
	posix_getsockopt(sockfd, level, optname, optval, optlen)
#define setsockopt(sockfd, level, optname, optval, optlen) \
	posix_setsockopt(sockfd, level, optname, optval, optlen)
#endif

#endif

#endif
