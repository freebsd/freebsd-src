/* pam_account.c - PAM Account Management */

#include <stdio.h>

#include "pam_private.h"

int pam_acct_mgmt(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("called"));

    IF_NO_PAMH("pam_acct_mgmt", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    retval = _pam_dispatch(pamh, flags, PAM_ACCOUNT);

    return retval;
}
