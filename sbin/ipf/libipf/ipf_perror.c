#include <fcntl.h>
#include <sys/ioctl.h>
#include "ipf.h"

void
ipf_perror(int err, char *string)
{
	if (err == 0)
		fprintf(stderr, "%s\n", string);
	else
		fprintf(stderr, "%s: %s\n", string, ipf_strerror(err));
}

int
ipf_perror_fd( int fd, ioctlfunc_t iocfunc, char *string)
{
	int save;
	int realerr;

	save = errno;
	if ((*iocfunc)(fd, SIOCIPFINTERROR, &realerr) == -1)
		realerr = 0;

	errno = save;
	fprintf(stderr, "%d:", realerr);
	ipf_perror(realerr, string);
	return (realerr ? realerr : save);

}

void
ipferror(int fd, char *msg)
{
	if (fd >= 0) {
		ipf_perror_fd(fd, ioctl, msg);
	} else {
		fprintf(stderr, "0:");
		perror(msg);
	}
}
