/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: auth-chall.c,v 1.9 2003/11/03 09:03:37 djm Exp $");
RCSID("$FreeBSD$");

#include "auth.h"
#include "log.h"
#include "xmalloc.h"

/* limited protocol v1 interface to kbd-interactive authentication */

extern KbdintDevice *devices[];
static KbdintDevice *device;

char *
get_challenge(Authctxt *authctxt)
{
	char *challenge, *name, *info, **prompts;
	u_int i, numprompts;
	u_int *echo_on;

	device = devices[0]; /* we always use the 1st device for protocol 1 */
	if (device == NULL)
		return NULL;
	if ((authctxt->kbdintctxt = device->init_ctx(authctxt)) == NULL)
		return NULL;
	if (device->query(authctxt->kbdintctxt, &name, &info,
	    &numprompts, &prompts, &echo_on)) {
		device->free_ctx(authctxt->kbdintctxt);
		authctxt->kbdintctxt = NULL;
		return NULL;
	}
	if (numprompts < 1)
		fatal("get_challenge: numprompts < 1");
	challenge = xstrdup(prompts[0]);
	for (i = 0; i < numprompts; i++)
		xfree(prompts[i]);
	xfree(prompts);
	xfree(name);
	xfree(echo_on);
	xfree(info);

	return (challenge);
}
int
verify_response(Authctxt *authctxt, const char *response)
{
	char *resp[1], *name, *info, **prompts;
	u_int i, numprompts, *echo_on;
	int authenticated = 0;

	if (device == NULL)
		return 0;
	if (authctxt->kbdintctxt == NULL)
		return 0;
	resp[0] = (char *)response;
	switch (device->respond(authctxt->kbdintctxt, 1, resp)) {
	case 0: /* Success */
		authenticated = 1;
		break;
	case 1: /* Postponed - retry with empty query for PAM */
		if ((device->query(authctxt->kbdintctxt, &name, &info,
		    &numprompts, &prompts, &echo_on)) != 0)
			break;
		if (numprompts == 0 &&
		    device->respond(authctxt->kbdintctxt, 0, resp) == 0)
			authenticated = 1;

		for (i = 0; i < numprompts; i++)
			xfree(prompts[i]);
		xfree(prompts);
		xfree(name);
		xfree(echo_on);
		xfree(info);
		break;
	}
	device->free_ctx(authctxt->kbdintctxt);
	authctxt->kbdintctxt = NULL;
	return authenticated;
}
void
abandon_challenge_response(Authctxt *authctxt)
{
	if (authctxt->kbdintctxt != NULL) {
		device->free_ctx(authctxt->kbdintctxt);
		authctxt->kbdintctxt = NULL;
	}
}
