/* $OpenBSD: auth2-chall.c,v 1.34 2008/12/09 04:32:22 djm Exp $ */
/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Per Allansson.  All rights reserved.
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

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "xmalloc.h"
#include "ssh2.h"
#include "key.h"
#include "hostfile.h"
#include "auth.h"
#include "buffer.h"
#include "packet.h"
#include "dispatch.h"
#include "log.h"
#include "servconf.h"

/* import */
extern ServerOptions options;

static int auth2_challenge_start(Authctxt *);
static int send_userauth_info_request(Authctxt *);
static void input_userauth_info_response(int, u_int32_t, void *);

#ifdef BSD_AUTH
extern KbdintDevice bsdauth_device;
#else
#ifdef USE_PAM
extern KbdintDevice sshpam_device;
#endif
#ifdef SKEY
extern KbdintDevice skey_device;
#endif
#endif

KbdintDevice *devices[] = {
#ifdef BSD_AUTH
	&bsdauth_device,
#else
#ifdef USE_PAM
	&sshpam_device,
#endif
#ifdef SKEY
	&skey_device,
#endif
#endif
	NULL
};

typedef struct KbdintAuthctxt KbdintAuthctxt;
struct KbdintAuthctxt
{
	char *devices;
	void *ctxt;
	KbdintDevice *device;
	u_int nreq;
	u_int devices_done;
};

#ifdef USE_PAM
void
remove_kbdint_device(const char *devname)
{
	int i, j;

	for (i = 0; devices[i] != NULL; i++)
		if (strcmp(devices[i]->name, devname) == 0) {
			for (j = i; devices[j] != NULL; j++)
				devices[j] = devices[j+1];
			i--;
		}
}
#endif

static KbdintAuthctxt *
kbdint_alloc(const char *devs)
{
	KbdintAuthctxt *kbdintctxt;
	Buffer b;
	int i;

#ifdef USE_PAM
	if (!options.use_pam)
		remove_kbdint_device("pam");
#endif

	kbdintctxt = xmalloc(sizeof(KbdintAuthctxt));
	if (strcmp(devs, "") == 0) {
		buffer_init(&b);
		for (i = 0; devices[i]; i++) {
			if (buffer_len(&b) > 0)
				buffer_append(&b, ",", 1);
			buffer_append(&b, devices[i]->name,
			    strlen(devices[i]->name));
		}
		buffer_append(&b, "\0", 1);
		kbdintctxt->devices = xstrdup(buffer_ptr(&b));
		buffer_free(&b);
	} else {
		kbdintctxt->devices = xstrdup(devs);
	}
	debug("kbdint_alloc: devices '%s'", kbdintctxt->devices);
	kbdintctxt->ctxt = NULL;
	kbdintctxt->device = NULL;
	kbdintctxt->nreq = 0;

	return kbdintctxt;
}
static void
kbdint_reset_device(KbdintAuthctxt *kbdintctxt)
{
	if (kbdintctxt->ctxt) {
		kbdintctxt->device->free_ctx(kbdintctxt->ctxt);
		kbdintctxt->ctxt = NULL;
	}
	kbdintctxt->device = NULL;
}
static void
kbdint_free(KbdintAuthctxt *kbdintctxt)
{
	if (kbdintctxt->device)
		kbdint_reset_device(kbdintctxt);
	if (kbdintctxt->devices) {
		xfree(kbdintctxt->devices);
		kbdintctxt->devices = NULL;
	}
	xfree(kbdintctxt);
}
/* get next device */
static int
kbdint_next_device(KbdintAuthctxt *kbdintctxt)
{
	size_t len;
	char *t;
	int i;

	if (kbdintctxt->device)
		kbdint_reset_device(kbdintctxt);
	do {
		len = kbdintctxt->devices ?
		    strcspn(kbdintctxt->devices, ",") : 0;

		if (len == 0)
			break;
		for (i = 0; devices[i]; i++) {
			if ((kbdintctxt->devices_done & (1 << i)) != 0)
				continue;
			if (strncmp(kbdintctxt->devices, devices[i]->name, len) == 0) {
				kbdintctxt->device = devices[i];
				kbdintctxt->devices_done |= 1 << i;
			}
		}
		t = kbdintctxt->devices;
		kbdintctxt->devices = t[len] ? xstrdup(t+len+1) : NULL;
		xfree(t);
		debug2("kbdint_next_device: devices %s", kbdintctxt->devices ?
		    kbdintctxt->devices : "<empty>");
	} while (kbdintctxt->devices && !kbdintctxt->device);

	return kbdintctxt->device ? 1 : 0;
}

/*
 * try challenge-response, set authctxt->postponed if we have to
 * wait for the response.
 */
int
auth2_challenge(Authctxt *authctxt, char *devs)
{
	debug("auth2_challenge: user=%s devs=%s",
	    authctxt->user ? authctxt->user : "<nouser>",
	    devs ? devs : "<no devs>");

	if (authctxt->user == NULL || !devs)
		return 0;
	if (authctxt->kbdintctxt == NULL)
		authctxt->kbdintctxt = kbdint_alloc(devs);
	return auth2_challenge_start(authctxt);
}

