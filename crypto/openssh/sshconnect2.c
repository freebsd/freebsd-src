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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
RCSID("$OpenBSD: sshconnect2.c,v 1.10 2000/05/08 17:42:25 markus Exp $");

#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/md5.h>
#include <openssl/dh.h>
#include <openssl/hmac.h>

#include "ssh.h"
#include "xmalloc.h"
#include "rsa.h"
#include "buffer.h"
#include "packet.h"
#include "cipher.h"
#include "uidswap.h"
#include "compat.h"
#include "readconf.h"
#include "bufaux.h"
#include "ssh2.h"
#include "kex.h"
#include "myproposal.h"
#include "key.h"
#include "dsa.h"
#include "sshconnect.h"
#include "authfile.h"

/* import */
extern char *client_version_string;
extern char *server_version_string;
extern Options options;

/*
 * SSH2 key exchange
 */

unsigned char *session_id2 = NULL;
int session_id2_len = 0;

void
ssh_kex2(char *host, struct sockaddr *hostaddr)
{
	Kex *kex;
	char *cprop[PROPOSAL_MAX];
	char *sprop[PROPOSAL_MAX];
	Buffer *client_kexinit;
	Buffer *server_kexinit;
	int payload_len, dlen;
	unsigned int klen, kout;
	char *ptr;
	char *signature = NULL;
	unsigned int slen;
	char *server_host_key_blob = NULL;
	Key *server_host_key;
	unsigned int sbloblen;
	DH *dh;
	BIGNUM *dh_server_pub = 0;
	BIGNUM *shared_secret = 0;
	int i;
	unsigned char *kbuf;
	unsigned char *hash;

/* KEXINIT */

	debug("Sending KEX init.");
	if (options.ciphers != NULL) {
		myproposal[PROPOSAL_ENC_ALGS_CTOS] =
		myproposal[PROPOSAL_ENC_ALGS_STOC] = options.ciphers;
	} else if (options.cipher == SSH_CIPHER_3DES) {
		myproposal[PROPOSAL_ENC_ALGS_CTOS] =
		myproposal[PROPOSAL_ENC_ALGS_STOC] =
		    cipher_name(SSH_CIPHER_3DES_CBC);
	} else if (options.cipher == SSH_CIPHER_BLOWFISH) {
		myproposal[PROPOSAL_ENC_ALGS_CTOS] =
		myproposal[PROPOSAL_ENC_ALGS_STOC] =
		    cipher_name(SSH_CIPHER_BLOWFISH_CBC);
	}
	if (options.compression) {
		myproposal[PROPOSAL_COMP_ALGS_CTOS] = "zlib";
		myproposal[PROPOSAL_COMP_ALGS_STOC] = "zlib";
	} else {
		myproposal[PROPOSAL_COMP_ALGS_CTOS] = "none";
		myproposal[PROPOSAL_COMP_ALGS_STOC] = "none";
	}
	for (i = 0; i < PROPOSAL_MAX; i++)
		cprop[i] = xstrdup(myproposal[i]);

	client_kexinit = kex_init(cprop);
	packet_start(SSH2_MSG_KEXINIT);
	packet_put_raw(buffer_ptr(client_kexinit), buffer_len(client_kexinit));	
	packet_send();
	packet_write_wait();

	debug("done");

	packet_read_expect(&payload_len, SSH2_MSG_KEXINIT);

	/* save payload for session_id */
	server_kexinit = xmalloc(sizeof(*server_kexinit));
	buffer_init(server_kexinit);
	ptr = packet_get_raw(&payload_len);
	buffer_append(server_kexinit, ptr, payload_len);

	/* skip cookie */
	for (i = 0; i < 16; i++)
		(void) packet_get_char();
	/* kex init proposal strings */
	for (i = 0; i < PROPOSAL_MAX; i++) {
		sprop[i] = packet_get_string(NULL);
		debug("got kexinit string: %s", sprop[i]);
	}
	i = (int) packet_get_char();
	debug("first kex follow == %d", i);
	i = packet_get_int();
	debug("reserved == %d", i);
	packet_done();

	debug("done read kexinit");
	kex = kex_choose_conf(cprop, sprop, 0);

/* KEXDH */

	debug("Sending SSH2_MSG_KEXDH_INIT.");

	/* generate and send 'e', client DH public key */
	dh = dh_new_group1();
	packet_start(SSH2_MSG_KEXDH_INIT);
	packet_put_bignum2(dh->pub_key);
	packet_send();
	packet_write_wait();

#ifdef DEBUG_KEXDH
	fprintf(stderr, "\np= ");
	bignum_print(dh->p);
	fprintf(stderr, "\ng= ");
	bignum_print(dh->g);
	fprintf(stderr, "\npub= ");
	bignum_print(dh->pub_key);
	fprintf(stderr, "\n");
	DHparams_print_fp(stderr, dh);
#endif

	debug("Wait SSH2_MSG_KEXDH_REPLY.");

	packet_read_expect(&payload_len, SSH2_MSG_KEXDH_REPLY);

	debug("Got SSH2_MSG_KEXDH_REPLY.");

	/* key, cert */
	server_host_key_blob = packet_get_string(&sbloblen);
	server_host_key = dsa_key_from_blob(server_host_key_blob, sbloblen);
	if (server_host_key == NULL)
		fatal("cannot decode server_host_key_blob");

	check_host_key(host, hostaddr, server_host_key,
	    options.user_hostfile2, options.system_hostfile2);

	/* DH paramter f, server public DH key */
	dh_server_pub = BN_new();
	if (dh_server_pub == NULL)
		fatal("dh_server_pub == NULL");
	packet_get_bignum2(dh_server_pub, &dlen);

#ifdef DEBUG_KEXDH
	fprintf(stderr, "\ndh_server_pub= ");
	bignum_print(dh_server_pub);
	fprintf(stderr, "\n");
	debug("bits %d", BN_num_bits(dh_server_pub));
#endif

	/* signed H */
	signature = packet_get_string(&slen);
	packet_done();

	if (!dh_pub_is_valid(dh, dh_server_pub))
		packet_disconnect("bad server public DH value");

	klen = DH_size(dh);
	kbuf = xmalloc(klen);
	kout = DH_compute_key(kbuf, dh_server_pub, dh);
#ifdef DEBUG_KEXDH
	debug("shared secret: len %d/%d", klen, kout);
	fprintf(stderr, "shared secret == ");
	for (i = 0; i< kout; i++)
		fprintf(stderr, "%02x", (kbuf[i])&0xff);
	fprintf(stderr, "\n");
#endif
	shared_secret = BN_new();

	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	/* calc and verify H */
	hash = kex_hash(
	    client_version_string,
	    server_version_string,
	    buffer_ptr(client_kexinit), buffer_len(client_kexinit),
	    buffer_ptr(server_kexinit), buffer_len(server_kexinit),
	    server_host_key_blob, sbloblen,
	    dh->pub_key,
	    dh_server_pub,
	    shared_secret
	);
	xfree(server_host_key_blob);
	buffer_free(client_kexinit);
	buffer_free(server_kexinit);
	xfree(client_kexinit);
	xfree(server_kexinit);
#ifdef DEBUG_KEXDH
	fprintf(stderr, "hash == ");
	for (i = 0; i< 20; i++)
		fprintf(stderr, "%02x", (hash[i])&0xff);
	fprintf(stderr, "\n");
#endif
	if (dsa_verify(server_host_key, (unsigned char *)signature, slen, hash, 20) != 1)
		fatal("dsa_verify failed for server_host_key");
	key_free(server_host_key);

	kex_derive_keys(kex, hash, shared_secret);
	packet_set_kex(kex);

	/* have keys, free DH */
	DH_free(dh);

	/* save session id */
	session_id2_len = 20;
	session_id2 = xmalloc(session_id2_len);
	memcpy(session_id2, hash, session_id2_len);

	debug("Wait SSH2_MSG_NEWKEYS.");
	packet_read_expect(&payload_len, SSH2_MSG_NEWKEYS);
	packet_done();
	debug("GOT SSH2_MSG_NEWKEYS.");

	debug("send SSH2_MSG_NEWKEYS.");
	packet_start(SSH2_MSG_NEWKEYS);
	packet_send();
	packet_write_wait();
	debug("done: send SSH2_MSG_NEWKEYS.");

#ifdef DEBUG_KEXDH
	/* send 1st encrypted/maced/compressed message */
	packet_start(SSH2_MSG_IGNORE);
	packet_put_cstring("markus");
	packet_send();
	packet_write_wait();
#endif
	debug("done: KEX2.");
}
/*
 * Authenticate user
 */
