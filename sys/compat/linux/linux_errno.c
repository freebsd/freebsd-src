/* $FreeBSD$ */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_errno.inc>

int
bsd_to_linux_errno(int error)
{

	KASSERT(error >= 0 && error <= ELAST,
	    ("%s: bad error %d", __func__, error));

	return (linux_errtbl[error]);
}
