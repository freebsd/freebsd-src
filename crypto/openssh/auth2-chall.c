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
RCSID("$OpenBSD: auth2-chall.c,v 1.4 2001/03/28 22:43:31 markus Exp $");

#include "ssh2.h"
#include "auth.h"
#include "packet.h"
#include "xmalloc.h"
#include "dispatch.h"
#include "log.h"

void send_userauth_into_request(Authctxt *authctxt, char *challenge, int echo);
void input_userauth_info_response(int type, int plen, void *ctxt);

/*
 * try challenge-reponse, return -1 (= postponed) if we have to
 * wait for the response.
 */
int
auth2_challenge(Authctxt *authctxt, char *devs)
{
	char *challenge;

	if (!authctxt->valid || authctxt->user == NULL)
		return 0;
	if ((challenge = get_challenge(authctxt, devs)) == NULL)
		return 0;
	send_userauth_into_request(authctxt, challenge, 0);
	dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE,
	    &input_userauth_info_response);
	authctxt->postponed = 1;
	return 0;
}

void
send_userauth_into_request(Authctxt *authctxt, char *challenge, int echo)
{
	int nprompts = 1;

	packet_start(SSH2_MSG_USERAUTH_INFO_REQUEST);
	/* name, instruction and language are unused */
	packet_put_cstring("");
	packet_put_cstring("");
	packet_put_cstring("");
	packet_put_int(nprompts);
	packet_put_cstring(challenge);
	packet_put_char(echo);
	packet_send();
	packet_write_wait();
}

void
input_userauth_info_response(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	int authenticated = 0;
	u_int nresp, rlen;
	char *response, *method = "challenge-reponse";

	if (authctxt == NULL)
		fatal("input_userauth_info_response: no authctxt");

	authctxt->postponed = 0;	/* reset */
	nresp = packet_get_int();
	if (nresp == 1) {
		response = packet_get_string(&rlen);
		packet_done();
		if (strlen(response) == 0) {
			/*
			 * if we received an empty response, resend challenge
			 * with echo enabled
			 */
			char *challenge = get_challenge(authctxt, NULL);
			if (challenge != NULL) {
				send_userauth_into_request(authctxt,
				    challenge, 1);
				authctxt->postponed = 1;
			}
		} else if (authctxt->valid) {
			authenticated = verify_response(authctxt, response);
			memset(response, 'r', rlen);
		}
		xfree(response);
	}
	/* unregister callback */
	if (!authctxt->postponed)
		dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE, NULL);

	userauth_finish(authctxt, authenticated, method);
}