int
ssh2_try_passwd(const char *server_user, const char *host, const char *service)
{
	static int attempt = 0;
	char prompt[80];
	char *password;

	if (attempt++ > options.number_of_password_prompts)
		return 0;

	snprintf(prompt, sizeof(prompt), "%.30s@%.40s's password: ",
	    server_user, host);
	password = read_passphrase(prompt, 0);
	packet_start(SSH2_MSG_USERAUTH_REQUEST);
	packet_put_cstring(server_user);
	packet_put_cstring(service);
	packet_put_cstring("password");
	packet_put_char(0);
	packet_put_cstring(password);
	memset(password, 0, strlen(password));
	xfree(password);
	packet_send();
	packet_write_wait();
	return 1;
}

int
ssh2_try_pubkey(char *filename,
    const char *server_user, const char *host, const char *service)
{
	Buffer b;
	Key *k;
	unsigned char *blob, *signature;
	int bloblen, slen;
	struct stat st;

	if (stat(filename, &st) != 0) {
		debug("key does not exist: %s", filename);
		return 0;
	}
	debug("try pubkey: %s", filename);

	k = key_new(KEY_DSA);
	if (!load_private_key(filename, "", k, NULL)) {
		int success = 0;
		char *passphrase;
		char prompt[300];
		snprintf(prompt, sizeof prompt,
		     "Enter passphrase for DSA key '%.100s': ",
		     filename);
		passphrase = read_passphrase(prompt, 0);
		success = load_private_key(filename, passphrase, k, NULL);
		memset(passphrase, 0, strlen(passphrase));
		xfree(passphrase);
		if (!success)
			return 0;
	}
	dsa_make_key_blob(k, &blob, &bloblen);

	/* data to be signed */
	buffer_init(&b);
	buffer_append(&b, session_id2, session_id2_len);
	buffer_put_char(&b, SSH2_MSG_USERAUTH_REQUEST);
	buffer_put_cstring(&b, server_user);
	buffer_put_cstring(&b,
	    datafellows & SSH_BUG_PUBKEYAUTH ?
	    "ssh-userauth" :
	    service);
	buffer_put_cstring(&b, "publickey");
	buffer_put_char(&b, 1);
	buffer_put_cstring(&b, KEX_DSS); 
	buffer_put_string(&b, blob, bloblen);

	/* generate signature */
	dsa_sign(k, &signature, &slen, buffer_ptr(&b), buffer_len(&b));
	key_free(k);
#ifdef DEBUG_DSS
	buffer_dump(&b);
#endif
	if (datafellows & SSH_BUG_PUBKEYAUTH) {
		/* e.g. ssh-2.0.13: data-to-be-signed != data-on-the-wire */
		buffer_clear(&b);
		buffer_append(&b, session_id2, session_id2_len);
		buffer_put_char(&b, SSH2_MSG_USERAUTH_REQUEST);
		buffer_put_cstring(&b, server_user);
		buffer_put_cstring(&b, service);
		buffer_put_cstring(&b, "publickey");
		buffer_put_char(&b, 1);
		buffer_put_cstring(&b, KEX_DSS); 
		buffer_put_string(&b, blob, bloblen);
	}
	xfree(blob);
	/* append signature */
	buffer_put_string(&b, signature, slen);
	xfree(signature);

	/* skip session id and packet type */
	if (buffer_len(&b) < session_id2_len + 1)
		fatal("ssh2_try_pubkey: internal error");
	buffer_consume(&b, session_id2_len + 1);

	/* put remaining data from buffer into packet */
	packet_start(SSH2_MSG_USERAUTH_REQUEST);
	packet_put_raw(buffer_ptr(&b), buffer_len(&b));
	buffer_free(&b);

	/* send */
	packet_send();
	packet_write_wait();
	return 1;
}

