/* 
 * $Id: support.c,v 1.8 2001/02/11 06:33:53 agmorgan Exp $
 *
 * Copyright information at end of file.
 */

#define _BSD_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <pwd.h>
#include <shadow.h>
#include <limits.h>
#include <utmp.h>

#include <security/_pam_macros.h>
#include <security/pam_modules.h>

#include "md5.h"
#include "support.h"

extern char *crypt(const char *key, const char *salt);
extern char *bigcrypt(const char *key, const char *salt);

/* syslogging function for errors and other information */

void _log_err(int err, pam_handle_t *pamh, const char *format,...)
{
	char *service = NULL;
	char logname[256];
	va_list args;

	pam_get_item(pamh, PAM_SERVICE, (const void **) &service);
	if (service) {
		strncpy(logname, service, sizeof(logname));
		logname[sizeof(logname) - 1 - strlen("(pam_unix)")] = '\0';
		strncat(logname, "(pam_unix)", strlen("(pam_unix)"));
	} else {
		strncpy(logname, "pam_unix", sizeof(logname) - 1);
	}

	va_start(args, format);
	openlog(logname, LOG_CONS | LOG_PID, LOG_AUTH);
	vsyslog(err, format, args);
	va_end(args);
	closelog();
}

/* this is a front-end for module-application conversations */

static int converse(pam_handle_t * pamh, int ctrl, int nargs
		    ,struct pam_message **message
		    ,struct pam_response **response)
{
	int retval;
	struct pam_conv *conv;

	D(("begin to converse"));

	retval = pam_get_item(pamh, PAM_CONV, (const void **) &conv);
	if (retval == PAM_SUCCESS) {

		retval = conv->conv(nargs, (const struct pam_message **) message
				    ,response, conv->appdata_ptr);

		D(("returned from application's conversation function"));

		if (retval != PAM_SUCCESS && on(UNIX_DEBUG, ctrl)) {
			_log_err(LOG_DEBUG, pamh, "conversation failure [%s]"
				 ,pam_strerror(pamh, retval));
		}
	} else if (retval != PAM_CONV_AGAIN) {
		_log_err(LOG_ERR, pamh
		         ,"couldn't obtain coversation function [%s]"
			 ,pam_strerror(pamh, retval));
	}
	D(("ready to return from module conversation"));

	return retval;		/* propagate error status */
}

int _make_remark(pam_handle_t * pamh, unsigned int ctrl
		       ,int type, const char *text)
{
	int retval = PAM_SUCCESS;

	if (off(UNIX__QUIET, ctrl)) {
		struct pam_message *pmsg[1], msg[1];
		struct pam_response *resp;

		pmsg[0] = &msg[0];
		msg[0].msg = text;
		msg[0].msg_style = type;

		resp = NULL;
		retval = converse(pamh, ctrl, 1, pmsg, &resp);

		if (resp) {
			_pam_drop_reply(resp, 1);
		}
	}
	return retval;
}

  /*
   * Beacause getlogin() is braindead and sometimes it just
   * doesn't work, we reimplement it here.
   */
char *PAM_getlogin(void)
{
	struct utmp *ut, line;
	char *curr_tty, *retval;
	static char curr_user[sizeof(ut->ut_user) + 4];

	retval = NULL;

	curr_tty = ttyname(0);
	if (curr_tty != NULL) {
		D(("PAM_getlogin ttyname: %s", curr_tty));
		curr_tty += 5;
		setutent();
		strncpy(line.ut_line, curr_tty, sizeof line.ut_line);
		if ((ut = getutline(&line)) != NULL) {
			strncpy(curr_user, ut->ut_user, sizeof(ut->ut_user));
			retval = curr_user;
		}
		endutent();
	}
	D(("PAM_getlogin retval: %s", retval));

	return retval;
}

/*
 * set the control flags for the UNIX module.
 */

