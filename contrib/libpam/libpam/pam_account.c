/* pam_account.c - PAM Account Management */
/* $FreeBSD$ */

#include <stdio.h>

#include "pam_private.h"

int pam_acct_mgmt(pam_handle_t *pamh, int flags)
{
    D(("called"));

    IF_NO_PAMH("pam_acct_mgmt",pamh,PAM_SYSTEM_ERR);
    return _pam_dispatch(pamh, flags, PAM_ACCOUNT);
}
