
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_errno.h>
#include <compat/linux/linux_errno.inc>

int
bsd_to_linux_errno(int error)
{

	KASSERT(error >= 0 && error <= ELAST,
	    ("%s: bad error %d", __func__, error));

	return (linux_errtbl[error]);
}

#ifdef INVARIANTS
void
linux_check_errtbl(void)
{
	int i;

	for (i = 1; i < nitems(linux_errtbl); i++) {
		KASSERT(linux_errtbl[i] != 0,
		    ("%s: linux_errtbl[%d] == 0", __func__, i));
	}

	for (i = 1; i < nitems(linux_to_bsd_errtbl); i++) {
		KASSERT(linux_to_bsd_errtbl[i] != 0,
		    ("%s: linux_to_bsd_errtbl[%d] == 0", __func__, i));
	}

}
#endif
