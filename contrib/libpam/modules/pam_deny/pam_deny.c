/* pam_deny module */

/*
 * $Id: pam_deny.c,v 1.4 1997/02/15 19:05:15 morgan Exp $
 * $FreeBSD$
 *
 * Written by Andrew Morgan <morgan@parc.power.net> 1996/3/11
 *
 * $Log: pam_deny.c,v $
 * Revision 1.4  1997/02/15 19:05:15  morgan
 * fixed email
 *
 * Revision 1.3  1996/06/02 08:06:19  morgan
 * changes for new static protocol
 *
 * Revision 1.2  1996/05/26 04:01:12  morgan
 * added static support
 *
 * Revision 1.1  1996/03/16 17:47:36  morgan
 * Initial revision
 *
 */

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

/* --- authentication management functions --- */

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
     return PAM_AUTH_ERR;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc
		   ,const char **argv)
{
     return PAM_CRED_UNAVAIL;
}

/* --- account management functions --- */

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh,int flags,int argc
		     ,const char **argv)
{
     return PAM_ACCT_EXPIRED;
}

/* --- password management --- */

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh,int flags,int argc
		     ,const char **argv)
{
     return PAM_AUTHTOK_ERR;
}

/* --- session management --- */

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
    return PAM_SYSTEM_ERR;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh,int flags,int argc
			 ,const char **argv)
{
     return PAM_SYSTEM_ERR;
}

/* end of module definition */

PAM_MODULE_ENTRY("pam_deny");
