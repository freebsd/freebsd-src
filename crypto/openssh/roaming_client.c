/* $OpenBSD: roaming_client.c,v 1.9 2015/01/27 12:54:06 okan Exp $ */
/*
 * Copyright (c) 2004-2009 AppGate Network Security AB
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include "openbsd-compat/sys-queue.h"
#include <sys/types.h>
#include <sys/socket.h>

#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "buffer.h"
#include "channels.h"
#include "cipher.h"
#include "dispatch.h"
#include "clientloop.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "packet.h"
#include "ssh.h"
#include "key.h"
#include "kex.h"
#include "readconf.h"
#include "roaming.h"
#include "ssh2.h"
#include "sshconnect.h"
#include "digest.h"

/* import */
extern Options options;
extern char *host;
extern struct sockaddr_storage hostaddr;
extern int session_resumed;

static u_int32_t roaming_id;
static u_int64_t cookie;
static u_int64_t lastseenchall;
static u_int64_t key1, key2, oldkey1, oldkey2;

void
roaming_reply(int type, u_int32_t seq, void *ctxt)
{
	if (type == SSH2_MSG_REQUEST_FAILURE) {
		logit("Server denied roaming");
		return;
	}
	verbose("Roaming enabled");
	roaming_id = packet_get_int();
	cookie = packet_get_int64();
	key1 = oldkey1 = packet_get_int64();
	key2 = oldkey2 = packet_get_int64();
	set_out_buffer_size(packet_get_int() + get_snd_buf_size());
	roaming_enabled = 1;
}

void
request_roaming(void)
{
	packet_start(SSH2_MSG_GLOBAL_REQUEST);
	packet_put_cstring(ROAMING_REQUEST);
	packet_put_char(1);
	packet_put_int(get_recv_buf_size());
	packet_send();
	client_register_global_confirm(roaming_reply, NULL);
}

static void
roaming_auth_required(void)
{
	u_char digest[SSH_DIGEST_MAX_LENGTH];
	Buffer b;
	u_int64_t chall, oldchall;

	chall = packet_get_int64();
	oldchall = packet_get_int64();
	if (oldchall != lastseenchall) {
		key1 = oldkey1;
		key2 = oldkey2;
	}
	lastseenchall = chall;

	buffer_init(&b);
	buffer_put_int64(&b, cookie);
	buffer_put_int64(&b, chall);
	if (ssh_digest_buffer(SSH_DIGEST_SHA1, &b, digest, sizeof(digest)) != 0)
		fatal("%s: ssh_digest_buffer failed", __func__);
	buffer_free(&b);

	packet_start(SSH2_MSG_KEX_ROAMING_AUTH);
	packet_put_int64(key1 ^ get_recv_bytes());
	packet_put_raw(digest, ssh_digest_bytes(SSH_DIGEST_SHA1));
	packet_send();

	oldkey1 = key1;
	oldkey2 = key2;
	calculate_new_key(&key1, cookie, chall);
	calculate_new_key(&key2, cookie, chall);

	debug("Received %llu bytes", (unsigned long long)get_recv_bytes());
	debug("Sent roaming_auth packet");
}

int
resume_kex(void)
{
	/*
	 * This should not happen - if the client sends the kex method
	 * resume@appgate.com then the kex is done in roaming_resume().
	 */
	return 1;
}

