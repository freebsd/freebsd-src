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

/*
 * Do a simple conversation which can consist of a message and/or a user
 * response.
 */
int
pam_prompt(pam_handle_t *pamh, int style, const char *prompt, char **user_msg)
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
	msg.msg_style = style;
	msg.msg = prompt != NULL ? prompt : "";
	msgs[0] = &msg;
	if ((retval = conv->conv(1, msgs, &resp, conv->appdata_ptr)) !=
	    PAM_SUCCESS)
		return retval;
	if (user_msg != NULL)
		*user_msg = resp[0].resp;
	else if (resp[0].resp != NULL)
		free(resp[0].resp);
	free(resp);
	return PAM_SUCCESS;
}
