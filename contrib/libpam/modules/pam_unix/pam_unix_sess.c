/* 
 * $Header: /home/morgan/pam/Linux-PAM-0.53/modules/pam_unix/RCS/pam_unix_sess.c,v 1.1 1996/11/09 19:44:35 morgan Exp $
 */

/*
 * Copyright Alexander O. Yuriev, 1996.  All rights reserved.
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

/* 
 * $Log: pam_unix_sess.c,v $
 * Revision 1.1  1996/11/09 19:44:35  morgan
 * Initial revision
 *
 * Revision 1.4  1996/05/21 03:55:17  morgan
 * added "const" to definition of rcsid[]
 *
 * Revision 1.3  1996/04/23 16:32:28  alex
 * nothing really got changed.
 *
 * Revision 1.2  1996/04/19 03:23:33  alex
 * session code implemented. account management moved into pam_unix_acct.c
 *
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>

#ifndef LINUX    /* AGM added this as of 0.2 */

	#include <security/pam_appl.h>

#endif           /* ditto */

#include <security/pam_modules.h>
#include <syslog.h>
#include <unistd.h>
#ifndef LOG_AUTHPRIV
#define LOG_AUTHPRIV LOG_AUTH
#endif

static const char rcsid[] = "$Id: pam_unix_sess.c,v 1.1 1996/11/09 19:44:35 morgan Exp $ pam_unix session management. alex@bach.cis.temple.edu";

/* Define internal functions */

static	int _get_log_level(	pam_handle_t *pamh,
				int flags,
				int argc,
				const char **argv	);
				
int _pam_unix_open_session(	pam_handle_t *pamh,
				int flags,
				int argc,
				const char **argv	);

int _pam_unix_close_session(	pam_handle_t *pamh,
				int flags,
				int argc,
				const char **argv	);

/* Implementation */

static	int _get_log_level(	pam_handle_t *pamh,
				int flags,
				int argc,
				const char **argv	)
{
	int	i = argc;
	int	log_level = LOG_DEBUG;
				
	while ( i-- ) 
		{
			if ( strcmp( *argv, "debug" ) == 0 )
					log_level = LOG_DEBUG;
			else if ( strcmp ( *argv, "trace" ) == 0 )
					log_level = LOG_AUTHPRIV;
			argv++;			
            	}

	return	log_level;         
}

int _pam_unix_open_session(	pam_handle_t *pamh,
				int flags,
				int argc,
				const char **argv	)
{
	int	log_level;
	char	*user_name, *service;
 	
	
	log_level = _get_log_level( pamh, flags, argc, argv );

	pam_get_item( pamh, PAM_USER, (void*) &user_name );
	if ( !user_name )
		return	PAM_CONV_ERR; /* How did we get authenticated with
					no username?! */
	
	pam_get_item( pamh, PAM_SERVICE, (void*) &service );
	if ( !service )
		return	PAM_CONV_ERR;
		
	syslog ( log_level, 
		"pam_unix authentication session started, user %s, service %s\n",
		user_name, service );
		
	return PAM_SUCCESS;
}

int _pam_unix_close_session(	pam_handle_t *pamh,
				int flags,
				int argc,
				const char **argv	)
{
	int	log_level;
	char	*user_name, *service;
 	
	log_level = _get_log_level( pamh, flags, argc, argv );

	pam_get_item( pamh, PAM_USER, (void*) &user_name );
	if ( !user_name )
		return	PAM_CONV_ERR; /* How did we get authenticated with
					no username?! */
	
	pam_get_item( pamh, PAM_SERVICE, (void*) &service );
	if ( !service )
		return	PAM_CONV_ERR;
		
	syslog ( log_level, 
		"pam_unix authentication session finished, user %s, service %s\n",
		user_name, service );
		
	return PAM_SUCCESS;
}

int pam_sm_open_session(	pam_handle_t *pamh, 
				int flags,
				int argc, 
				const char **argv	)
{
    return _pam_unix_open_session( pamh, flags, argc, argv ) ;
}

int pam_sm_close_session(pam_handle_t *pamh, int flags,
			 int argc, const char **argv)
{
    return _pam_unix_close_session( pamh, flags, argc, argv ) ;
}

