/*
 * pam_radius 
 *      Process an user session according to a RADIUS server response
 *
 * 1.0 - initial release - Linux ONLY
 * 1.1 - revised and reorganized for libpwdb 0.54preB or higher
 *     - removed the conf= parameter, since we use libpwdb exclusively now
 *
 * See end for Copyright information
 */

#if !(defined(linux))
#error THIS CODE IS KNOWN TO WORK ONLY ON LINUX !!!
#endif 

/* Module defines */
#define BUFFER_SIZE      1024
#define LONG_VAL_PTR(ptr) ((*(ptr)<<24)+(*((ptr)+1)<<16)+(*((ptr)+2)<<8)+(*((ptr)+3)))

#define PAM_SM_SESSION

#include "pam_radius.h"

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

static time_t session_time;

/* we need to save these from open_session to close_session, since
 * when close_session will be called we won't be root anymore and
 * won't be able to access again the radius server configuration file
 * -- cristiang */

static RADIUS_SERVER rad_server;
static char hostname[BUFFER_SIZE];
static char secret[BUFFER_SIZE];

/* logging */
static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("pam_radius", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

/* argument parsing */

#define PAM_DEBUG_ARG       0x0001

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

/* now the session stuff */
PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags,
                                   int argc, const char **argv)
{
    int retval;
    char *user_name;
    int ctrl;
        
    ctrl = _pam_parse(argc, argv); 
    retval = pam_get_item( pamh, PAM_USER, (void*) &user_name );
    if ( user_name == NULL || retval != PAM_SUCCESS ) {
        _pam_log(LOG_CRIT, "open_session - error recovering username");
        return PAM_SESSION_ERR;
     }
	
    if (ctrl & PAM_DEBUG_ARG)
        _pam_log(LOG_DEBUG, "starting RADIUS user session for '%s'",
                             user_name);

    retval = get_server_entries(hostname, secret);
    if ((retval != PWDB_RADIUS_SUCCESS) ||
        !strlen(hostname) || !strlen(secret)) {
	_pam_log(LOG_CRIT, "Could not determine the radius server to talk to");
        return PAM_IGNORE;
    }
    session_time = time(NULL);
    rad_server.hostname = hostname;
    rad_server.secret = secret;
    retval = radius_acct_start(rad_server, user_name);
    if (retval != PWDB_RADIUS_SUCCESS) {
	if (ctrl & PAM_DEBUG_ARG)
	    _pam_log(LOG_DEBUG, "ERROR communicating with the RADIUS server");
	return PAM_IGNORE;
    }

    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags,
                                    int argc, const char **argv)
{
    int ctrl;
    char *user_name;
    int retval;

    ctrl = _pam_parse(argc, argv); 
    retval = pam_get_item( pamh, PAM_USER, (void*) &user_name );
    if ( user_name == NULL || retval != PAM_SUCCESS ) {
        _pam_log(LOG_CRIT, "open_session - error recovering username");
        return PAM_SESSION_ERR;
     }

    if (ctrl & PAM_DEBUG_ARG)
         _pam_log(LOG_DEBUG, "closing RADIUS user session for '%s'",
                              user_name);

    if (!strlen(hostname) || !strlen(secret)) {
        _pam_log(LOG_CRIT, "Could not determine the radius server to talk to");
        return PAM_IGNORE;
    }
    retval = radius_acct_stop(rad_server, user_name, 
			      time(NULL) - session_time);
    if (retval != PWDB_RADIUS_SUCCESS) {
        if (ctrl & PAM_DEBUG_ARG)
            _pam_log(LOG_DEBUG, "ERROR communicating with the RADIUS server");
        return PAM_IGNORE;
    }
    
    return PAM_SUCCESS;
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_radius_modstruct = {
     "pam_radius",
     NULL,
     NULL,
     NULL,
     pam_sm_open_session,
     pam_sm_close_session,
     NULL
};
#endif

/*
 * Copyright (c) Cristian Gafton, 1996, <gafton@redhat.com>
 *                                              All rights reserved.
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
