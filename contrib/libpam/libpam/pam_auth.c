/*
 * pam_auth.c -- PAM authentication
 *
 * $Id: pam_auth.c,v 1.3 2001/01/22 06:07:28 agmorgan Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "pam_private.h"

int pam_authenticate(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("pam_authenticate called"));

    IF_NO_PAMH("pam_authenticate", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    if (pamh->former.choice == PAM_NOT_STACKED) {
	_pam_sanitize(pamh);
	_pam_start_timer(pamh);    /* we try to make the time for a failure
				      independent of the time it takes to
				      fail */
    }

    retval = _pam_dispatch(pamh, flags, PAM_AUTHENTICATE);

    if (retval != PAM_INCOMPLETE) {
	_pam_sanitize(pamh);
	_pam_await_timer(pamh, retval);   /* if unsuccessful then wait now */
	D(("pam_authenticate exit"));
    } else {
	D(("will resume when ready"));
    }

    return retval;
}

int pam_setcred(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("pam_setcred called"));

    IF_NO_PAMH("pam_setcred", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    if (! flags) {
	flags = PAM_ESTABLISH_CRED;
    }

    retval = _pam_dispatch(pamh, flags, PAM_SETCRED);

    D(("pam_setcred exit"));

    return retval;
}
