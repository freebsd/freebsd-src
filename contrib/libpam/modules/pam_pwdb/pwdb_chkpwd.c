/*
 * $Id: pwdb_chkpwd.c,v 1.3 2001/02/11 06:33:53 agmorgan Exp $
 *
 * This program is designed to run setuid(root) or with sufficient
 * privilege to read all of the unix password databases. It is designed
 * to provide a mechanism for the current user (defined by this
 * process' real uid) to verify their own password.
 *
 * The password is read from the standard input. The exit status of
 * this program indicates whether the user is authenticated or not.
 *
 * Copyright information is located at the end of the file.
 *
 */

#include <security/_pam_aconf.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <security/_pam_macros.h>

#define MAXPASS      200        /* the maximum length of a password */

#define UNIX_PASSED  (PWDB_SUCCESS)
#define UNIX_FAILED  (PWDB_SUCCESS+1)

#include <pwdb/pwdb_public.h>

/* syslogging function for errors and other information */

static void _log_err(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("pwdb_chkpwd", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

#define PWDB_NO_MD_COMPAT
#include "pam_unix_md.-c"

static int _unix_verify_passwd(const char *salt, const char *p)
{
    char *pp=NULL;
    int retval;

    if (p == NULL) {
	if (*salt == '\0') {
	    retval = UNIX_PASSED;
	} else {
	    retval = UNIX_FAILED;
	}
    } else {
	pp = _pam_md(p, salt);
	p = NULL;                     /* no longer needed here */

	if ( strcmp( pp, salt ) == 0 ) {
	    retval = UNIX_PASSED;
	} else {
	    retval = UNIX_FAILED;
	}
    }

    /* clean up */
    {
	char *tp = pp;
	if (pp != NULL) {
	    while(tp && *tp)
		*tp++ = '\0';
	    free(pp);
	    pp = tp = NULL;
	}
    }

    return retval;
}

int main(int argc, char **argv)
{
    const struct pwdb *pw=NULL;
    const struct pwdb_entry *pwe=NULL;
    char pass[MAXPASS+1];
    int npass, force_failure=0;
    int retval=UNIX_FAILED;

    /*
     * we establish that this program is running with non-tty stdin.
     * this is to discourage casual use. It does *NOT* prevent an
     * intruder from repeatadly running this program to determine the
     * password of the current user (brute force attack, but one for
     * which the attacker must already have gained access to the user's
     * account).
     */

    if ( isatty(STDIN_FILENO) ) {
	_log_err(LOG_NOTICE
		 , "inappropriate use of PWDB helper binary [UID=%d]"
		 , getuid() );
	fprintf(stderr,
		"This program is not designed for running in this way\n"
		"-- the system administrator has been informed\n");
	exit(UNIX_FAILED);
    }

    /*
     * determine the current user's name:
     */

    retval = pwdb_start();
    if (retval != PWDB_SUCCESS) {
	_log_err(LOG_ALERT, "failed to open pwdb");
	retval = UNIX_FAILED;
    }
    if (retval != UNIX_FAILED) {
	retval = pwdb_locate("user", PWDB_DEFAULT, PWDB_NAME_UNKNOWN,
			     getuid(), &pw);
    }
    if (retval != PWDB_SUCCESS) {
	_log_err(LOG_ALERT, "could not identify user");
	while (pwdb_end() != PWDB_SUCCESS);
	exit(UNIX_FAILED);
    }
    if (argc == 2) {
	if (pwdb_get_entry(pw, "user", &pwe) == PWDB_SUCCESS) {
	    if (pwe == NULL) {
		force_failure = 1;
	    } else {
		if (strcmp((const char *) pwe->value, argv[1])) {
		    force_failure = 1;
		}
		pwdb_entry_delete(&pwe);
	    }
	}
    }

    /* read the password from stdin (a pipe from the pam_pwdb module) */

    npass = read(STDIN_FILENO, pass, MAXPASS);

    if (npass < 0) {                             /* is it a valid password? */
	_log_err(LOG_DEBUG, "no password supplied");
	retval = UNIX_FAILED;
    } else if (npass >= MAXPASS-1) {
	_log_err(LOG_DEBUG, "password too long");
	retval = UNIX_FAILED;
    } else if (pwdb_get_entry(pw, "passwd", &pwe) != PWDB_SUCCESS) {
	_log_err(LOG_WARNING, "password not found");
	retval = UNIX_FAILED;
    } else {
	if (npass <= 0) {
	    /* the password is NULL */

	    retval = _unix_verify_passwd((const char *)(pwe->value), NULL);
	} else {
	    /* does pass agree with the official one? */

	    pass[npass] = '\0';                     /* NUL terminate */
	    retval = _unix_verify_passwd((const char *)(pwe->value), pass);
	}
    }

    memset(pass, '\0', MAXPASS);        /* clear memory of the password */
    while (pwdb_end() != PWDB_SUCCESS);

    if ((retval != UNIX_FAILED) && force_failure) {
	retval = UNIX_FAILED;
    }
    
    /* return pass or fail */

    exit(retval);
}

/*
 * Copyright (c) Andrew G. Morgan, 1997. All rights reserved
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
