/* pam_ftp module */

/*
 * $Id: pam_ftp.c,v 1.2 2000/11/19 23:54:03 agmorgan Exp $
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/3/11
 *
 */

#define PLEASE_ENTER_PASSWORD "Password required for %s."
#define GUEST_LOGIN_PROMPT "Guest login ok, " \
"send your complete e-mail address as password."

/* the following is a password that "can't be correct" */
#define BLOCK_PASSWORD "\177BAD PASSWPRD\177"

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

/* some syslogging */

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-ftp", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

static int converse(pam_handle_t *pamh, int nargs
		    , struct pam_message **message
		    , struct pam_response **response)
{
    int retval;
    struct pam_conv *conv;

    D(("begin to converse\n"));

    retval = pam_get_item( pamh, PAM_CONV, (const void **) &conv ) ; 
    if ( retval == PAM_SUCCESS ) {

	retval = conv->conv(nargs, ( const struct pam_message ** ) message
			    , response, conv->appdata_ptr);

	D(("returned from application's conversation function\n"));

	if ((retval != PAM_SUCCESS) && (retval != PAM_CONV_AGAIN)) {
	    _pam_log(LOG_DEBUG, "conversation failure [%s]"
		     , pam_strerror(pamh, retval));
	}

    } else {
	_pam_log(LOG_ERR, "couldn't obtain coversation function [%s]"
		 , pam_strerror(pamh, retval));
    }

    D(("ready to return from module conversation\n"));

    return retval;                  /* propagate error status */
}

/* argument parsing */

#define PAM_DEBUG_ARG       01
#define PAM_IGNORE_EMAIL    02
#define PAM_NO_ANON         04

static int _pam_parse(int argc, const char **argv, char **users)
{
    int ctrl=0;

    /* step through arguments */
    for (ctrl=0; argc-- > 0; ++argv) {

	/* generic options */

	if (!strcmp(*argv,"debug"))
	    ctrl |= PAM_DEBUG_ARG;
	else if (!strncmp(*argv,"users=",6)) {
	    *users = x_strdup(6+*argv);
	    if (*users == NULL) {
		ctrl |= PAM_NO_ANON;
		_pam_log(LOG_CRIT, "failed to duplicate user list - anon off");
	    }
	} else if (!strcmp(*argv,"ignore")) {
	    ctrl |= PAM_IGNORE_EMAIL;
	} else {
	    _pam_log(LOG_ERR,"pam_parse: unknown option; %s",*argv);
	}
    }

    return ctrl;
}

/*
 * check if name is in list or default list. place users name in *_user
 * return 1 if listed 0 if not.
 */

static int lookup(const char *name, char *list, const char **_user)
{
    int anon = 0;

    *_user = name;                 /* this is the default */
    if (list) {
	const char *l;
	char *x;

	x = list;
	while ((l = strtok(x, ","))) {
	    x = NULL;
	    if (!strcmp(name, l)) {
		*_user = list;
		anon = 1;
	    }
	}
    } else {
#define MAX_L 2
	static const char *l[MAX_L] = { "ftp", "anonymous" };
	int i;

	for (i=0; i<MAX_L; ++i) {
	    if (!strcmp(l[i], name)) {
		*_user = l[0];
		anon = 1;
		break;
	    }
	}
    }

    return anon;
}

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
    int retval, anon=0, ctrl;
    const char *user;
    char *users=NULL;

    /*
     * this module checks if the user name is ftp or annonymous. If
     * this is the case, it can set the PAM_RUSER to the entered email
     * address and SUCCEEDS, otherwise it FAILS.
     */

    ctrl = _pam_parse(argc, argv, &users);

    retval = pam_get_user(pamh, &user, NULL);
    if (retval != PAM_SUCCESS || user == NULL) {
	_pam_log(LOG_ERR, "no user specified");
	return PAM_USER_UNKNOWN;
    }

    if (!(ctrl & PAM_NO_ANON)) {
	anon = lookup(user, users, &user);
    }

    if (anon) {
	retval = pam_set_item(pamh, PAM_USER, (const void *)user);
	if (retval != PAM_SUCCESS || user == NULL) {
	    _pam_log(LOG_ERR, "user resetting failed");
	    return PAM_USER_UNKNOWN;
	}
    }

    /*
     * OK. we require an email address for user or the user's password.
     * - build conversation and get their input.
     */

    {
	struct pam_message msg[1], *mesg[1];
	struct pam_response *resp=NULL;
	const char *token;
	char *prompt=NULL;
	int i=0;

	if (!anon) {
	    prompt = malloc(strlen(PLEASE_ENTER_PASSWORD) + strlen(user));
	    if (prompt == NULL) {
		D(("out of memory!?"));
		return PAM_BUF_ERR;
	    } else {
		sprintf(prompt, PLEASE_ENTER_PASSWORD, user);
		msg[i].msg = prompt;
	    }
	} else {
	    msg[i].msg = GUEST_LOGIN_PROMPT;
	}

	msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
	mesg[i] = &msg[i];

	retval = converse(pamh, ++i, mesg, &resp);
	if (prompt) {
	    _pam_overwrite(prompt);
	    _pam_drop(prompt);
	}

	if (retval != PAM_SUCCESS) {
	    if (resp != NULL)
		_pam_drop_reply(resp,i);
	    return ((retval == PAM_CONV_AGAIN)
		    ? PAM_INCOMPLETE:PAM_AUTHINFO_UNAVAIL);
	}

	if (anon) {
	    /* XXX: Some effort should be made to verify this email address! */

	    if (!(ctrl & PAM_IGNORE_EMAIL)) {
		token = strtok(resp->resp, "@");
		retval = pam_set_item(pamh, PAM_RUSER, token);

		if ((token) && (retval == PAM_SUCCESS)) {
		    token = strtok(NULL, "@");
		    retval = pam_set_item(pamh, PAM_RHOST, token);
		}
	    }

	    /* we are happy to grant annonymous access to the user */
	    retval = PAM_SUCCESS;

	} else {
	    /*
	     * we have a password so set AUTHTOK
	     */

	    (void) pam_set_item(pamh, PAM_AUTHTOK, resp->resp);

	    /*
	     * this module failed, but the next one might succeed with
	     * this password.
	     */

	    retval = PAM_AUTH_ERR;
	}

	if (resp) {                                      /* clean up */
	    _pam_drop_reply(resp, i);
	}

	/* success or failure */

	return retval;
    }
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc
		   ,const char **argv)
{
     return PAM_IGNORE;
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_ftp_modstruct = {
     "pam_ftp",
     pam_sm_authenticate,
     pam_sm_setcred,
     NULL,
     NULL,
     NULL,
     NULL,
};

#endif

/* end of module definition */