static int
roaming_resume(void)
{
	u_int64_t recv_bytes;
	char *str = NULL, *kexlist = NULL, *c;
	int i, type;
	int timeout_ms = options.connection_timeout * 1000;
	u_int len;
	u_int32_t rnd = 0;

	resume_in_progress = 1;

	/* Exchange banners */
	ssh_exchange_identification(timeout_ms);
	packet_set_nonblocking();

	/* Send a kexinit message with resume@appgate.com as only kex algo */
	packet_start(SSH2_MSG_KEXINIT);
	for (i = 0; i < KEX_COOKIE_LEN; i++) {
		if (i % 4 == 0)
			rnd = arc4random();
		packet_put_char(rnd & 0xff);
		rnd >>= 8;
	}
	packet_put_cstring(KEX_RESUME);
	for (i = 1; i < PROPOSAL_MAX; i++) {
		/* kex algorithm added so start with i=1 and not 0 */
		packet_put_cstring(""); /* Not used when we resume */
	}
	packet_put_char(1); /* first kex_packet follows */
	packet_put_int(0); /* reserved */
	packet_send();

	/* Assume that resume@appgate.com will be accepted */
	packet_start(SSH2_MSG_KEX_ROAMING_RESUME);
	packet_put_int(roaming_id);
	packet_send();

	/* Read the server's kexinit and check for resume@appgate.com */
	if ((type = packet_read()) != SSH2_MSG_KEXINIT) {
		debug("expected kexinit on resume, got %d", type);
		goto fail;
	}
	for (i = 0; i < KEX_COOKIE_LEN; i++)
		(void)packet_get_char();
	kexlist = packet_get_string(&len);
	if (!kexlist
	    || (str = match_list(KEX_RESUME, kexlist, NULL)) == NULL) {
		debug("server doesn't allow resume");
		goto fail;
	}
	free(str);
	for (i = 1; i < PROPOSAL_MAX; i++) {
		/* kex algorithm taken care of so start with i=1 and not 0 */
		free(packet_get_string(&len));
	}
	i = packet_get_char(); /* first_kex_packet_follows */
	if (i && (c = strchr(kexlist, ',')))
		*c = 0;
	if (i && strcmp(kexlist, KEX_RESUME)) {
		debug("server's kex guess (%s) was wrong, skipping", kexlist);
		(void)packet_read(); /* Wrong guess - discard packet */
	}

	/*
	 * Read the ROAMING_AUTH_REQUIRED challenge from the server and
	 * send ROAMING_AUTH
	 */
	if ((type = packet_read()) != SSH2_MSG_KEX_ROAMING_AUTH_REQUIRED) {
		debug("expected roaming_auth_required, got %d", type);
		goto fail;
	}
	roaming_auth_required();

	/* Read ROAMING_AUTH_OK from the server */
	if ((type = packet_read()) != SSH2_MSG_KEX_ROAMING_AUTH_OK) {
		debug("expected roaming_auth_ok, got %d", type);
		goto fail;
	}
	recv_bytes = packet_get_int64() ^ oldkey2;
	debug("Peer received %llu bytes", (unsigned long long)recv_bytes);
	resend_bytes(packet_get_connection_out(), &recv_bytes);

	resume_in_progress = 0;

	session_resumed = 1; /* Tell clientloop */

	return 0;

fail:
	free(kexlist);
	if (packet_get_connection_in() == packet_get_connection_out())
		close(packet_get_connection_in());
	else {
		close(packet_get_connection_in());
		close(packet_get_connection_out());
	}
	return 1;
}

int
wait_for_roaming_reconnect(void)
{
	static int reenter_guard = 0;
	int timeout_ms = options.connection_timeout * 1000;
	int c;

	if (reenter_guard != 0)
		fatal("Server refused resume, roaming timeout may be exceeded");
	reenter_guard = 1;

	fprintf(stderr, "[connection suspended, press return to resume]");
	fflush(stderr);
	packet_backup_state();
	/* TODO Perhaps we should read from tty here */
	while ((c = fgetc(stdin)) != EOF) {
		if (c == 'Z' - 64) {
			kill(getpid(), SIGTSTP);
			continue;
		}
		if (c != '\n' && c != '\r')
			continue;

		if (ssh_connect(host, NULL, &hostaddr, options.port,
		    options.address_family, 1, &timeout_ms,
		    options.tcp_keep_alive, options.use_privileged_port) == 0 &&
		    roaming_resume() == 0) {
			packet_restore_state();
			reenter_guard = 0;
			fprintf(stderr, "[connection resumed]\n");
			fflush(stderr);
			return 0;
		}

		fprintf(stderr, "[reconnect failed, press return to retry]");
		fflush(stderr);
	}
	fprintf(stderr, "[exiting]\n");
	fflush(stderr);
	exit(0);
}
