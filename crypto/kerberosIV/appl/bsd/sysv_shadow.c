/* Author: Wietse Venema <wietse@wzv.win.tue.nl> */

#include "bsd_locl.h"

RCSID("$Id: sysv_shadow.c,v 1.8 1997/12/29 19:56:07 bg Exp $");

#ifdef SYSV_SHADOW

#include <sysv_shadow.h>

/* sysv_expire - check account and password expiration times */

int
sysv_expire(struct spwd *spwd)
{
    long    today;

    tzset();
    today = time(0)/(60*60*24);	/* In days since Jan. 1, 1970 */

    if (spwd->sp_expire > 0) {
	if (today > spwd->sp_expire) {
	    printf("Your account has expired.\n");
	    sleepexit(1);
	} else if (spwd->sp_expire - today < 14) {
	    printf("Your account will expire in %d days.\n",
		   (int)(spwd->sp_expire - today));
	    return (0);
	}
    }
    if (spwd->sp_max > 0) {
	if (today > (spwd->sp_lstchg + spwd->sp_max)) {
	    printf("Your password has expired. Choose a new one.\n");
	    return (1);
	} else if (spwd->sp_warn > 0
	    && (today > (spwd->sp_lstchg + spwd->sp_max - spwd->sp_warn))) {
	    printf("Your password will expire in %d days.\n",
		   (int)(spwd->sp_lstchg + spwd->sp_max - today));
	    return (0);
	}
    }
    return (0);
}

#endif /* SYSV_SHADOW */
