/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>

static int
_pam_echo(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
	struct pam_message msg;
	const struct pam_message *msgp;
	const struct pam_conv *pamc;
	struct pam_response *resp;
	size_t len;
	int i, pam_err;

	if (flags & PAM_SILENT)
		return (PAM_SUCCESS);
	pam_err = pam_get_item(pamh, PAM_CONV, (const void **)&pamc);
	if (pam_err != PAM_SUCCESS)
		return (pam_err);
	for (i = 0, len = 0; i < argc; ++i)
		len += strlen(argv[i]) + 1;
	if ((msg.msg = malloc(len)) == NULL)
		return (PAM_BUF_ERR);
	for (i = 0, len = 0; i < argc; ++i)
		len += sprintf(msg.msg + len, "%s%s", i ? " " : "", argv[i]);
	msg.msg[len] = '\0';
	msg.msg_style = PAM_TEXT_INFO;
	msgp = &msg;
	resp = NULL;
	pam_err = (pamc->conv)(1, &msgp, &resp, pamc->appdata_ptr);
	free(resp);
	free(msg.msg);
	return (pam_err);
}

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_echo(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_echo(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_echo(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_echo(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_echo(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_echo(pamh, flags, argc, argv));
}

PAM_MODULE_ENTRY("pam_echo");
