/* pam_mail module */

/*
 * $Id: pam_mail.c,v 1.2 1997/02/15 16:06:14 morgan Exp morgan $
 *
 * Written by Andrew Morgan <morgan@parc.power.net> 1996/3/11
 * $HOME additions by David Kinchlea <kinch@kinch.ark.com> 1997/1/7
 *
 * $Log: pam_mail.c,v $
 * Revision 1.2  1997/02/15 16:06:14  morgan
 * session -> setcred, also added "~"=$HOME
 *
 * Revision 1.1  1997/01/04 20:33:02  morgan
 * Initial revision
 */

#define DEFAULT_MAIL_DIRECTORY    "/var/spool/mail"
#define MAIL_FILE_FORMAT          "%s/%s"
#define MAIL_ENV_NAME             "MAIL"
#define MAIL_ENV_FORMAT           MAIL_ENV_NAME "=%s"
#define YOUR_MAIL_FORMAT          "You have %s mail in %s"

#ifdef linux
# define _GNU_SOURCE
# include <features.h>
#endif

#include <ctype.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef WANT_PWDB
#include <pwdb/pwdb_public.h>
#endif

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

static void _log_err(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-mail", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

/* argument parsing */

#define PAM_DEBUG_ARG       01
#define PAM_NO_LOGIN        02
#define PAM_LOGOUT_TOO      04
#define PAM_NEW_MAIL_DIR   010
#define PAM_MAIL_SILENT    020
#define PAM_NO_ENV         040
#define PAM_HOME_MAIL     0100
#define PAM_EMPTY_TOO     0200

static int _pam_parse(int flags, int argc, const char **argv, char **maildir)
{
    int ctrl=0;

    if (flags & PAM_SILENT) {
	ctrl |= PAM_MAIL_SILENT;
    }

    /* step through arguments */
    for (; argc-- > 0; ++argv) {

	/* generic options */

	if (!strcmp(*argv,"debug"))
	    ctrl |= PAM_DEBUG_ARG;
	else if (!strncmp(*argv,"dir=",4)) {
	    *maildir = x_strdup(4+*argv);
	    if (*maildir != NULL) {
		D(("new mail directory: %s", *maildir));
		ctrl |= PAM_NEW_MAIL_DIR;
	    } else {
		_log_err(LOG_CRIT,
			 "failed to duplicate mail directory - ignored");
	    }
	} else if (!strcmp(*argv,"close")) {
	    ctrl |= PAM_LOGOUT_TOO;
	} else if (!strcmp(*argv,"nopen")) {
	    ctrl |= PAM_NO_LOGIN;
	} else if (!strcmp(*argv,"noenv")) {
	    ctrl |= PAM_NO_ENV;
	} else if (!strcmp(*argv,"empty")) {
	    ctrl |= PAM_EMPTY_TOO;
	} else {
	    _log_err(LOG_ERR,"pam_parse: unknown option; %s",*argv);
	}
    }

    return ctrl;
}

/* a front end for conversations */

static int converse(pam_handle_t *pamh, int ctrl, int nargs
		    , struct pam_message **message
		    , struct pam_response **response)
{
    int retval;
    struct pam_conv *conv;

    D(("begin to converse"));

    retval = pam_get_item( pamh, PAM_CONV, (const void **) &conv ) ; 
    if ( retval == PAM_SUCCESS ) {

	retval = conv->conv(nargs, ( const struct pam_message ** ) message
			    , response, conv->appdata_ptr);

	D(("returned from application's conversation function"));

	if (retval != PAM_SUCCESS && (PAM_DEBUG_ARG & ctrl) ) {
	    _log_err(LOG_DEBUG, "conversation failure [%s]"
		     , pam_strerror(pamh, retval));
	}

    } else {
	_log_err(LOG_ERR, "couldn't obtain coversation function [%s]"
		 , pam_strerror(pamh, retval));
    }

    D(("ready to return from module conversation"));

    return retval;                  /* propagate error status */
}

static int get_folder(pam_handle_t *pamh, int ctrl
		      , char **path_mail, char **folder_p)
{
    int retval;
    const char *user, *path;
    char *folder;
    const struct passwd *pwd=NULL;

    retval = pam_get_user(pamh, &user, NULL);
    if (retval != PAM_SUCCESS || user == NULL) {
	_log_err(LOG_ERR, "no user specified");
	return PAM_USER_UNKNOWN;
    }

    if (ctrl & PAM_NEW_MAIL_DIR) {
	path = *path_mail;
	if (*path == '~') {       /* support for $HOME delivery */
	    pwd = getpwnam(user);
	    if (pwd == NULL) {
		_log_err(LOG_ERR, "user [%s] unknown", user);
		_pam_overwrite(*path_mail);
		_pam_drop(*path_mail);
		return PAM_USER_UNKNOWN;
	    }
	    /*
	     * "~/xxx" and "~xxx" are treated as same
	     */
	    if (!*++path || (*path == '/' && !*++path)) {
		_log_err(LOG_ALERT, "badly formed mail path [%s]", *path_mail);
		_pam_overwrite(*path_mail);
		_pam_drop(*path_mail);
		return PAM_ABORT;
	    }
	    ctrl |= PAM_HOME_MAIL;
	}
    } else {
	path = DEFAULT_MAIL_DIRECTORY;
    }

    /* put folder together */

    if (ctrl & PAM_HOME_MAIL) {
	folder = malloc(sizeof(MAIL_FILE_FORMAT)
			+strlen(pwd->pw_dir)+strlen(path));
    } else {
	folder = malloc(sizeof(MAIL_FILE_FORMAT)+strlen(path)+strlen(user));
    }

    if (folder != NULL) {
	if (ctrl & PAM_HOME_MAIL) {
	    sprintf(folder, MAIL_FILE_FORMAT, pwd->pw_dir, path);
	} else {
	    sprintf(folder, MAIL_FILE_FORMAT, path, user);
	}
	D(("folder =[%s]", folder));
    }

    /* tidy up */

    _pam_overwrite(*path_mail);
    _pam_drop(*path_mail);
    user = NULL;

    if (folder == NULL) {
	_log_err(LOG_CRIT, "out of memory for mail folder");
	return PAM_BUF_ERR;
    }

    *folder_p = folder;
    folder = NULL;

    return PAM_SUCCESS;
}

static const char *get_mail_status(int ctrl, const char *folder)
{
    const char *type;
    struct stat mail_st;

    if (stat(folder, &mail_st) == 0 && mail_st.st_size > 0) {
	type = (mail_st.st_atime < mail_st.st_mtime) ? "new":"old" ;
    } else if (ctrl & PAM_EMPTY_TOO) {
	type = "no";
    } else {
	type = NULL;
    }

    memset(&mail_st, 0, sizeof(mail_st));
    D(("user has %s mail in %s folder", type, folder));
    return type;
}

static int report_mail(pam_handle_t *pamh, int ctrl
		       , const char *type, const char *folder)
{
    int retval;

    if (!(ctrl & PAM_MAIL_SILENT)) {
	char *remark;

	remark = malloc(sizeof(YOUR_MAIL_FORMAT)+strlen(type)+strlen(folder));
	if (remark == NULL) {
	    retval = PAM_BUF_ERR;
	} else {
	    struct pam_message msg[1], *mesg[1];
	    struct pam_response *resp=NULL;

	    sprintf(remark, YOUR_MAIL_FORMAT, type, folder);

	    mesg[0] = &msg[0];
	    msg[0].msg_style = PAM_TEXT_INFO;
	    msg[0].msg = remark;

	    retval = converse(pamh, ctrl, 1, mesg, &resp);

	    _pam_overwrite(remark);
	    _pam_drop(remark);
	    if (resp)
		_pam_drop_reply(resp, 1);
	}
    } else {
	D(("keeping quiet"));
	retval = PAM_SUCCESS;
    }

    D(("returning %s", pam_strerror(pamh, retval)));
    return retval;
}

/* --- authentication management functions (only) --- */

/*
 * Cannot use mail to authenticate yourself
 */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh,int flags,int argc
			 ,const char **argv)
{
    return PAM_IGNORE;
}

