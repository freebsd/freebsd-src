/*
 * pam_krb5_sess.c
 *
 * PAM session management functions for pam_krb5
 * (null functions)
 *
 * $FreeBSD$
 */

static const char rcsid[] = "$Id: pam_krb5_sess.c,v 1.3 1999/01/19 20:49:44 fcusack Exp $";

#include <security/pam_appl.h>
#include <security/pam_modules.h>

/* Initiate session management */
int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    return PAM_SUCCESS;
}


/* Terminate session management */
int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    return PAM_SUCCESS;
}
