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
RCSID("$OpenBSD: sshconnect2.c,v 1.18 2000/09/07 20:27:55 deraadt Exp $");

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
#include "authfd.h"

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
ssh_kex_dh(Kex *kex, char *host, struct sockaddr *hostaddr,
    Buffer *client_kexinit, Buffer *server_kexinit)
{
	int plen, dlen;
	unsigned int klen, kout;
	char *signature = NULL;
	unsigned int slen;
	char *server_host_key_blob = NULL;
	Key *server_host_key;
	unsigned int sbloblen;
	DH *dh;
	BIGNUM *dh_server_pub = 0;
	BIGNUM *shared_secret = 0;
	unsigned char *kbuf;
	unsigned char *hash;

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

	packet_read_expect(&plen, SSH2_MSG_KEXDH_REPLY);

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
	DH_free(dh);
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

	/* save session id */
	session_id2_len = 20;
	session_id2 = xmalloc(session_id2_len);
	memcpy(session_id2, hash, session_id2_len);
}

void
ssh_kex2(char *host, struct sockaddr *hostaddr)
{
	int i, plen;
	Kex *kex;
	Buffer *client_kexinit, *server_kexinit;
	char *sprop[PROPOSAL_MAX];

	if (options.ciphers != NULL) {
		myproposal[PROPOSAL_ENC_ALGS_CTOS] =
		myproposal[PROPOSAL_ENC_ALGS_STOC] = options.ciphers;
	} else if (options.cipher == SSH_CIPHER_3DES) {
		myproposal[PROPOSAL_ENC_ALGS_CTOS] =
		myproposal[PROPOSAL_ENC_ALGS_STOC] =
		    (char *) cipher_name(SSH_CIPHER_3DES_CBC);
	} else if (options.cipher == SSH_CIPHER_BLOWFISH) {
		myproposal[PROPOSAL_ENC_ALGS_CTOS] =
		myproposal[PROPOSAL_ENC_ALGS_STOC] =
		    (char *) cipher_name(SSH_CIPHER_BLOWFISH_CBC);
	}
	if (options.compression) {
		myproposal[PROPOSAL_COMP_ALGS_CTOS] = "zlib";
		myproposal[PROPOSAL_COMP_ALGS_STOC] = "zlib";
	} else {
		myproposal[PROPOSAL_COMP_ALGS_CTOS] = "none";
		myproposal[PROPOSAL_COMP_ALGS_STOC] = "none";
	}

	/* buffers with raw kexinit messages */
	server_kexinit = xmalloc(sizeof(*server_kexinit));
	buffer_init(server_kexinit);
	client_kexinit = kex_init(myproposal);

	/* algorithm negotiation */
	kex_exchange_kexinit(client_kexinit, server_kexinit, sprop);
	kex = kex_choose_conf(myproposal, sprop, 0);
	for (i = 0; i < PROPOSAL_MAX; i++)
		xfree(sprop[i]);

	/* server authentication and session key agreement */
	ssh_kex_dh(kex, host, hostaddr, client_kexinit, server_kexinit);

	buffer_free(client_kexinit);
	buffer_free(server_kexinit);
	xfree(client_kexinit);
	xfree(server_kexinit);

	debug("Wait SSH2_MSG_NEWKEYS.");
	packet_read_expect(&plen, SSH2_MSG_NEWKEYS);
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

	if (attempt++ >= options.number_of_password_prompts)
		return 0;

	if(attempt != 1)
		error("Permission denied, please try again.");

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

typedef int sign_fn(
    Key *key,
    unsigned char **sigp, int *lenp,
    unsigned char *data, int datalen);

int
ssh2_sign_and_send_pubkey(Key *k, sign_fn *do_sign,
    const char *server_user, const char *host, const char *service)
{
	Buffer b;
	unsigned char *blob, *signature;
	int bloblen, slen;
	int skip = 0;
	int ret = -1;

	dsa_make_key_blob(k, &blob, &bloblen);

	/* data to be signed */
	buffer_init(&b);
	if (datafellows & SSH_COMPAT_SESSIONID_ENCODING) {
		buffer_put_string(&b, session_id2, session_id2_len);
		skip = buffer_len(&b);
	} else {
		buffer_append(&b, session_id2, session_id2_len);
		skip = session_id2_len; 
	}
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
	ret = do_sign(k, &signature, &slen, buffer_ptr(&b), buffer_len(&b));
	if (ret == -1) {
		xfree(blob);
		buffer_free(&b);
		return 0;
	}
#ifdef DEBUG_DSS
	buffer_dump(&b);
#endif
	if (datafellows & SSH_BUG_PUBKEYAUTH) {
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
	if (buffer_len(&b) < skip + 1)
		fatal("ssh2_try_pubkey: internal error");
	buffer_consume(&b, skip + 1);

	/* put remaining data from buffer into packet */
	packet_start(SSH2_MSG_USERAUTH_REQUEST);
	packet_put_raw(buffer_ptr(&b), buffer_len(&b));
	buffer_free(&b);

	/* send */
	packet_send();
	packet_write_wait();

	return 1;
}

int
ssh2_try_pubkey(char *filename,
    const char *server_user, const char *host, const char *service)
{
	Key *k;
	int ret = 0;
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
		if (!success) {
			key_free(k);
			return 0;
		}
	}
	ret = ssh2_sign_and_send_pubkey(k, dsa_sign, server_user, host, service);
	key_free(k);
	return ret;
}

int agent_sign(
    Key *key,
    unsigned char **sigp, int *lenp,
    unsigned char *data, int datalen)
{
	int ret = -1;
	AuthenticationConnection *ac = ssh_get_authentication_connection();
	if (ac != NULL) {
		ret = ssh_agent_sign(ac, key, sigp, lenp, data, datalen);
		ssh_close_authentication_connection(ac);
	}
	return ret;
}

int
ssh2_try_agent(AuthenticationConnection *ac,
    const char *server_user, const char *host, const char *service)
{
	static int called = 0;
	char *comment;
	Key *k;
	int ret;

	if (called == 0) {
		k = ssh_get_first_identity(ac, &comment, 2);
		called ++;
	} else {
		k = ssh_get_next_identity(ac, &comment, 2);
	}
	if (k == NULL)
		return 0;
	debug("trying DSA agent key %s", comment);
	xfree(comment);
	ret = ssh2_sign_and_send_pubkey(k, agent_sign, server_user, host, service);
	key_free(k);
	return ret;
}

void
ssh_userauth2(const char *server_user, char *host)
{
	AuthenticationConnection *ac = ssh_get_authentication_connection();
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
			if (ac != NULL)
				sent = ssh2_try_agent(ac,
				    server_user, host, service);
			if (!sent) {
				while (i < options.num_identity_files2) {
					sent = ssh2_try_pubkey(
					    options.identity_files2[i++],
					    server_user, host, service);
					if (sent)
						break;
				}
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
	if (ac != NULL)
		ssh_close_authentication_connection(ac);
	packet_done();
	debug("ssh-userauth2 successfull");
}
