/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: auth2-kbdint.c,v 1.2 2002/05/31 11:35:15 markus Exp $");

#include "packet.h"
#include "auth.h"
#include "log.h"
#include "servconf.h"
#include "xmalloc.h"

/* import */
extern ServerOptions options;

static int
userauth_kbdint(Authctxt *authctxt)
{
	int authenticated = 0;
	char *lang, *devs;

	lang = packet_get_string(NULL);
	devs = packet_get_string(NULL);
	packet_check_eom();

	debug("keyboard-interactive devs %s", devs);

	if (options.challenge_response_authentication)
		authenticated = auth2_challenge(authctxt, devs);

	xfree(devs);
	xfree(lang);
#ifdef HAVE_CYGWIN
	if (check_nt_auth(0, authctxt->pw) == 0)
		authenticated = 0;
#endif
	return authenticated;
}

Authmethod method_kbdint = {
	"keyboard-interactive",
	userauth_kbdint,
	&options.kbd_interactive_authentication
};