int _set_ctrl(pam_handle_t *pamh, int flags, int *remember, int argc,
              const char **argv)
{
	unsigned int ctrl;

	D(("called."));

	ctrl = UNIX_DEFAULTS;	/* the default selection of options */

	/* set some flags manually */

	if (getuid() == 0 && !(flags & PAM_CHANGE_EXPIRED_AUTHTOK)) {
		D(("IAMROOT"));
		set(UNIX__IAMROOT, ctrl);
	}
	if (flags & PAM_UPDATE_AUTHTOK) {
		D(("UPDATE_AUTHTOK"));
		set(UNIX__UPDATE, ctrl);
	}
	if (flags & PAM_PRELIM_CHECK) {
		D(("PRELIM_CHECK"));
		set(UNIX__PRELIM, ctrl);
	}
	if (flags & PAM_DISALLOW_NULL_AUTHTOK) {
		D(("DISALLOW_NULL_AUTHTOK"));
		set(UNIX__NONULL, ctrl);
	}
	if (flags & PAM_SILENT) {
		D(("SILENT"));
		set(UNIX__QUIET, ctrl);
	}
	/* now parse the arguments to this module */

	while (argc-- > 0) {
		int j;

		D(("pam_unix arg: %s", *argv));

		for (j = 0; j < UNIX_CTRLS_; ++j) {
			if (unix_args[j].token
			    && !strncmp(*argv, unix_args[j].token, strlen(unix_args[j].token))) {
				break;
			}
		}

		if (j >= UNIX_CTRLS_) {
			_log_err(LOG_ERR, pamh,
			         "unrecognized option [%s]", *argv);
		} else {
			ctrl &= unix_args[j].mask;	/* for turning things off */
			ctrl |= unix_args[j].flag;	/* for turning things on  */

			if (remember != NULL) {
				if (j == UNIX_REMEMBER_PASSWD) {
					*remember = strtol(*argv + 9, NULL, 10);
					if ((*remember == LONG_MIN) || (*remember == LONG_MAX))
						*remember = -1;
					if (*remember > 400)
						*remember = 400;
				}
			}
		}

		++argv;		/* step to next argument */
	}

	/* auditing is a more sensitive version of debug */

	if (on(UNIX_AUDIT, ctrl)) {
		set(UNIX_DEBUG, ctrl);
	}
	/* return the set of flags */

	D(("done."));
	return ctrl;
}

static void _cleanup(pam_handle_t * pamh, void *x, int error_status)
{
	_pam_delete(x);
}

/* ************************************************************** *
 * Useful non-trivial functions                                   *
 * ************************************************************** */

  /*
   * the following is used to keep track of the number of times a user fails
   * to authenticate themself.
   */

#define FAIL_PREFIX                   "-UN*X-FAIL-"
#define UNIX_MAX_RETRIES              3

struct _pam_failed_auth {
	char *user;		/* user that's failed to be authenticated */
	char *name;		/* attempt from user with name */
	int uid;		/* uid of calling user */
	int euid;		/* euid of calling process */
	int count;		/* number of failures so far */
};

#ifndef PAM_DATA_REPLACE
#error "Need to get an updated libpam 0.52 or better"
#endif

static void _cleanup_failures(pam_handle_t * pamh, void *fl, int err)
{
	int quiet;
	const char *service = NULL;
	const char *ruser = NULL;
	const char *rhost = NULL;
	const char *tty = NULL;
	struct _pam_failed_auth *failure;

	D(("called"));

	quiet = err & PAM_DATA_SILENT;	/* should we log something? */
	err &= PAM_DATA_REPLACE;	/* are we just replacing data? */
	failure = (struct _pam_failed_auth *) fl;

	if (failure != NULL) {

		if (!quiet && !err) {	/* under advisement from Sun,may go away */

			/* log the number of authentication failures */
			if (failure->count > 1) {
				(void) pam_get_item(pamh, PAM_SERVICE,
						    (const void **)&service);
				(void) pam_get_item(pamh, PAM_RUSER,
						    (const void **)&ruser);
				(void) pam_get_item(pamh, PAM_RHOST,
						    (const void **)&rhost);
				(void) pam_get_item(pamh, PAM_TTY,
						    (const void **)&tty);
				_log_err(LOG_NOTICE, pamh,
				         "%d more authentication failure%s; "
				         "logname=%s uid=%d euid=%d "
				         "tty=%s ruser=%s rhost=%s "
				         "%s%s",
				         failure->count - 1, failure->count == 2 ? "" : "s",
				         failure->name, failure->uid, failure->euid,
				         tty ? tty : "", ruser ? ruser : "",
				         rhost ? rhost : "",
				         (failure->user && failure->user[0] != '\0')
				          ? " user=" : "", failure->user
				);

				if (failure->count > UNIX_MAX_RETRIES) {
					_log_err(LOG_ALERT, pamh
						 ,"service(%s) ignoring max retries; %d > %d"
						 ,service == NULL ? "**unknown**" : service
						 ,failure->count
						 ,UNIX_MAX_RETRIES);
				}
			}
		}
		_pam_delete(failure->user);	/* tidy up */
		_pam_delete(failure->name);	/* tidy up */
		free(failure);
	}
}

