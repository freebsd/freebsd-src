/* pam_permit module */

/*
 * $Id: pam_permit.c,v 1.5 1997/02/15 19:03:15 morgan Exp $
 * $FreeBSD$
 *
 * Written by Andrew Morgan <morgan@parc.power.net> 1996/3/11
 *
 * $Log: pam_permit.c,v $
 * Revision 1.5  1997/02/15 19:03:15  morgan
 * fixed email address
 *
 * Revision 1.4  1997/02/15 16:03:10  morgan
 * force a name for user
 *
 * Revision 1.3  1996/06/02 08:10:14  morgan
 * updated for new static protocol
 *
 */

#define DEFAULT_USER "nobody"

#include <stdio.h>

/*
 * here, we make definitions for the externally accessible functions
 * in this file (these definitions are required for static modules
 * but strongly encouraged generally) they are used to instruct the
 * modules include file to define their prototypes.
 */

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

/* --- authentication management functions --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
    int retval;
    const char *user=NULL;

    /*
     * authentication requires we know who the user wants to be
     */
    retval = pam_get_user(pamh, &user, NULL);
    if (retval != PAM_SUCCESS) {
	D(("get user returned error: %s", pam_strerror(pamh,retval)));
	return retval;
    }
    if (user == NULL || *user == '\0') {
	D(("username not known"));
	pam_set_item(pamh, PAM_USER, (const void *) DEFAULT_USER);
    }
    user = NULL;                                            /* clean up */

    return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc
		   ,const char **argv)
{
     return PAM_SUCCESS;
}

/* --- account management functions --- */

PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh,int flags,int argc
		     ,const char **argv)
{
     return PAM_SUCCESS;
}

/* --- password management --- */

PAM_EXTERN
int pam_sm_chauthtok(pam_handle_t *pamh,int flags,int argc
		     ,const char **argv)
{
     return PAM_SUCCESS;
}

/* --- session management --- */

PAM_EXTERN
int pam_sm_open_session(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
    return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_close_session(pam_handle_t *pamh,int flags,int argc
			 ,const char **argv)
{
     return PAM_SUCCESS;
}

/* end of module definition */

PAM_MODULE_ENTRY("pam_permit");
