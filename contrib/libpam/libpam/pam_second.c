/*
 * pam_second.c -- PAM secondary authentication
 * (based on XSSO draft spec of March 1997)
 *
 * $Id: pam_second.c,v 1.2 2000/12/04 19:02:34 baggins Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "pam_private.h"

/* p 42 */

int pam_authenticate_secondary(pam_handle_t *pamh,
			       char *target_username,
			       char *target_module_type,
			       char *target_authn_domain,
			       char *target_supp_data,
			       unsigned char *target_module_authtok,
			       int flags)
{
    int retval=PAM_SYSTEM_ERR;

    D(("called"));

    _pam_start_timer(pamh);    /* we try to make the time for a failure
				  independent of the time it takes to
				  fail */

    IF_NO_PAMH("pam_authenticate_secondary",pamh,PAM_SYSTEM_ERR);

    _pam_await_timer(pamh, retval);   /* if unsuccessful then wait now */

    D(("pam_authenticate_secondary exit"));

    return retval;
}
