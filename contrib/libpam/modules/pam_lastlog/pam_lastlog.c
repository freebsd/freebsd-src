/* pam_lastlog module */

/*
 * $Id: pam_lastlog.c,v 1.3 2001/02/10 22:33:10 agmorgan Exp $
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/3/11
 *
 * This module does the necessary work to display the last login
 * time+date for this user, it then updates this entry for the
 * present (login) service.
 */

#include <security/_pam_aconf.h>

#include <fcntl.h>
#include <time.h>
#ifdef HAVE_UTMP_H
# include <utmp.h>
#else
# include <lastlog.h>
#endif
#include <pwd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#ifdef WANT_PWDB
#include <pwdb/pwdb_public.h>                /* use POSIX front end */
#endif

#if defined(hpux) || defined(sunos) || defined(solaris)
# ifndef _PATH_LASTLOG
#  define _PATH_LASTLOG "/usr/adm/lastlog"
# endif /* _PATH_LASTLOG */
# ifndef UT_HOSTSIZE
#  define UT_HOSTSIZE 16
# endif /* UT_HOSTSIZE */
# ifndef UT_LINESIZE
#  define UT_LINESIZE 12
# endif /* UT_LINESIZE */
#endif
#if defined(hpux)
struct lastlog {
    time_t  ll_time;
    char    ll_line[UT_LINESIZE];
    char    ll_host[UT_HOSTSIZE];            /* same as in utmp */
};
#endif /* hpux */

/* XXX - time before ignoring lock. Is 1 sec enough? */
#define LASTLOG_IGNORE_LOCK_TIME     1

#define DEFAULT_HOST     ""  /* "[no.where]" */
#define DEFAULT_TERM     ""  /* "tt???" */
#define LASTLOG_NEVER_WELCOME       "Welcome to your new account!"
#define LASTLOG_INTRO    "Last login:"
#define LASTLOG_TIME     " %s"
#define _LASTLOG_HOST_FORMAT   " from %%.%ds"
#define _LASTLOG_LINE_FORMAT   " on %%.%ds"
#define LASTLOG_TAIL     ""
#define LASTLOG_MAXSIZE  (sizeof(LASTLOG_INTRO)+0 \
			  +sizeof(LASTLOG_TIME)+strlen(the_time) \
			  +sizeof(_LASTLOG_HOST_FORMAT)+UT_HOSTSIZE \
			  +sizeof(_LASTLOG_LINE_FORMAT)+UT_LINESIZE \
			  +sizeof(LASTLOG_TAIL))

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_SESSION

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

/* some syslogging */

