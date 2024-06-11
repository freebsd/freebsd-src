/*
 * Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>

#if _FFR_DMTRIGGER && _FFR_NOTIFY < 2
#include <sm/conf.h>	/* FDSET_CAST */
#include <sm/fdset.h>
#include <sm/assert.h>
#include <sm/notify.h>
#include "notify.h"
#include <sm/time.h>
#include <sm/string.h>

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>	/* for memset() */

static int	Notifypipe[2];
#define NotifyRDpipe Notifypipe[0]
#define NotifyWRpipe Notifypipe[1]

#define CLOSEFD(fd) do { \
		if ((fd) != -1) {	\
			(void) close(fd);	\
			fd = - 1;	\
		}	\
	} while (0)	\


/*
**  SM_NOTIFY_INIT -- initialize notify system
**
**	Parameters:
**		flags -- ignored
**
**	Returns:
**		0: success
**		<0: -errno
*/

int
sm_notify_init(flags)
	int flags;
{
	if (pipe(Notifypipe) < 0)
		return -errno;
	return 0;
}

/*
**  SM_NOTIFY_START -- start notify system
**
**	Parameters:
**		owner -- owner.
**		flags -- currently ignored.
**
**	Returns:
**		0: success
**		<0: -errno
*/

int
sm_notify_start(owner, flags)
	bool owner;
	int flags;
{
	int r;

	r = 0;
	if (owner)
		CLOSEFD(NotifyWRpipe);
	else
		CLOSEFD(NotifyRDpipe);
	return r;
}

/*
**  SM_NOTIFY_STOP -- stop notify system
**
**	Parameters:
**		owner -- owner.
**		flags -- currently ignored.
**
**	Returns:
**		0: success
**		<0: -errno
*/

int
sm_notify_stop(owner, flags)
	bool owner;
	int flags;
{
	if (owner)
		CLOSEFD(NotifyRDpipe);
	else
		CLOSEFD(NotifyWRpipe);
	return 0;
}

/*
**  SM_NOTIFY_SND -- send notification
**
**	Parameters:
**		buf -- where to write data
**		buflen -- len of buffer
**
**	Returns:
**		0: success
**		<0: -errno
*/

int
sm_notify_snd(buf, buflen)
	char *buf;
	size_t buflen;
{
	int r;
	int save_errno;
	size_t len;
	char netstr[MAX_NETSTR];

	SM_REQUIRE(buf != NULL);
	SM_REQUIRE(buflen > 0);
	if (NotifyWRpipe < 0)
		return -EINVAL;
	if (buflen >= MAX_NETSTR - 7)
		return -E2BIG;	/* XXX "TOO LARGE"? */

	len = sm_snprintf(netstr, sizeof(netstr), "%04d:%s,", (int)buflen, buf);
	r = write(NotifyWRpipe, netstr, len);
	save_errno = errno;
	SM_DBG((stderr, "pid=%ld, write=%d, fd=%d, e=%d\n", (long)getpid(), r, NotifyWRpipe, save_errno));
	return r >= 0 ? 0 : -save_errno;
}

/*
**  SM_NOTIFY_RCV -- receive notification
**
**	Parameters:
**		buf -- where to write data
**		buflen -- len of buffer
**		tmo -- timeout (micro seconds)
**
**	Returns:
**		0: EOF (XXX need to provide info about client)
**		>0: length of received data
**		<0: -errno
*/

int
sm_notify_rcv(buf, buflen, tmo)
	char *buf;
	size_t buflen;
	long tmo;
{
	int r, len;
	int save_errno;
	fd_set readfds;
	struct timeval timeout, *tval;

	SM_REQUIRE(buf != NULL);
	SM_REQUIRE(buflen > NETSTRPRE + 2);
	if (NotifyRDpipe < 0)
		return -EINVAL;
	FD_ZERO(&readfds);
	SM_FD_SET(NotifyRDpipe, &readfds);
	SM_MICROS2TVAL(tmo, tval, timeout);

	do {
		r = select(NotifyRDpipe + 1, FDSET_CAST &readfds, NULL, NULL, tval);
		save_errno = errno;
		SM_DBG((stderr, "pid=%ld, select=%d, fd=%d, e=%d\n", (long)getpid(), r, NotifyRDpipe, save_errno));
	} while (r < 0 && save_errno == EINTR);

	RDNETSTR(r, NotifyRDpipe, (void)0);
}
#endif /* _FFR_DMTRIGGER && _FFR_NOTIFY < 2 */