/* unregister kbd-int callbacks and context */
void
auth2_challenge_stop(Authctxt *authctxt)
{
	/* unregister callback */
	dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE, NULL);
	if (authctxt->kbdintctxt != NULL) {
		kbdint_free(authctxt->kbdintctxt);
		authctxt->kbdintctxt = NULL;
	}
}

/* side effect: sets authctxt->postponed if a reply was sent*/
static int
auth2_challenge_start(Authctxt *authctxt)
{
	KbdintAuthctxt *kbdintctxt = authctxt->kbdintctxt;

	debug2("auth2_challenge_start: devices %s",
	    kbdintctxt->devices ?  kbdintctxt->devices : "<empty>");

	if (kbdint_next_device(kbdintctxt) == 0) {
		auth2_challenge_stop(authctxt);
		return 0;
	}
	debug("auth2_challenge_start: trying authentication method '%s'",
	    kbdintctxt->device->name);

	if ((kbdintctxt->ctxt = kbdintctxt->device->init_ctx(authctxt)) == NULL) {
		auth2_challenge_stop(authctxt);
		return 0;
	}
	if (send_userauth_info_request(authctxt) == 0) {
		auth2_challenge_stop(authctxt);
		return 0;
	}
	dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE,
	    &input_userauth_info_response);

	authctxt->postponed = 1;
	return 0;
}

static int
send_userauth_info_request(Authctxt *authctxt)
{
	KbdintAuthctxt *kbdintctxt;
	char *name, *instr, **prompts;
	u_int i, *echo_on;

	kbdintctxt = authctxt->kbdintctxt;
	if (kbdintctxt->device->query(kbdintctxt->ctxt,
	    &name, &instr, &kbdintctxt->nreq, &prompts, &echo_on))
		return 0;

	packet_start(SSH2_MSG_USERAUTH_INFO_REQUEST);
	packet_put_cstring(name);
	packet_put_cstring(instr);
	packet_put_cstring("");		/* language not used */
	packet_put_int(kbdintctxt->nreq);
	for (i = 0; i < kbdintctxt->nreq; i++) {
		packet_put_cstring(prompts[i]);
		packet_put_char(echo_on[i]);
	}
	packet_send();
	packet_write_wait();

	for (i = 0; i < kbdintctxt->nreq; i++)
		xfree(prompts[i]);
	xfree(prompts);
	xfree(echo_on);
	xfree(name);
	xfree(instr);
	return 1;
}

static void
input_userauth_info_response(int type, u_int32_t seq, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	KbdintAuthctxt *kbdintctxt;
	int authenticated = 0, res;
	u_int i, nresp;
	char **response = NULL, *method;

	if (authctxt == NULL)
		fatal("input_userauth_info_response: no authctxt");
	kbdintctxt = authctxt->kbdintctxt;
	if (kbdintctxt == NULL || kbdintctxt->ctxt == NULL)
		fatal("input_userauth_info_response: no kbdintctxt");
	if (kbdintctxt->device == NULL)
		fatal("input_userauth_info_response: no device");

	authctxt->postponed = 0;	/* reset */
	nresp = packet_get_int();
	if (nresp != kbdintctxt->nreq)
		fatal("input_userauth_info_response: wrong number of replies");
	if (nresp > 100)
		fatal("input_userauth_info_response: too many replies");
	if (nresp > 0) {
		response = xcalloc(nresp, sizeof(char *));
		for (i = 0; i < nresp; i++)
			response[i] = packet_get_string(NULL);
	}
	packet_check_eom();

	res = kbdintctxt->device->respond(kbdintctxt->ctxt, nresp, response);

	for (i = 0; i < nresp; i++) {
		memset(response[i], 'r', strlen(response[i]));
		xfree(response[i]);
	}
	if (response)
		xfree(response);

	switch (res) {
	case 0:
		/* Success! */
		authenticated = authctxt->valid ? 1 : 0;
		break;
	case 1:
		/* Authentication needs further interaction */
		if (send_userauth_info_request(authctxt) == 1)
			authctxt->postponed = 1;
		break;
	default:
		/* Failure! */
		break;
	}

	xasprintf(&method, "keyboard-interactive/%s", kbdintctxt->device->name);

	if (!authctxt->postponed) {
		if (authenticated) {
			auth2_challenge_stop(authctxt);
		} else {
			/* start next device */
			/* may set authctxt->postponed */
			auth2_challenge_start(authctxt);
		}
	}
	userauth_finish(authctxt, authenticated, method);
	xfree(method);
}

void
privsep_challenge_enable(void)
{
#if defined(BSD_AUTH) || defined(USE_PAM) || defined(SKEY)
	int n = 0;
#endif
#ifdef BSD_AUTH
	extern KbdintDevice mm_bsdauth_device;
#endif
#ifdef USE_PAM
	extern KbdintDevice mm_sshpam_device;
#endif
#ifdef SKEY
	extern KbdintDevice mm_skey_device;
#endif

#ifdef BSD_AUTH
	devices[n++] = &mm_bsdauth_device;
#else
#ifdef USE_PAM
	devices[n++] = &mm_sshpam_device;
#endif
#ifdef SKEY
	devices[n++] = &mm_skey_device;
#endif
#endif
}