static void _log_err(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-lastlog", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

/* argument parsing */

#define LASTLOG_DATE        01  /* display the date of the last login */
#define LASTLOG_HOST        02  /* display the last host used (if set) */
#define LASTLOG_LINE        04  /* display the last terminal used */
#define LASTLOG_NEVER      010  /* display a welcome message for first login */
#define LASTLOG_DEBUG      020  /* send info to syslog(3) */
#define LASTLOG_QUIET      040  /* keep quiet about things */

static int _pam_parse(int flags, int argc, const char **argv)
{
    int ctrl=(LASTLOG_DATE|LASTLOG_HOST|LASTLOG_LINE);

    /* does the appliction require quiet? */
    if (flags & PAM_SILENT) {
	ctrl |= LASTLOG_QUIET;
    }

    /* step through arguments */
    for (; argc-- > 0; ++argv) {

	/* generic options */

	if (!strcmp(*argv,"debug")) {
	    ctrl |= LASTLOG_DEBUG;
	} else if (!strcmp(*argv,"nodate")) {
	    ctrl |= ~LASTLOG_DATE;
	} else if (!strcmp(*argv,"noterm")) {
	    ctrl |= ~LASTLOG_LINE;
	} else if (!strcmp(*argv,"nohost")) {
	    ctrl |= ~LASTLOG_HOST;
	} else if (!strcmp(*argv,"silent")) {
	    ctrl |= LASTLOG_QUIET;
	} else if (!strcmp(*argv,"never")) {
	    ctrl |= LASTLOG_NEVER;
	} else {
	    _log_err(LOG_ERR,"unknown option; %s",*argv);
	}
    }

    D(("ctrl = %o", ctrl));
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

	if (retval != PAM_SUCCESS && (ctrl & LASTLOG_DEBUG) ) {
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

static int make_remark(pam_handle_t *pamh, int ctrl, const char *remark)
{
    int retval;

    if (!(ctrl & LASTLOG_QUIET)) {
	struct pam_message msg[1], *mesg[1];
	struct pam_response *resp=NULL;

	mesg[0] = &msg[0];
	msg[0].msg_style = PAM_TEXT_INFO;
	msg[0].msg = remark;

	retval = converse(pamh, ctrl, 1, mesg, &resp);

	msg[0].msg = NULL;
	if (resp) {
	    _pam_drop_reply(resp, 1);
	}
    } else {
	D(("keeping quiet"));
	retval = PAM_SUCCESS;
    }

    D(("returning %s", pam_strerror(pamh, retval)));
    return retval;
}

/*
 * Values for the announce flags..
 */

static int last_login_date(pam_handle_t *pamh, int announce, uid_t uid)
{
    struct flock last_lock;
    struct lastlog last_login;
    int retval = PAM_SESSION_ERR;
    int last_fd;

    /* obtain the last login date and all the relevant info */
    last_fd = open(_PATH_LASTLOG, O_RDWR);
    if (last_fd < 0) {
	D(("unable to open the %s file", _PATH_LASTLOG));
	if (announce & LASTLOG_DEBUG) {
	    _log_err(LOG_DEBUG, "unable to open %s file", _PATH_LASTLOG);
	}
	retval = PAM_PERM_DENIED;
    } else {
	int win;

	/* read the lastlogin file - for this uid */
	(void) lseek(last_fd, sizeof(last_login) * (off_t) uid, SEEK_SET);

	memset(&last_lock, 0, sizeof(last_lock));
	last_lock.l_type = F_RDLCK;
	last_lock.l_whence = SEEK_SET;
	last_lock.l_start = sizeof(last_login) * (off_t) uid;
	last_lock.l_len = sizeof(last_login);

	if ( fcntl(last_fd, F_SETLK, &last_lock) < 0 ) {
	    D(("locking %s failed..(waiting a little)", _PATH_LASTLOG));
	    _log_err(LOG_ALERT, "%s file is locked/read", _PATH_LASTLOG);
	    sleep(LASTLOG_IGNORE_LOCK_TIME);
	}

	win = ( read(last_fd, &last_login, sizeof(last_login))
		== sizeof(last_login) );

	last_lock.l_type = F_UNLCK;
	(void) fcntl(last_fd, F_SETLK, &last_lock);        /* unlock */

	if (!win) {
	    D(("First login for user uid=%d", _PATH_LASTLOG, uid));
	    if (announce & LASTLOG_DEBUG) {
		_log_err(LOG_DEBUG, "creating lastlog for uid %d", uid);
	    }
	    memset(&last_login, 0, sizeof(last_login));
	}

	/* rewind */
	(void) lseek(last_fd, sizeof(last_login) * (off_t) uid, SEEK_SET);

	if (!(announce & LASTLOG_QUIET)) {
	    if (last_login.ll_time) {
		char *the_time;
		char *remark;

		the_time = ctime(&last_login.ll_time);
		the_time[-1+strlen(the_time)] = '\0';    /* delete '\n' */

		remark = malloc(LASTLOG_MAXSIZE);
		if (remark == NULL) {
		    D(("no memory for last login remark"));
		    retval = PAM_BUF_ERR;
		} else {
		    int at;

		    /* printing prefix */
		    at = sprintf(remark, "%s", LASTLOG_INTRO);

		    /* we want the date? */
		    if (announce & LASTLOG_DATE) {
			at += sprintf(remark+at, LASTLOG_TIME, the_time);
		    }

		    /* we want & have the host? */
		    if ((announce & LASTLOG_HOST)
			&& (last_login.ll_host[0] != '\0')) {
			char format[2*sizeof(_LASTLOG_HOST_FORMAT)];

			(void) sprintf(format, _LASTLOG_HOST_FORMAT
				       , UT_HOSTSIZE);
			D(("format: %s", format));
			at += sprintf(remark+at, format, last_login.ll_host);
			_pam_overwrite(format);
		    }

		    /* we want and have the terminal? */
		    if ((announce & LASTLOG_LINE)
			&& (last_login.ll_line[0] != '\0')) {
			char format[2*sizeof(_LASTLOG_LINE_FORMAT)];

			(void) sprintf(format, _LASTLOG_LINE_FORMAT
				       , UT_LINESIZE);
			D(("format: %s", format));
			at += sprintf(remark+at, format, last_login.ll_line);
			_pam_overwrite(format);
		    }

		    /* display requested combo */
		    sprintf(remark+at, "%s", LASTLOG_TAIL);

		    retval = make_remark(pamh, announce, remark);

		    /* free all the stuff malloced */
		    _pam_overwrite(remark);
		    _pam_drop(remark);
		}
	    } else if ((!last_login.ll_time) && (announce & LASTLOG_NEVER)) {
		D(("this is the first time this user has logged in"));
		retval = make_remark(pamh, announce, LASTLOG_NEVER_WELCOME);
	    }
	} else {
	    D(("no text was requested"));
	    retval = PAM_SUCCESS;
	}

	/* write latest value */
	{
	    const char *remote_host=NULL
		, *terminal_line=DEFAULT_TERM;

	    /* set this login date */
	    D(("set the most recent login time"));

	    (void) time(&last_login.ll_time);    /* set the time */

	    /* set the remote host */
	    (void) pam_get_item(pamh, PAM_RHOST, (const void **)&remote_host);
	    if (remote_host == NULL) {
		remote_host = DEFAULT_HOST;
	    }

	    /* copy to last_login */
	    strncpy(last_login.ll_host, remote_host
		    , sizeof(last_login.ll_host));
	    remote_host = NULL;

	    /* set the terminal line */
	    (void) pam_get_item(pamh, PAM_TTY, (const void **)&terminal_line);
	    D(("terminal = %s", terminal_line));
	    if (terminal_line == NULL) {
		terminal_line = DEFAULT_TERM;
	    } else if ( !strncmp("/dev/", terminal_line, 5) ) {
		/* strip leading "/dev/" from tty.. */
		terminal_line += 5;
	    }
	    D(("terminal = %s", terminal_line));

	    /* copy to last_login */
	    strncpy(last_login.ll_line, terminal_line
		    , sizeof(last_login.ll_line));
	    terminal_line = NULL;

	    D(("locking last_log file"));

	    /* now we try to lock this file-record exclusively; non-blocking */
	    memset(&last_lock, 0, sizeof(last_lock));
	    last_lock.l_type = F_WRLCK;
	    last_lock.l_whence = SEEK_SET;
	    last_lock.l_start = sizeof(last_login) * (off_t) uid;
	    last_lock.l_len = sizeof(last_login);

	    if ( fcntl(last_fd, F_SETLK, &last_lock) < 0 ) {
		D(("locking %s failed..(waiting a little)", _PATH_LASTLOG));
		_log_err(LOG_ALERT, "%s file is locked/write", _PATH_LASTLOG);
		sleep(LASTLOG_IGNORE_LOCK_TIME);
	    }

	    D(("writing to the last_log file"));
	    (void) write(last_fd, &last_login, sizeof(last_login));

	    last_lock.l_type = F_UNLCK;
	    (void) fcntl(last_fd, F_SETLK, &last_lock);        /* unlock */
	    D(("unlocked"));

	    close(last_fd);                                  /* all done */
	}
	D(("all done with last login"));
    }

    /* reset the last login structure */
    memset(&last_login, 0, sizeof(last_login));

    return retval;
}

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc
			, const char **argv)
{
    int retval, ctrl;
    const char *user;
    const struct passwd *pwd;
    uid_t uid;

    /*
     * this module gets the uid of the PAM_USER. Uses it to display
     * last login info and then updates the lastlog for that user.
     */

    ctrl = _pam_parse(flags, argc, argv);

    /* which user? */

    retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
    if (retval != PAM_SUCCESS || user == NULL || *user == '\0') {
	_log_err(LOG_NOTICE, "user unknown");
	return PAM_USER_UNKNOWN;
    }

    /* what uid? */

    pwd = getpwnam(user);
    if (pwd == NULL) {
	D(("couldn't identify user %s", user));
	return PAM_CRED_INSUFFICIENT;
    }
    uid = pwd->pw_uid;
    pwd = NULL;                                         /* tidy up */

    /* process the current login attempt (indicate last) */

    retval = last_login_date(pamh, ctrl, uid);

    /* indicate success or failure */

    uid = -1;                                           /* forget this */

    return retval;
}

PAM_EXTERN
int pam_sm_close_session(pam_handle_t *pamh,int flags,int argc
			 ,const char **argv)
{
    return PAM_SUCCESS;
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_lastlog_modstruct = {
     "pam_lastlog",
     NULL,
     NULL,
     NULL,
     pam_sm_open_session,
     pam_sm_close_session,
     NULL,
};

#endif

/* end of module definition */
