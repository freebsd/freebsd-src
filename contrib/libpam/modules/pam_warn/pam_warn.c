/* pam_warn module */

/*
 * $Id: pam_warn.c,v 1.1.1.1 2000/06/20 22:12:10 agmorgan Exp $
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/3/11
 */

#define _BSD_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH
#define PAM_SM_PASSWORD

#include <security/pam_modules.h>

/* some syslogging */

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-warn", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc
			, const char **argv)
{
     const char *service=NULL, *user=NULL, *terminal=NULL
	 , *rhost=NULL, *ruser=NULL;

     (void) pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
     (void) pam_get_item(pamh, PAM_TTY, (const void **)&terminal);
     _pam_log(LOG_NOTICE, "service: %s [on terminal: %s]"
	      , service ? service : "<unknown>"
	      , terminal ? terminal : "<unknown>"
	 );
     (void) pam_get_user(pamh, &user, "Who are you? ");
     (void) pam_get_item(pamh, PAM_RUSER, (const void **)&ruser);
     (void) pam_get_item(pamh, PAM_RHOST, (const void **)&rhost);
     _pam_log(LOG_NOTICE, "user: (uid=%d) -> %s [remote: %s@%s]"
	      , getuid()
	      , user ? user : "<unknown>"
	      , ruser ? ruser : "?nobody"
	      , rhost ? rhost : "?nowhere"
	      );

     /* we are just a fly on the wall */

     return PAM_IGNORE;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc
		   , const char **argv)
{
    return PAM_IGNORE;
}

/* password updating functions */

PAM_EXTERN
int pam_sm_chauthtok(pam_handle_t *pamh,int flags,int argc
		   , const char **argv)
{
    /* map to the authentication function... */

    return pam_sm_authenticate(pamh, flags, argc, argv);
}

PAM_EXTERN int
pam_sm_acct_mgmt (pam_handle_t *pamh, int flags,
                  int argc, const char **argv)
{
    /* map to the authentication function... */

    return pam_sm_authenticate(pamh, flags, argc, argv);
}

PAM_EXTERN int
pam_sm_open_session (pam_handle_t *pamh, int flags, int argc,
                     const char **argv)
{
    /* map to the authentication function... */

    return pam_sm_authenticate(pamh, flags, argc, argv);
}

PAM_EXTERN int
pam_sm_close_session (pam_handle_t *pamh, int flags, int argc,
		      const char **argv)
{
    /* map to the authentication function... */

    return pam_sm_authenticate(pamh, flags, argc, argv);
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_warn_modstruct = {
    "pam_warn",
    pam_sm_authenticate,
    pam_sm_setcred,
    pam_sm_acct_mgmt,
    pam_sm_open_session,
    pam_sm_close_session,
    pam_sm_chauthtok,
};

#endif

/* end of module definition */
