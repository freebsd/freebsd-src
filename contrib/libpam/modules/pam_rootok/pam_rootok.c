/* pam_rootok module */

/*
 * $Id: pam_rootok.c,v 1.1.1.1 2000/06/20 22:11:56 agmorgan Exp $
 * $FreeBSD$
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/3/11
 */

#define _GNU_SOURCE

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

#include <security/pam_modules.h>

/* some syslogging */

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-rootok", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}


/* argument parsing */

#define PAM_DEBUG_ARG       01

static int _pam_parse(int argc, const char **argv)
{
    int ctrl=0;

    /* step through arguments */
    for (ctrl=0; argc-- > 0; ++argv) {

	/* generic options */

	if (!strcmp(*argv,"debug"))
	    ctrl |= PAM_DEBUG_ARG;
	else {
	    _pam_log(LOG_ERR,"pam_parse: unknown option; %s",*argv);
	}
    }

    return ctrl;
}

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
    int ctrl;
    int retval = PAM_AUTH_ERR;

    ctrl = _pam_parse(argc, argv);
    if (getuid() == 0)
	retval = PAM_SUCCESS;

    if (ctrl & PAM_DEBUG_ARG) {
	_pam_log(LOG_DEBUG, "authetication %s"
		 , retval==PAM_SUCCESS ? "succeeded":"failed" );
    }

    return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc
		   ,const char **argv)
{
    return PAM_SUCCESS;
}


/* end of module definition */

PAM_MODULE_ENTRY("pam_rootok");
