#include "includes.h"
RCSID("$OpenBSD: auth2-skey.c,v 1.1 2000/10/11 20:14:38 markus Exp $");

#include "ssh.h"
#include "ssh2.h"
#include "auth.h"
#include "packet.h"
#include "xmalloc.h"
#include "dispatch.h"

void	send_userauth_into_request(Authctxt *authctxt, int echo);
void	input_userauth_info_response(int type, int plen, void *ctxt);

/*
 * try skey authentication, always return -1 (= postponed) since we have to
 * wait for the s/key response.
 */
int
auth2_skey(Authctxt *authctxt)
{
	send_userauth_into_request(authctxt, 0);
	dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE, &input_userauth_info_response);
	return -1;
}

void
send_userauth_into_request(Authctxt *authctxt, int echo)
{
	int retval = -1;
	struct skey skey;
	char challenge[SKEY_MAX_CHALLENGE];
	char *fake;

	if (authctxt->user == NULL)
		fatal("send_userauth_into_request: internal error: no user");

	/* get skey challenge */
	if (authctxt->valid)
		retval = skeychallenge(&skey, authctxt->user, challenge);

	if (retval == -1) {
		fake = skey_fake_keyinfo(authctxt->user);
		strlcpy(challenge, fake, sizeof challenge);
	}
	/* send our info request */
	packet_start(SSH2_MSG_USERAUTH_INFO_REQUEST);
	packet_put_cstring("S/Key Authentication");	/* Name */
	packet_put_cstring(challenge);			/* Instruction */
	packet_put_cstring("");				/* Language */
	packet_put_int(1);			 	/* Number of prompts */
	packet_put_cstring(echo ?
		 "Response [Echo]: ": "Response: ");	/* Prompt */
	packet_put_char(echo);				/* Echo */
	packet_send();
	packet_write_wait();
	memset(challenge, 'c', sizeof challenge);
}

void
input_userauth_info_response(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	int authenticated = 0;
	unsigned int nresp, rlen;
	char *resp, *method;

	if (authctxt == NULL)
		fatal("input_userauth_info_response: no authentication context");

	if (authctxt->attempt++ >= AUTH_FAIL_MAX)
		packet_disconnect("too many failed userauth_requests");

	nresp = packet_get_int();
	if (nresp == 1) {
		/* we only support s/key and assume s/key for nresp == 1 */
		method = "s/key";
		resp = packet_get_string(&rlen);
		packet_done();
		if (strlen(resp) == 0) {
			/*
			 * if we received a null response, resend prompt with
			 * echo enabled
			 */
			authenticated = -1;
			userauth_log(authctxt, authenticated, method);
			send_userauth_into_request(authctxt, 1);
		} else {
			/* verify skey response */
			if (authctxt->valid &&
			    skey_haskey(authctxt->pw->pw_name) == 0 &&
			    skey_passcheck(authctxt->pw->pw_name, resp) != -1) {
				authenticated = 1;
			} else {
				authenticated = 0;
			}
			memset(resp, 'r', rlen);
			/* unregister callback */
			dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE, NULL);
			userauth_log(authctxt, authenticated, method);
			userauth_reply(authctxt, authenticated);
		}
		xfree(resp);
	}
}