/*
 * _unix_blankpasswd() is a quick check for a blank password
 *
 * returns TRUE if user does not have a password
 * - to avoid prompting for one in such cases (CG)
 */

int _unix_blankpasswd(unsigned int ctrl, const char *name)
{
	struct passwd *pwd = NULL;
	struct spwd *spwdent = NULL;
	char *salt = NULL;
	int retval;

	D(("called"));

	/*
	 * This function does not have to be too smart if something goes
	 * wrong, return FALSE and let this case to be treated somewhere
	 * else (CG)
	 */

	if (on(UNIX__NONULL, ctrl))
		return 0;	/* will fail but don't let on yet */

	/* UNIX passwords area */
	pwd = getpwnam(name);	/* Get password file entry... */

	if (pwd != NULL) {
		if (strcmp( pwd->pw_passwd, "*NP*" ) == 0)
		{ /* NIS+ */                 
			uid_t save_euid, save_uid;
	
			save_euid = geteuid();
			save_uid = getuid();
			if (save_uid == pwd->pw_uid)
				setreuid( save_euid, save_uid );
			else  {
				setreuid( 0, -1 );
				if (setreuid( -1, pwd->pw_uid ) == -1) {
					setreuid( -1, 0 );
					setreuid( 0, -1 );
					if(setreuid( -1, pwd->pw_uid ) == -1)
						/* Will fail elsewhere. */
						return 0;
				}
			}
	
			spwdent = getspnam( name );
			if (save_uid == pwd->pw_uid)
				setreuid( save_uid, save_euid );
			else {
				if (setreuid( -1, 0 ) == -1)
				setreuid( save_uid, -1 );
				setreuid( -1, save_euid );
			}
		} else if (strcmp(pwd->pw_passwd, "x") == 0) {
			/*
			 * ...and shadow password file entry for this user,
			 * if shadowing is enabled
			 */
			spwdent = getspnam(name);
		}
		if (spwdent)
			salt = x_strdup(spwdent->sp_pwdp);
		else
			salt = x_strdup(pwd->pw_passwd);
	}
	/* Does this user have a password? */
	if (salt == NULL) {
		retval = 0;
	} else {
		if (strlen(salt) == 0)
			retval = 1;
		else
			retval = 0;
	}

	/* tidy up */

	if (salt)
		_pam_delete(salt);

	return retval;
}

/*
 * verify the password of a user
 */

#include <sys/types.h>
#include <sys/wait.h>

static int _unix_run_helper_binary(pam_handle_t *pamh, const char *passwd,
				   unsigned int ctrl, const char *user)
{
    int retval, child, fds[2];

    D(("called."));
    /* create a pipe for the password */
    if (pipe(fds) != 0) {
	D(("could not make pipe"));
	return PAM_AUTH_ERR;
    }

    /* fork */
    child = fork();
    if (child == 0) {
	static char *envp[] = { NULL };
	char *args[] = { NULL, NULL, NULL };

	/* XXX - should really tidy up PAM here too */

	/* reopen stdin as pipe */
	close(fds[1]);
	dup2(fds[0], STDIN_FILENO);

	/* exec binary helper */
	args[0] = x_strdup(CHKPWD_HELPER);
	args[1] = x_strdup(user);

	execve(CHKPWD_HELPER, args, envp);

	/* should not get here: exit with error */
	D(("helper binary is not available"));
	exit(PAM_AUTHINFO_UNAVAIL);
    } else if (child > 0) {
	/* wait for child */
	/* if the stored password is NULL */
	if (off(UNIX__NONULL, ctrl)) {	/* this means we've succeeded */
	    write(fds[1], "nullok\0\0", 8);
	} else {
	    write(fds[1], "nonull\0\0", 8);
	}
	if (passwd != NULL) {            /* send the password to the child */
	    write(fds[1], passwd, strlen(passwd)+1);
	    passwd = NULL;
	} else {
	    write(fds[1], "", 1);                        /* blank password */
	}
	close(fds[0]);       /* close here to avoid possible SIGPIPE above */
	close(fds[1]);
	(void) waitpid(child, &retval, 0);  /* wait for helper to complete */
	retval = (retval == 0) ? PAM_SUCCESS:PAM_AUTH_ERR;
    } else {
	D(("fork failed"));
	retval = PAM_AUTH_ERR;
    }

    D(("returning %d", retval));
    return retval;
}

