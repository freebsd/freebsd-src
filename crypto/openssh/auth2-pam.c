#include "includes.h"
RCSID("$Id: auth2-pam.c,v 1.14 2002/06/28 16:48:12 mouring Exp $");

#ifdef USE_PAM
#include <security/pam_appl.h>

#include "ssh.h"
#include "ssh2.h"
#include "auth.h"
#include "auth-pam.h"
#include "packet.h"
#include "xmalloc.h"
#include "dispatch.h"
#include "log.h"

static int do_pam_conversation_kbd_int(int num_msg, 
    const struct pam_message **msg, struct pam_response **resp, 
    void *appdata_ptr);
void input_userauth_info_response_pam(int type, u_int32_t seqnr, void *ctxt);

struct {
	int finished, num_received, num_expected;
	int *prompts;
	struct pam_response *responses;
} context_pam2 = {0, 0, 0, NULL};

static struct pam_conv conv2 = {
	do_pam_conversation_kbd_int,
	NULL,
};

int
auth2_pam(Authctxt *authctxt)
{
	int retval = -1;

	if (authctxt->user == NULL)
		fatal("auth2_pam: internal error: no user");

	conv2.appdata_ptr = authctxt;
	do_pam_set_conv(&conv2);

	dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE,
	    &input_userauth_info_response_pam);
	retval = (do_pam_authenticate(0) == PAM_SUCCESS);
	dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE, NULL);

	return retval;
}

static int
do_pam_conversation_kbd_int(int num_msg, const struct pam_message **msg,
    struct pam_response **resp, void *appdata_ptr)
{
	int i, j, done;
	char *text;

	context_pam2.finished = 0;
	context_pam2.num_received = 0;
	context_pam2.num_expected = 0;
	context_pam2.prompts = xmalloc(sizeof(int) * num_msg);
	context_pam2.responses = xmalloc(sizeof(struct pam_response) * num_msg);
	memset(context_pam2.responses, 0, sizeof(struct pam_response) * num_msg);

	text = NULL;
	for (i = 0, context_pam2.num_expected = 0; i < num_msg; i++) {
		int style = PAM_MSG_MEMBER(msg, i, msg_style);
		switch (style) {
		case PAM_PROMPT_ECHO_ON:
		case PAM_PROMPT_ECHO_OFF:
			context_pam2.num_expected++;
			break;
		case PAM_TEXT_INFO:
		case PAM_ERROR_MSG:
		default:
			/* Capture all these messages to be sent at once */
			message_cat(&text, PAM_MSG_MEMBER(msg, i, msg));
			break;
		}
	}

	if (context_pam2.num_expected == 0)
		return PAM_SUCCESS;

	packet_start(SSH2_MSG_USERAUTH_INFO_REQUEST);
	packet_put_cstring("");	/* Name */
	packet_put_cstring("");	/* Instructions */
	packet_put_cstring("");	/* Language */
	packet_put_int(context_pam2.num_expected);
	
	for (i = 0, j = 0; i < num_msg; i++) {
		int style = PAM_MSG_MEMBER(msg, i, msg_style);
		
		/* Skip messages which don't need a reply */
		if (style != PAM_PROMPT_ECHO_ON && style != PAM_PROMPT_ECHO_OFF)
			continue;
		
		context_pam2.prompts[j++] = i;
		if (text) {
			message_cat(&text, PAM_MSG_MEMBER(msg, i, msg));
			packet_put_cstring(text);
			text = NULL;
		} else
			packet_put_cstring(PAM_MSG_MEMBER(msg, i, msg));
		packet_put_char(style == PAM_PROMPT_ECHO_ON);
	}
	packet_send();
	packet_write_wait();

	/*
	 * Grabbing control of execution and spinning until we get what
	 * we want is probably rude, but it seems to work properly, and
	 * the client *should* be in lock-step with us, so the loop should
	 * only be traversed once.
	 */
	while(context_pam2.finished == 0) {
		done = 1;
		dispatch_run(DISPATCH_BLOCK, &done, appdata_ptr);
		if (context_pam2.finished == 0)
			debug("extra packet during conversation");
	}

	if (context_pam2.num_received == context_pam2.num_expected) {
		*resp = context_pam2.responses;
		return PAM_SUCCESS;
	} else
		return PAM_CONV_ERR;
}

void
input_userauth_info_response_pam(int type, u_int32_t seqnr, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	unsigned int nresp = 0, rlen = 0, i = 0;
	char *resp;

	if (authctxt == NULL)
		fatal("input_userauth_info_response_pam: no authentication context");

	nresp = packet_get_int();	/* Number of responses. */
	debug("got %d responses", nresp);


	if (nresp != context_pam2.num_expected)
		fatal("%s: Received incorrect number of responses "
		    "(expected %d, received %u)", __func__, 
		    context_pam2.num_expected, nresp);

	if (nresp > 100)
		fatal("%s: too many replies", __func__);

	for (i = 0; i < nresp; i++) {
		int j = context_pam2.prompts[i];

		resp = packet_get_string(&rlen);
		context_pam2.responses[j].resp_retcode = PAM_SUCCESS;
		context_pam2.responses[j].resp = xstrdup(resp);
		xfree(resp);
		context_pam2.num_received++;
	}

	context_pam2.finished = 1;

	packet_check_eom();
}
#endif
