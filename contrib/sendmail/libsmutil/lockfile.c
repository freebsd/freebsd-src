/*
 * Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: lockfile.c,v 8.3 1999/08/31 15:38:27 ca Exp $";
#endif /* ! lint */

#include <sendmail.h>

/*
**  LOCKFILE -- lock a file using flock or (shudder) fcntl locking
**
**	Parameters:
**		fd -- the file descriptor of the file.
**		filename -- the file name (for error messages). [unused]
**		ext -- the filename extension. [unused]
**		type -- type of the lock.  Bits can be:
**			LOCK_EX -- exclusive lock.
**			LOCK_NB -- non-blocking.
**
**	Returns:
**		TRUE if the lock was acquired.
**		FALSE otherwise.
*/

bool
lockfile(fd, filename, ext, type)
	int fd;
	char *filename;
	char *ext;
	int type;
{
#if !HASFLOCK
	int action;
	struct flock lfd;
	extern int errno;

	memset(&lfd, '\0', sizeof lfd);
	if (bitset(LOCK_UN, type))
		lfd.l_type = F_UNLCK;
	else if (bitset(LOCK_EX, type))
		lfd.l_type = F_WRLCK;
	else
		lfd.l_type = F_RDLCK;
	if (bitset(LOCK_NB, type))
		action = F_SETLK;
	else
		action = F_SETLKW;

	if (fcntl(fd, action, &lfd) >= 0)
		return TRUE;

	/*
	**  On SunOS, if you are testing using -oQ/tmp/mqueue or
	**  -oA/tmp/aliases or anything like that, and /tmp is mounted
	**  as type "tmp" (that is, served from swap space), the
	**  previous fcntl will fail with "Invalid argument" errors.
	**  Since this is fairly common during testing, we will assume
	**  that this indicates that the lock is successfully grabbed.
	*/

	if (errno == EINVAL)
		return TRUE;

#else /* !HASFLOCK */

	if (flock(fd, type) >= 0)
		return TRUE;

#endif /* !HASFLOCK */

	return FALSE;
}