int _unix_verify_password(pam_handle_t * pamh, const char *name
			  ,const char *p, unsigned int ctrl)
{
	struct passwd *pwd = NULL;
	struct spwd *spwdent = NULL;
	char *salt = NULL;
	char *pp = NULL;
	char *data_name;
	int retval;

	D(("called"));

#ifdef HAVE_PAM_FAIL_DELAY
	if (off(UNIX_NODELAY, ctrl)) {
		D(("setting delay"));
		(void) pam_fail_delay(pamh, 2000000);	/* 2 sec delay for on failure */
	}
#endif

	/* locate the entry for this user */

	D(("locating user's record"));

	/* UNIX passwords area */
	pwd = getpwnam(name);	/* Get password file entry... */

	if (pwd != NULL) {
		if (strcmp( pwd->pw_passwd, "*NP*" ) == 0)
		{ /* NIS+ */                 
			uid_t save_euid, save_uid;
	
			save_euid = geteuid();
			save_uid = getuid();
			if (save_uid == pwd->pw_uid)
				setreuid( save_euid, save_uid );
			else  {
				setreuid( 0, -1 );
				if (setreuid( -1, pwd->pw_uid ) == -1) {
					setreuid( -1, 0 );
					setreuid( 0, -1 );
					if(setreuid( -1, pwd->pw_uid ) == -1)
						return PAM_CRED_INSUFFICIENT;
				}
			}
	
			spwdent = getspnam( name );
			if (save_uid == pwd->pw_uid)
				setreuid( save_uid, save_euid );
			else {
				if (setreuid( -1, 0 ) == -1)
				setreuid( save_uid, -1 );
				setreuid( -1, save_euid );
			}
		} else if (strcmp(pwd->pw_passwd, "x") == 0) {
			/*
			 * ...and shadow password file entry for this user,
			 * if shadowing is enabled
			 */
			spwdent = getspnam(name);
		}
		if (spwdent)
			salt = x_strdup(spwdent->sp_pwdp);
		else
			salt = x_strdup(pwd->pw_passwd);
	}

	data_name = (char *) malloc(sizeof(FAIL_PREFIX) + strlen(name));
	if (data_name == NULL) {
		_log_err(LOG_CRIT, pamh, "no memory for data-name");
	} else {
		strcpy(data_name, FAIL_PREFIX);
		strcpy(data_name + sizeof(FAIL_PREFIX) - 1, name);
	}

	retval = PAM_SUCCESS;
	if (pwd == NULL || salt == NULL || !strcmp(salt, "x")) {
		if (geteuid()) {
			/* we are not root perhaps this is the reason? Run helper */
			D(("running helper binary"));
			retval = _unix_run_helper_binary(pamh, p, ctrl, name);
			if (pwd == NULL && !on(UNIX_AUDIT,ctrl)
			    && retval != PAM_SUCCESS)
			{
				name = NULL;
			}
		} else {
			D(("user's record unavailable"));
			if (on(UNIX_AUDIT, ctrl)) {
				/* this might be a typo and the user has given a password
				   instead of a username. Careful with this. */
				_log_err(LOG_ALERT, pamh,
				         "check pass; user (%s) unknown", name);
			} else {
				name = NULL;
				_log_err(LOG_ALERT, pamh,
				         "check pass; user unknown");
			}
			p = NULL;
			retval = PAM_AUTHINFO_UNAVAIL;
		}
	} else {
		if (!strlen(salt)) {
			/* the stored password is NULL */
			if (off(UNIX__NONULL, ctrl)) {	/* this means we've succeeded */
				D(("user has empty password - access granted"));
				retval = PAM_SUCCESS;
			} else {
				D(("user has empty password - access denied"));
				retval = PAM_AUTH_ERR;
			}
		} else if (!p) {
				retval = PAM_AUTH_ERR;
		} else {
			if (!strncmp(salt, "$1$", 3)) {
				pp = Goodcrypt_md5(p, salt);
				if (strcmp(pp, salt) != 0) {
					pp = Brokencrypt_md5(p, salt);
				}
			} else {
				pp = bigcrypt(p, salt);
			}
			p = NULL;		/* no longer needed here */

			/* the moment of truth -- do we agree with the password? */
			D(("comparing state of pp[%s] and salt[%s]", pp, salt));

			if (strcmp(pp, salt) == 0) {
				retval = PAM_SUCCESS;
			} else {
				retval = PAM_AUTH_ERR;
			}
		}
	}

	if (retval == PAM_SUCCESS) {
		if (data_name)	/* reset failures */
			pam_set_data(pamh, data_name, NULL, _cleanup_failures);
	} else {
		if (data_name != NULL) {
			struct _pam_failed_auth *new = NULL;
			const struct _pam_failed_auth *old = NULL;

			/* get a failure recorder */

			new = (struct _pam_failed_auth *)
			    malloc(sizeof(struct _pam_failed_auth));

			if (new != NULL) {

				new->user = x_strdup(name ? name : "");
				new->uid = getuid();
				new->euid = geteuid();
				new->name = x_strdup(PAM_getlogin()? PAM_getlogin() : "");

				/* any previous failures for this user ? */
				pam_get_data(pamh, data_name, (const void **) &old);

				if (old != NULL) {
					new->count = old->count + 1;
					if (new->count >= UNIX_MAX_RETRIES) {
						retval = PAM_MAXTRIES;
					}
				} else {
					const char *service=NULL;
					const char *ruser=NULL;
					const char *rhost=NULL;
					const char *tty=NULL;

					(void) pam_get_item(pamh, PAM_SERVICE,
							    (const void **)&service);
					(void) pam_get_item(pamh, PAM_RUSER,
							    (const void **)&ruser);
					(void) pam_get_item(pamh, PAM_RHOST,
							    (const void **)&rhost);
					(void) pam_get_item(pamh, PAM_TTY,
							    (const void **)&tty);

					_log_err(LOG_NOTICE, pamh,
					         "authentication failure; "
					         "logname=%s uid=%d euid=%d "
					         "tty=%s ruser=%s rhost=%s "
					         "%s%s",
					         new->name, new->uid, new->euid,
					         tty ? tty : "",
					         ruser ? ruser : "",
					         rhost ? rhost : "",
					         (new->user && new->user[0] != '\0')
					          ? " user=" : "",
					         new->user
					);
					new->count = 1;
				}

				pam_set_data(pamh, data_name, new, _cleanup_failures);

			} else {
				_log_err(LOG_CRIT, pamh,
				         "no memory for failure recorder");
			}
		}
	}

	if (data_name)
		_pam_delete(data_name);
	if (salt)
		_pam_delete(salt);
	if (pp)
		_pam_overwrite(pp);

	D(("done [%d].", retval));

	return retval;
}