void
ssh_userauth2(const char *server_user, char *host)
{
	int type;
	int plen;
	int sent;
	unsigned int dlen;
	int partial;
	int i = 0;
	char *auths;
	char *service = "ssh-connection";		/* service name */

	debug("send SSH2_MSG_SERVICE_REQUEST");
	packet_start(SSH2_MSG_SERVICE_REQUEST);
	packet_put_cstring("ssh-userauth");
	packet_send();
	packet_write_wait();

	type = packet_read(&plen);
	if (type != SSH2_MSG_SERVICE_ACCEPT) {
		fatal("denied SSH2_MSG_SERVICE_ACCEPT: %d", type);
	}
	if (packet_remaining() > 0) {
		char *reply = packet_get_string(&plen);
		debug("service_accept: %s", reply);
		xfree(reply);
	} else {
		/* payload empty for ssh-2.0.13 ?? */
		debug("buggy server: service_accept w/o service");
	}
	packet_done();
	debug("got SSH2_MSG_SERVICE_ACCEPT");

	/* INITIAL request for auth */
	packet_start(SSH2_MSG_USERAUTH_REQUEST);
	packet_put_cstring(server_user);
	packet_put_cstring(service);
	packet_put_cstring("none");
	packet_send();
	packet_write_wait();

	for (;;) {
		sent = 0;
		type = packet_read(&plen);
		if (type == SSH2_MSG_USERAUTH_SUCCESS)
			break;
		if (type != SSH2_MSG_USERAUTH_FAILURE)
			fatal("access denied: %d", type);
		/* SSH2_MSG_USERAUTH_FAILURE means: try again */
		auths = packet_get_string(&dlen);
		debug("authentications that can continue: %s", auths);
		partial = packet_get_char();
		packet_done();
		if (partial)
			debug("partial success");
		if (options.dsa_authentication &&
		    strstr(auths, "publickey") != NULL) {
			while (i < options.num_identity_files2) {
				sent = ssh2_try_pubkey(
				    options.identity_files2[i++],
				    server_user, host, service);
				if (sent)
					break;
			}
		}
		if (!sent) {
			if (options.password_authentication &&
			    !options.batch_mode &&
			    strstr(auths, "password") != NULL) {
				sent = ssh2_try_passwd(server_user, host, service);
			}
		}
		if (!sent)
			fatal("Permission denied (%s).", auths);
		xfree(auths);
	}
	packet_done();
	debug("ssh-userauth2 successfull");
}
