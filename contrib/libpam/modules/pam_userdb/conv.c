/*
 * Conversation related functions
 */

/* $Id */
/* Copyright at the end of the file */

#define _BSD_SOURCE

#include <stdlib.h>
#include <string.h>

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

#include "pam_userdb.h"

/*
 * dummy conversation function sending exactly one prompt
 * and expecting exactly one response from the other party
 */
static int converse(pam_handle_t *pamh,
		    struct pam_message **message,
		    struct pam_response **response)
{
    int retval;
    const struct pam_conv *conv;

    retval = pam_get_item(pamh, PAM_CONV, (const void **) &conv ) ;
    if (retval == PAM_SUCCESS)
	retval = conv->conv(1, (const struct pam_message **)message,
			    response, conv->appdata_ptr);
	
    return retval; /* propagate error status */
}


static char *_pam_delete(register char *xx)
{
    _pam_overwrite(xx);
    _pam_drop(xx);
    return NULL;
}

/*
 * This is a conversation function to obtain the user's password
 */
int conversation(pam_handle_t *pamh)
{
    struct pam_message msg[2],*pmsg[2];
    struct pam_response *resp;
    int retval;
    char * token = NULL;
    
    pmsg[0] = &msg[0];
    msg[0].msg_style = PAM_PROMPT_ECHO_OFF;
    msg[0].msg = "Password: ";

    /* so call the conversation expecting i responses */
    resp = NULL;
    retval = converse(pamh, pmsg, &resp);

    if (resp != NULL) {
	const char * item;
	/* interpret the response */
	if (retval == PAM_SUCCESS) {     /* a good conversation */
	    token = x_strdup(resp[0].resp);
	    if (token == NULL) {
		return PAM_AUTHTOK_RECOVER_ERR;
	    }
	}

	/* set the auth token */
	retval = pam_set_item(pamh, PAM_AUTHTOK, token);
	token = _pam_delete(token);   /* clean it up */
	if ( (retval != PAM_SUCCESS) ||
	     (retval = pam_get_item(pamh, PAM_AUTHTOK, (const void **)&item))
	     != PAM_SUCCESS ) {
	    return retval;
	}
	
	_pam_drop_reply(resp, 1);
    } else {
	retval = (retval == PAM_SUCCESS)
	    ? PAM_AUTHTOK_RECOVER_ERR:retval ;
    }

    return retval;
}

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