/*
 * obtain a password from the user
 */

int _unix_read_password(pam_handle_t * pamh
			,unsigned int ctrl
			,const char *comment
			,const char *prompt1
			,const char *prompt2
			,const char *data_name
			,const char **pass)
{
	int authtok_flag;
	int retval;
	const char *item;
	char *token;

	D(("called"));

	/*
	 * make sure nothing inappropriate gets returned
	 */

	*pass = token = NULL;

	/*
	 * which authentication token are we getting?
	 */

	authtok_flag = on(UNIX__OLD_PASSWD, ctrl) ? PAM_OLDAUTHTOK : PAM_AUTHTOK;

	/*
	 * should we obtain the password from a PAM item ?
	 */

	if (on(UNIX_TRY_FIRST_PASS, ctrl) || on(UNIX_USE_FIRST_PASS, ctrl)) {
		retval = pam_get_item(pamh, authtok_flag, (const void **) &item);
		if (retval != PAM_SUCCESS) {
			/* very strange. */
			_log_err(LOG_ALERT, pamh
				 ,"pam_get_item returned error to unix-read-password"
			    );
			return retval;
		} else if (item != NULL) {	/* we have a password! */
			*pass = item;
			item = NULL;
			return PAM_SUCCESS;
		} else if (on(UNIX_USE_FIRST_PASS, ctrl)) {
			return PAM_AUTHTOK_RECOVER_ERR;		/* didn't work */
		} else if (on(UNIX_USE_AUTHTOK, ctrl)
			   && off(UNIX__OLD_PASSWD, ctrl)) {
			return PAM_AUTHTOK_RECOVER_ERR;
		}
	}
	/*
	 * getting here implies we will have to get the password from the
	 * user directly.
	 */

	{
		struct pam_message msg[3], *pmsg[3];
		struct pam_response *resp;
		int i, replies;

		/* prepare to converse */

		if (comment != NULL && off(UNIX__QUIET, ctrl)) {
			pmsg[0] = &msg[0];
			msg[0].msg_style = PAM_TEXT_INFO;
			msg[0].msg = comment;
			i = 1;
		} else {
			i = 0;
		}

		pmsg[i] = &msg[i];
		msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
		msg[i++].msg = prompt1;
		replies = 1;

		if (prompt2 != NULL) {
			pmsg[i] = &msg[i];
			msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
			msg[i++].msg = prompt2;
			++replies;
		}
		/* so call the conversation expecting i responses */
		resp = NULL;
		retval = converse(pamh, ctrl, i, pmsg, &resp);

		if (resp != NULL) {

			/* interpret the response */

			if (retval == PAM_SUCCESS) {	/* a good conversation */

				token = x_strdup(resp[i - replies].resp);
				if (token != NULL) {
					if (replies == 2) {

						/* verify that password entered correctly */
						if (!resp[i - 1].resp
						    || strcmp(token, resp[i - 1].resp)) {
							_pam_delete(token);	/* mistyped */
							retval = PAM_AUTHTOK_RECOVER_ERR;
							_make_remark(pamh, ctrl
								    ,PAM_ERROR_MSG, MISTYPED_PASS);
						}
					}
				} else {
					_log_err(LOG_NOTICE, pamh
						 ,"could not recover authentication token");
				}

			}
			/*
			 * tidy up the conversation (resp_retcode) is ignored
			 * -- what is it for anyway? AGM
			 */

			_pam_drop_reply(resp, i);

		} else {
			retval = (retval == PAM_SUCCESS)
			    ? PAM_AUTHTOK_RECOVER_ERR : retval;
		}
	}

	if (retval != PAM_SUCCESS) {
		if (on(UNIX_DEBUG, ctrl))
			_log_err(LOG_DEBUG, pamh,
			         "unable to obtain a password");
		return retval;
	}
	/* 'token' is the entered password */

	if (off(UNIX_NOT_SET_PASS, ctrl)) {

		/* we store this password as an item */

		retval = pam_set_item(pamh, authtok_flag, token);
		_pam_delete(token);	/* clean it up */
		if (retval != PAM_SUCCESS
		    || (retval = pam_get_item(pamh, authtok_flag
					      ,(const void **) &item))
		    != PAM_SUCCESS) {

			_log_err(LOG_CRIT, pamh, "error manipulating password");
			return retval;

		}
	} else {
		/*
		 * then store it as data specific to this module. pam_end()
		 * will arrange to clean it up.
		 */

		retval = pam_set_data(pamh, data_name, (void *) token, _cleanup);
		if (retval != PAM_SUCCESS) {
			_log_err(LOG_CRIT, pamh
			         ,"error manipulating password data [%s]"
				 ,pam_strerror(pamh, retval));
			_pam_delete(token);
			return retval;
		}
		item = token;
		token = NULL;	/* break link to password */
	}

	*pass = item;
	item = NULL;		/* break link to password */

	return PAM_SUCCESS;
}

/* ****************************************************************** *
 * Copyright (c) Jan Rêkorajski 1999.
 * Copyright (c) Andrew G. Morgan 1996-8.
 * Copyright (c) Alex O. Yuriev, 1996.
 * Copyright (c) Cristian Gafton 1996.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
