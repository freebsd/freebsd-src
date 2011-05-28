#ifndef LINT
static const char rcsid[] = "$Id: writev.c,v 1.2.164.1 2005-04-27 05:00:47 sra Exp $";
#endif

#include "port_before.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "port_after.h"

#ifndef NEED_WRITEV
int __bindcompat_writev;
#else

#ifdef _CRAY
#define OWN_WRITEV
int
__writev(int fd, struct iovec *iov, int iovlen)
{
	struct stat statbuf;

	if (fstat(fd, &statbuf) < 0)
		return (-1);

	/*
	 * Allow for atomic writes to network.
	 */
	if (statbuf.st_mode & S_IFSOCK) {
		struct msghdr   mesg;		

		memset(&mesg, 0, sizeof(mesg));
		mesg.msg_name = 0;
		mesg.msg_namelen = 0;
		mesg.msg_iov = iov;
		mesg.msg_iovlen = iovlen;
		mesg.msg_accrights = 0;
		mesg.msg_accrightslen = 0;
		return (sendmsg(fd, &mesg, 0));
	} else {
		struct iovec *tv;
		int i, rcode = 0, count = 0;

		for (i = 0, tv = iov; i <= iovlen; tv++) {
			rcode = write(fd, tv->iov_base, tv->iov_len);

			if (rcode < 0)
				break;

			count += rcode;
		}

		if (count == 0)
			return (rcode);
		else
			return (count);
	}
}

#else /*_CRAY*/

int
__writev(fd, vp, vpcount)
	int fd;
	const struct iovec *vp;
	int vpcount;
{
	int count = 0;

	while (vpcount-- > 0) {
		int written = write(fd, vp->iov_base, vp->iov_len);

		if (written < 0)
			return (-1);
		count += written;
		if (written != vp->iov_len)
			break;
		vp++;
	}
	return (count);
}

#endif /*_CRAY*/

#endif /*NEED_WRITEV*/

/*! \file */
