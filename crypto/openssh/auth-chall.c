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
RCSID("$OpenBSD: auth-chall.c,v 1.7 2001/04/05 10:42:47 markus Exp $");
RCSID("$FreeBSD$");

#include "auth.h"
#include "log.h"

#ifdef BSD_AUTH
char *
get_challenge(Authctxt *authctxt, char *devs)
{
	char *challenge;

	if (authctxt->as != NULL) {
		debug2("try reuse session");
		challenge = auth_getitem(authctxt->as, AUTHV_CHALLENGE);
		if (challenge != NULL) {
			debug2("reuse bsd auth session");
			return challenge;
		}
		auth_close(authctxt->as);
		authctxt->as = NULL;
	}
	debug2("new bsd auth session");
	if (devs == NULL || strlen(devs) == 0)
		devs = authctxt->style;
	debug3("bsd auth: devs %s", devs ? devs : "<default>");
	authctxt->as = auth_userchallenge(authctxt->user, devs, "auth-ssh",
	    &challenge);
	if (authctxt->as == NULL)
		return NULL;
	debug2("get_challenge: <%s>", challenge ? challenge : "EMPTY");
	return challenge;
}
int
verify_response(Authctxt *authctxt, char *response)
{
	int authok;

	if (authctxt->as == 0)
		error("verify_response: no bsd auth session");
	authok = auth_userresponse(authctxt->as, response, 0);
	authctxt->as = NULL;
	debug("verify_response: <%s> = <%d>", response, authok);
	return authok != 0;
}
#else
#ifdef SKEY
#include <opie.h>

char *
get_challenge(Authctxt *authctxt, char *devs)
{
	static char challenge[1024];
	struct opie opie;
	if (opiechallenge(&opie, authctxt->user, challenge) == -1)
		return NULL;
	strlcat(challenge, "\nS/Key Password: ", sizeof challenge);
	return challenge;
}
int
verify_response(Authctxt *authctxt, char *response)
{
	return (authctxt->valid &&
	    opie_haskey(authctxt->pw->pw_name) == 0 &&
	    opie_passverify(authctxt->pw->pw_name, response) != -1);
}
#else
/* not available */
char *
get_challenge(Authctxt *authctxt, char *devs)
{
	return NULL;
}
int
verify_response(Authctxt *authctxt, char *response)
{
	return 0;
}
#endif
#endif
