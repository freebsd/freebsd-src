/*-
 * Copyright 1998 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <security/pam_modules.h>
#include "pam_mod_misc.h"

static int	 pam_conv_pass(pam_handle_t *, const char *, int);

static int
pam_conv_pass(pam_handle_t *pamh, const char *prompt, int options)
{
	int retval;
	const void *item;
	const struct pam_conv *conv;
	struct pam_message msg;
	const struct pam_message *msgs[1];
	struct pam_response *resp;

	if ((retval = pam_get_item(pamh, PAM_CONV, &item)) !=
	    PAM_SUCCESS)
		return retval;
	conv = (const struct pam_conv *)item;
	msg.msg_style = options & PAM_OPT_ECHO_PASS ?
	    PAM_PROMPT_ECHO_ON : PAM_PROMPT_ECHO_OFF;
	msg.msg = prompt;
	msgs[0] = &msg;
	if ((retval = conv->conv(1, msgs, &resp, conv->appdata_ptr)) !=
	    PAM_SUCCESS)
		return retval;
	if ((retval = pam_set_item(pamh, PAM_AUTHTOK, resp[0].resp)) !=
	    PAM_SUCCESS)
		return retval;
	memset(resp[0].resp, 0, strlen(resp[0].resp));
	free(resp[0].resp);
	free(resp);
	return PAM_SUCCESS;
}

int
pam_get_pass(pam_handle_t *pamh, const char **passp, const char *prompt,
    int options)
{
	int retval;
	const void *item = NULL;

	/*
	 * Grab the already-entered password if we might want to use it.
	 */
	if (options & (PAM_OPT_TRY_FIRST_PASS | PAM_OPT_USE_FIRST_PASS)) {
		if ((retval = pam_get_item(pamh, PAM_AUTHTOK, &item)) !=
		    PAM_SUCCESS)
			return retval;
	}

	if (item == NULL) {
		/* The user hasn't entered a password yet. */
		if (options & PAM_OPT_USE_FIRST_PASS)
			return PAM_AUTH_ERR;
		/* Use the conversation function to get a password. */
		if ((retval = pam_conv_pass(pamh, prompt, options)) !=
		    PAM_SUCCESS ||
		    (retval = pam_get_item(pamh, PAM_AUTHTOK, &item)) !=
		    PAM_SUCCESS)
			return retval;
	}
	*passp = (const char *)item;
	return PAM_SUCCESS;
}
