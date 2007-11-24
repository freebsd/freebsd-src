/*
	dup2 -- 7th Edition UNIX system call emulation for UNIX System V

	last edit:	11-Feb-1987	D A Gwyn
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include	<errno.h>
#include	<fcntl.h>

extern int	close(), fcntl();

int
dup2( oldfd, newfd )
	int		oldfd;		/* already-open file descriptor */
	int		newfd;		/* desired duplicate descriptor */
{
	register int	ret;		/* for fcntl() return value */
	register int	save;		/* for saving entry errno */

	if ( oldfd == newfd )
		return oldfd;		/* be careful not to close() */

	save = errno;			/* save entry errno */
	(void) close( newfd );		/* in case newfd is open */
	/* (may have just clobbered the original errno value) */

	ret = fcntl( oldfd, F_DUPFD, newfd );	/* dupe it */

	if ( ret >= 0 )
		errno = save;		/* restore entry errno */
	else				/* fcntl() returned error */
		if ( errno == EINVAL )
			errno = EBADF;	/* we think of everything */

	return ret;			/* return file descriptor */
}
