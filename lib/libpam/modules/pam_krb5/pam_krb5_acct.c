/*
 * pam_krb5_acct.c
 *
 * PAM account management functions for pam_krb5
 *
 * $FreeBSD$
 */

static const char rcsid[] = "$Id: pam_krb5_acct.c,v 1.3 1999/01/19 21:26:44 fcusack Exp $";

#include <syslog.h>	/* syslog */
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <krb5.h>
#include <com_err.h>
#include "pam_krb5.h"

/* A useful logging macro */
#define DLOG(error_func, error_msg) \
if (debug) \
    syslog(LOG_DEBUG, "pam_krb5: pam_sm_acct_mgmt(%s %s): %s: %s", \
	   service, name, error_func, error_msg)

/* Check authorization of user */
int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    krb5_error_code	krbret;
    krb5_context	pam_context;
    krb5_ccache		ccache;
    krb5_principal	princ;

    char		*service, *name;
    int			debug = 0;
    int			i, pamret;

    for (i = 0; i < argc; i++) {
	if (strcmp(argv[i], "debug") == 0)
	    debug = 1;
    }

    /* Get username */
    if (pam_get_item(pamh, PAM_USER, (const void **) &name)) {
	return PAM_PERM_DENIED;;
    }

    /* Get service name */
    (void) pam_get_item(pamh, PAM_SERVICE, (const void **) &service);
    if (!service)
	service = "unknown";

    DLOG("entry", "");

    if (pam_get_data(pamh, "ccache", (const void **) &ccache)) {
	/* User did not use krb5 to login */
	DLOG("ccache", "not found");
	return PAM_SUCCESS;
    }

    if ((krbret = krb5_init_context(&pam_context)) != 0) {
	DLOG("krb5_init_context()", error_message(krbret));
	return PAM_PERM_DENIED;;
    }

    if ((krbret = krb5_cc_get_principal(pam_context, ccache, &princ)) != 0) {
	DLOG("krb5_cc_get_principal()", error_message(krbret));
	pamret = PAM_PERM_DENIED;;
	goto cleanup;
    }

    if (krb5_kuserok(pam_context, princ, name))
	pamret = PAM_SUCCESS;
    else
	pamret = PAM_PERM_DENIED;
    krb5_free_principal(pam_context, princ);

cleanup:
    krb5_free_context(pam_context);
    DLOG("exit", pamret ? "failure" : "success");
    return pamret;

}

