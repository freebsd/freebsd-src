/* pam_password.c - PAM Password Management */

/*
 * $Id: pam_password.c,v 1.7 1997/04/05 06:56:45 morgan Exp $
 * $FreeBSD$
 *
 * $Log: pam_password.c,v $
 */

#include <stdio.h>
#include <stdlib.h>

#include "pam_private.h"

int pam_chauthtok(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("called."));

    IF_NO_PAMH("pam_chauthtok", pamh, PAM_SYSTEM_ERR);

    if (pamh->former.choice == PAM_NOT_STACKED) {
	_pam_start_timer(pamh);    /* we try to make the time for a failure
				      independent of the time it takes to
				      fail */
	_pam_sanitize(pamh);
	pamh->former.update = PAM_FALSE;
    }

    /* first loop through to check if there will be a problem */
    if (pamh->former.update ||
	(retval = _pam_dispatch(pamh, flags|PAM_PRELIM_CHECK,
				PAM_CHAUTHTOK)) == PAM_SUCCESS) {
	pamh->former.update = PAM_TRUE;
	retval = _pam_dispatch(pamh, flags|PAM_UPDATE_AUTHTOK,
			       PAM_CHAUTHTOK);
    }

    /* if we completed we should clean up */
    if (retval != PAM_INCOMPLETE) {
	_pam_sanitize(pamh);
	pamh->former.update = PAM_FALSE;
	_pam_await_timer(pamh, retval);   /* if unsuccessful then wait now */
	D(("pam_authenticate exit"));
    } else {
	D(("will resume when ready"));
    }

    return retval;
}