/*
 * MAIL is a "credential"
 */

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc
		   , const char **argv)
{
    int retval, ctrl;
    char *path_mail=NULL, *folder;
    const char *type;

    /*
     * this module (un)sets the MAIL environment variable, and checks if
     * the user has any new mail.
     */

    ctrl = _pam_parse(flags, argc, argv, &path_mail);

    /* Do we have anything to do? */

    if (!(flags & (PAM_ESTABLISH_CRED|PAM_DELETE_CRED))) {
	return PAM_SUCCESS;
    }

    /* which folder? */

    retval = get_folder(pamh, ctrl, &path_mail, &folder);
    if (retval != PAM_SUCCESS) {
	D(("failed to find folder"));
	return retval;
    }

    /* set the MAIL variable? */

    if (!(ctrl & PAM_NO_ENV) && (flags & PAM_ESTABLISH_CRED)) {
	char *tmp;

	tmp = malloc(strlen(folder)+sizeof(MAIL_ENV_FORMAT));
	if (tmp != NULL) {
	    sprintf(tmp, MAIL_ENV_FORMAT, folder);
	    D(("setting env: %s", tmp));
	    retval = pam_putenv(pamh, tmp);
	    _pam_overwrite(tmp);
	    _pam_drop(tmp);
	    if (retval != PAM_SUCCESS) {
		_pam_overwrite(folder);
		_pam_drop(folder);
		_log_err(LOG_CRIT, "unable to set " MAIL_ENV_NAME " variable");
		return retval;
	    }
	} else {
	    _log_err(LOG_CRIT, "no memory for " MAIL_ENV_NAME " variable");
	    _pam_overwrite(folder);
	    _pam_drop(folder);
	    return retval;
	}
    } else {
	D(("not setting " MAIL_ENV_NAME " variable"));
    }

    /*
     * OK. we've got the mail folder... what about its status?
     */

    if (((flags & PAM_ESTABLISH_CRED) && !(ctrl & PAM_NO_LOGIN))
	|| ((flags & PAM_DELETE_CRED) && (ctrl & PAM_LOGOUT_TOO))) {
	type = get_mail_status(ctrl, folder);
	if (type != NULL) {
	    retval = report_mail(pamh, ctrl, type, folder);
	    type = NULL;
	}
    }

    /*
     * Delete environment variable?
     */

    if (flags & PAM_DELETE_CRED) {
	(void) pam_putenv(pamh, MAIL_ENV_NAME);
    }

    _pam_overwrite(folder);                             /* clean up */
    _pam_drop(folder);

    /* indicate success or failure */

    return retval;
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_mail_modstruct = {
     "pam_mail",
     pam_sm_authenticate,
     pam_sm_setcred,
     NULL,
     NULL,
     NULL,
     NULL,
};

#endif

/* end of module definition */
