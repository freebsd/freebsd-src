#include <sys/socket.h>

#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>

#include "musl_compat.h"

const char *
musl_gai_strerror(int ecode)
{
	return (gai_strerror(ecode));
}

int
musl_socket(int domain, int type, int protocol)
{
	return (socket(domain, type, protocol));
}

int
musl_connect(int fd, const musl_sockaddr *addr, musl_socklen_t len)
{
	return (connect(fd, (const struct sockaddr *)addr, len));
}

int
musl_clock_gettime(int clock_id, struct timespec *ts)
{
	return (clock_gettime(clock_id, ts));
}

int
musl_poll(struct musl_pollfd *fds, musl_nfds_t nfds, int timeout)
{
	return (poll((struct pollfd *)fds, nfds, timeout));
}

ssize_t
musl_recv(int fd, void *buf, size_t len, int flags)
{
	return (recv(fd, buf, len, flags));
}

ssize_t
musl_send(int fd, const void *buf, size_t len, int flags)
{
	return (send(fd, buf, len, flags));
}

int
musl_fcntl(int fd, int cmd, long arg)
{
	return (fcntl(fd, cmd, arg));
}

int
musl_getsockopt(int fd, int level, int optname, void *optval,
    musl_socklen_t *optlen)
{
	return (getsockopt(fd, level, optname, optval, optlen));
}
