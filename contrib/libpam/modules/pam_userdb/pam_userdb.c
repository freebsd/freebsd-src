/* pam_userdb module */
 
/*
 * $Id: pam_userdb.c,v 1.4 2000/12/04 15:02:16 baggins Exp $
 * $FreeBSD$
 * Written by Cristian Gafton <gafton@redhat.com> 1996/09/10
 * See the end of the file for Copyright Information
 */

#include <security/_pam_aconf.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "pam_userdb.h"

#ifdef HAVE_NDBM_H
# include <ndbm.h>
#else
# ifdef HAVE_DB_H
#  define DB_DBM_HSEARCH    1 /* use the dbm interface */
#  include <db.h>
# else
#  error "failed to find a libdb or equivalent"
# endif
#endif

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT

#include <security/pam_modules.h>

/* some syslogging */

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog(MODULE_NAME, LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

char * database	 = NULL;
static int ctrl	 = 0;

static int _pam_parse(int argc, const char **argv)
{
     /* step through arguments */
     for (ctrl = 0; argc-- > 0; ++argv) {

          /* generic options */

          if (!strcmp(*argv,"debug"))
               ctrl |= PAM_DEBUG_ARG;
	  else if (!strcasecmp(*argv, "icase"))
	      ctrl |= PAM_ICASE_ARG;
	  else if (!strcasecmp(*argv, "dump"))
	      ctrl |= PAM_DUMP_ARG;
          else if (!strncasecmp(*argv,"db=", 3)) {
	      database = strdup((*argv) + 3);
	      if (database == NULL)
		  _pam_log(LOG_ERR, "pam_parse: could not parse argument \"%s\"",
			   *argv);
	  } else {
               _pam_log(LOG_ERR, "pam_parse: unknown option; %s", *argv);
          }
     }

     return ctrl;
}


/*
 * Looks up an user name in a database and checks the password
 *
 * return values:
 *	 1  = User not found
 *	 0  = OK
 * 	-1  = Password incorrect
 *	-2  = System error
 */
static int user_lookup(const char *user, const char *pass)
{
    DBM *dbm;
    datum key, data;

    /* Open the DB file. */
    dbm = dbm_open(database, O_RDONLY, 0644);
    if (dbm == NULL) {
	_pam_log(LOG_ERR, "user_lookup: could not open database `%s'",
		 database);
	return -2;
    }

    if (ctrl &PAM_DUMP_ARG) {
	_pam_log(LOG_INFO, "Database dump:");
	for (key = dbm_firstkey(dbm);  key.dptr != NULL;
	     key = dbm_nextkey(dbm)) {
	    data = dbm_fetch(dbm, key);
	    _pam_log(LOG_INFO, "key[len=%d] = `%s', data[len=%d] = `%s'",
		     key.dsize, key.dptr, data.dsize, data.dptr);
	}
    } 
    /* do some more init work */

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.dptr = x_strdup(user);
    key.dsize = strlen(user);
    user = NULL;

    if (key.dptr) {
	data = dbm_fetch(dbm, key);
	memset(key.dptr, 0, key.dsize);
	free(key.dptr);
    }

    if (ctrl & PAM_DEBUG_ARG) {
	_pam_log(LOG_INFO, "password in database is [%p]`%s', len is %d",
		 data.dptr, (char *) data.dptr, data.dsize);
    }

    if (data.dptr != NULL) {
	int compare = 0;
	/* bingo, got it */
	if (ctrl & PAM_ICASE_ARG)
	    compare = strncasecmp(pass, data.dptr, data.dsize);
	else
	    compare = strncmp(pass, data.dptr, data.dsize);
	dbm_close(dbm);
	if (compare == 0)
	    return 0; /* match */
	else
	    return -1; /* wrong */
    } else {
	if (ctrl & PAM_DEBUG_ARG) {    
	    _pam_log(LOG_INFO, "error returned by dbm_fetch: %s",
		     strerror(errno));
	}
	dbm_close(dbm);
	/* probably we should check dbm_error() here */
	return 1; /* not found */
    }

    /* NOT REACHED */
    return -2;
}

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
     const char *username;
     const char *password;
     int retval = PAM_AUTH_ERR;
    
     /* parse arguments */
     ctrl = _pam_parse(argc, argv);

     /* Get the username */
     retval = pam_get_user(pamh, &username, NULL);
     if ((retval != PAM_SUCCESS) || (!username)) {
        if (ctrl & PAM_DEBUG_ARG)
            _pam_log(LOG_DEBUG,"can not get the username");
        return PAM_SERVICE_ERR;
     }
     
     /* Converse just to be sure we have the password */
     retval = conversation(pamh);
     if (retval != PAM_SUCCESS) {
	 _pam_log(LOG_ERR, "could not obtain password for `%s'",
		  username);
	 return -2;
     }
     
     /* Get the password */
     retval = pam_get_item(pamh, PAM_AUTHTOK, (const void **)&password);
     if (retval != PAM_SUCCESS) {
	 _pam_log(LOG_ERR, "Could not retrive user's password");
	 return -2;
     }
     
     if (ctrl & PAM_DEBUG_ARG)
	 _pam_log(LOG_INFO, "Verify user `%s' with password `%s'",
		  username, password);
     
     /* Now use the username to look up password in the database file */
     retval = user_lookup(username, password);
     switch (retval) {
	 case -2:
	     /* some sort of system error. The log was already printed */
	     return PAM_SERVICE_ERR;    
	 case -1:
	     /* incorrect password */
	     _pam_log(LOG_WARNING,
		      "user `%s' denied access (incorrect password)",
		      username);
	     return PAM_AUTH_ERR;
	 case 1:
	     /* the user does not exist in the database */
	     if (ctrl & PAM_DEBUG_ARG)
		 _pam_log(LOG_NOTICE, "user `%s' not found in the database",
			  username);
	     return PAM_USER_UNKNOWN;
	 case 0:
	     /* Otherwise, the authentication looked good */
	     _pam_log(LOG_NOTICE, "user '%s' granted acces", username);
	     return PAM_SUCCESS;
	 default:
	     /* we don't know anything about this return value */
	     _pam_log(LOG_ERR,
		      "internal module error (retval = %d, user = `%s'",
		      retval, username);
	     return PAM_SERVICE_ERR;
     }

     /* should not be reached */
     return PAM_IGNORE;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
    return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
    return PAM_SUCCESS;
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_userdb_modstruct = {
     "pam_userdb",
     pam_sm_authenticate,
     pam_sm_setcred,
     NULL,
     NULL,
     NULL,
     NULL,
};

#endif

/*
 * Copyright (c) Cristian Gafton <gafton@redhat.com>, 1999
 *                                              All rights reserved
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
 * THIS SOFTWARE IS PROVIDED `AS IS'' AND ANY EXPRESS OR IMPLIED
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
