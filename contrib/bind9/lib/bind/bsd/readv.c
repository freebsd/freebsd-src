#ifndef LINT
static const char rcsid[] = "$Id: readv.c,v 1.1 2001/03/29 06:30:35 marka Exp $";
#endif

#include "port_before.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "port_after.h"

#ifndef NEED_READV
int __bindcompat_readv;
#else

int
__readv(fd, vp, vpcount)
	int fd;
	const struct iovec *vp;
	int vpcount;
{
	int count = 0;

	while (vpcount-- > 0) {
		int bytes = read(fd, vp->iov_base, vp->iov_len);

		if (bytes < 0)
			return (-1);
		count += bytes;
		if (bytes != vp->iov_len)
			break;
		vp++;
	}
	return (count);
}
#endif /* NEED_READV */
