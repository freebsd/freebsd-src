/* pam_strerror.c */

/*
 * $Id: pam_strerror.c,v 1.2 2000/12/04 19:02:34 baggins Exp $
 */

#include "pam_private.h"

const char *pam_strerror(pam_handle_t *pamh, int errnum)
{
#ifdef UGLY_HACK_FOR_PRIOR_BEHAVIOR_SUPPORT  /* will be removed from v 1.0 */

    int possible_error;

    possible_error = (int) pamh;
    if (!(possible_error >= 0 && possible_error <= PAM_BAD_ITEM)) {
	possible_error = errnum;
    }

/* mask standard behavior to use possible_error variable. */
#define errnum possible_error

#endif /* UGLY_HACK_FOR_PRIOR_BEHAVIOR_SUPPORT */

    switch (errnum) {
    case PAM_SUCCESS:
	return "Success";
    case PAM_ABORT:
	return "Critical error - immediate abort";
    case PAM_OPEN_ERR:
	return "dlopen() failure";
    case PAM_SYMBOL_ERR:
	return "Symbol not found";
    case PAM_SERVICE_ERR:
	return "Error in service module";
    case PAM_SYSTEM_ERR:
	return "System error";
    case PAM_BUF_ERR:
	return "Memory buffer error";
    case PAM_PERM_DENIED:
	return "Permission denied";
    case PAM_AUTH_ERR:
	return "Authentication failure";
    case PAM_CRED_INSUFFICIENT:
	return "Insufficient credentials to access authentication data";
    case PAM_AUTHINFO_UNAVAIL:
	return "Authentication service cannot retrieve authentication info.";
    case PAM_USER_UNKNOWN:
	return "User not known to the underlying authentication module";
    case PAM_MAXTRIES:
	return "Have exhasted maximum number of retries for service.";
    case PAM_NEW_AUTHTOK_REQD:
	return "Authentication token is no longer valid; new one required.";
    case PAM_ACCT_EXPIRED:
	return "User account has expired";
    case PAM_SESSION_ERR:
	return "Cannot make/remove an entry for the specified session";
    case PAM_CRED_UNAVAIL:
	return "Authentication service cannot retrieve user credentials";
    case PAM_CRED_EXPIRED:
	return "User credentials expired";
    case PAM_CRED_ERR:
	return "Failure setting user credentials";
    case PAM_NO_MODULE_DATA:
	return "No module specific data is present";
    case PAM_BAD_ITEM:
	return "Bad item passed to pam_*_item()";
    case PAM_CONV_ERR:
	return "Conversation error";
    case PAM_AUTHTOK_ERR:
	return "Authentication token manipulation error";
    case PAM_AUTHTOK_RECOVER_ERR:
	return "Authentication information cannot be recovered";
    case PAM_AUTHTOK_LOCK_BUSY:
	return "Authentication token lock busy";
    case PAM_AUTHTOK_DISABLE_AGING:
	return "Authentication token aging disabled";
    case PAM_TRY_AGAIN:
	return "Failed preliminary check by password service";
    case PAM_IGNORE:
	return "Please ignore underlying account module";
    case PAM_MODULE_UNKNOWN:
	return "Module is unknown";
    case PAM_AUTHTOK_EXPIRED:
	return "Authentication token expired";
    case PAM_CONV_AGAIN:
	return "Conversation is waiting for event";
    case PAM_INCOMPLETE:
	return "Application needs to call libpam again";
    }

    return "Unknown Linux-PAM error (need to upgrde libpam?)";
}
