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
RCSID("$FreeBSD$");
RCSID("$OpenBSD: sshconnect2.c,v 1.27 2000/10/19 16:45:16 provos Exp $");

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
#include "cli.h"
#include "dispatch.h"
#include "authfd.h"

void ssh_dh1_client(Kex *, char *, struct sockaddr *, Buffer *, Buffer *);
void ssh_dhgex_client(Kex *, char *, struct sockaddr *, Buffer *, Buffer *);

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
	int i, plen;
	Kex *kex;
	Buffer *client_kexinit, *server_kexinit;
	char *sprop[PROPOSAL_MAX];

	if (options.ciphers == NULL) {
		if (options.cipher == SSH_CIPHER_3DES) {
			options.ciphers = "3des-cbc";
		} else if (options.cipher == SSH_CIPHER_BLOWFISH) {
			options.ciphers = "blowfish-cbc";
		} else if (options.cipher == SSH_CIPHER_DES) {
			fatal("cipher DES not supported for protocol version 2");
		}
	}
	if (options.ciphers != NULL) {
		myproposal[PROPOSAL_ENC_ALGS_CTOS] =
		myproposal[PROPOSAL_ENC_ALGS_STOC] = options.ciphers;
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
	switch(kex->kex_type) {
	case DH_GRP1_SHA1:
		ssh_dh1_client(kex, host, hostaddr,
			       client_kexinit, server_kexinit);
		break;
	case DH_GEX_SHA1:
		ssh_dhgex_client(kex, host, hostaddr, client_kexinit,
				 server_kexinit);
		break;
	default:
		fatal("Unsupported key exchange %d", kex->kex_type);
	}

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

/* diffie-hellman-group1-sha1 */

void
ssh_dh1_client(Kex *kex, char *host, struct sockaddr *hostaddr, 
	       Buffer *client_kexinit, Buffer *server_kexinit)
{
#ifdef DEBUG_KEXDH
	int i;
#endif
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
	BN_print_fp(stderr, dh->p);
	fprintf(stderr, "\ng= ");
	BN_print_fp(stderr, dh->g);
	fprintf(stderr, "\npub= ");
	BN_print_fp(stderr, dh->pub_key);
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
	BN_print_fp(stderr, dh_server_pub);
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

/* diffie-hellman-group-exchange-sha1 */

/*
 * Estimates the group order for a Diffie-Hellman group that has an
 * attack complexity approximately the same as O(2**bits).  Estimate
 * with:  O(exp(1.9223 * (ln q)^(1/3) (ln ln q)^(2/3)))
 */

int
dh_estimate(int bits)
{
	
	if (bits < 64)
		return (512);	/* O(2**63) */
	if (bits < 128)
		return (1024);	/* O(2**86) */
	if (bits < 192)
		return (2048);	/* O(2**116) */
	return (4096);		/* O(2**156) */
}

void
ssh_dhgex_client(Kex *kex, char *host, struct sockaddr *hostaddr,
		 Buffer *client_kexinit, Buffer *server_kexinit)
{
#ifdef DEBUG_KEXDH
	int i;
#endif
	int plen, dlen;
	unsigned int klen, kout;
	char *signature = NULL;
	unsigned int slen, nbits;
	char *server_host_key_blob = NULL;
	Key *server_host_key;
	unsigned int sbloblen;
	DH *dh;
	BIGNUM *dh_server_pub = 0;
	BIGNUM *shared_secret = 0;
	BIGNUM *p = 0, *g = 0;
	unsigned char *kbuf;
	unsigned char *hash;

	nbits = dh_estimate(kex->enc[MODE_OUT].cipher->key_len * 8);

	debug("Sending SSH2_MSG_KEX_DH_GEX_REQUEST.");
	packet_start(SSH2_MSG_KEX_DH_GEX_REQUEST);
	packet_put_int(nbits);
	packet_send();
	packet_write_wait();

#ifdef DEBUG_KEXDH
	fprintf(stderr, "\nnbits = %d", nbits);
#endif

	debug("Wait SSH2_MSG_KEX_DH_GEX_GROUP.");

	packet_read_expect(&plen, SSH2_MSG_KEX_DH_GEX_GROUP);

	debug("Got SSH2_MSG_KEX_DH_GEX_GROUP.");

	if ((p = BN_new()) == NULL)
		fatal("BN_new");
	packet_get_bignum2(p, &dlen);
	if ((g = BN_new()) == NULL)
		fatal("BN_new");
	packet_get_bignum2(g, &dlen);
	if ((dh = dh_new_group(g, p)) == NULL)
		fatal("dh_new_group");

#ifdef DEBUG_KEXDH
	fprintf(stderr, "\np= ");
	BN_print_fp(stderr, dh->p);
	fprintf(stderr, "\ng= ");
	BN_print_fp(stderr, dh->g);
	fprintf(stderr, "\npub= ");
	BN_print_fp(stderr, dh->pub_key);
	fprintf(stderr, "\n");
	DHparams_print_fp(stderr, dh);
#endif

	debug("Sending SSH2_MSG_KEX_DH_GEX_INIT.");
	/* generate and send 'e', client DH public key */
	packet_start(SSH2_MSG_KEX_DH_GEX_INIT);
	packet_put_bignum2(dh->pub_key);
	packet_send();
	packet_write_wait();

	debug("Wait SSH2_MSG_KEX_DH_GEX_REPLY.");

	packet_read_expect(&plen, SSH2_MSG_KEX_DH_GEX_REPLY);

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
	BN_print_fp(stderr, dh_server_pub);
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
	hash = kex_hash_gex(
	    client_version_string,
	    server_version_string,
	    buffer_ptr(client_kexinit), buffer_len(client_kexinit),
	    buffer_ptr(server_kexinit), buffer_len(server_kexinit),
	    server_host_key_blob, sbloblen,
	    nbits, dh->p, dh->g, 
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

/*
 * Authenticate user
 */

typedef struct Authctxt Authctxt;
typedef struct Authmethod Authmethod;

typedef int sign_cb_fn(
    Authctxt *authctxt, Key *key,
    unsigned char **sigp, int *lenp, unsigned char *data, int datalen);

struct Authctxt {
	const char *server_user;
	const char *host;
	const char *service;
	AuthenticationConnection *agent;
	Authmethod *method;
	int success;
};
struct Authmethod {
	char	*name;		/* string to compare against server's list */
	int	(*userauth)(Authctxt *authctxt);
	int	*enabled;	/* flag in option struct that enables method */
	int	*batch_flag;	/* flag in option struct that disables method */
};

void	input_userauth_success(int type, int plen, void *ctxt);
void	input_userauth_failure(int type, int plen, void *ctxt);
void	input_userauth_error(int type, int plen, void *ctxt);
void	input_userauth_info_req(int type, int plen, void *ctxt);

int	userauth_none(Authctxt *authctxt);
int	userauth_pubkey(Authctxt *authctxt);
int	userauth_passwd(Authctxt *authctxt);
int	userauth_kbdint(Authctxt *authctxt);

void	authmethod_clear();
Authmethod *authmethod_get(char *authlist);
Authmethod *authmethod_lookup(const char *name);

Authmethod authmethods[] = {
	{"publickey",
		userauth_pubkey,
		&options.dsa_authentication,
		NULL},
	{"password",
		userauth_passwd,
		&options.password_authentication,
		&options.batch_mode},
	{"keyboard-interactive",
		userauth_kbdint,
		&options.kbd_interactive_authentication,
		&options.batch_mode},
	{"none",
		userauth_none,
		NULL,
		NULL},
	{NULL, NULL, NULL, NULL}
};

void
ssh_userauth2(const char *server_user, char *host)
{
	Authctxt authctxt;
	int type;
	int plen;

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
		packet_done();
	} else {
		debug("buggy server: service_accept w/o service");
	}
	packet_done();
	debug("got SSH2_MSG_SERVICE_ACCEPT");

	/* setup authentication context */
	authctxt.agent = ssh_get_authentication_connection();
	authctxt.server_user = server_user;
	authctxt.host = host;
	authctxt.service = "ssh-connection";		/* service name */
	authctxt.success = 0;
	authctxt.method = authmethod_lookup("none");
	if (authctxt.method == NULL)
		fatal("ssh_userauth2: internal error: cannot send userauth none request");
	authmethod_clear();

	/* initial userauth request */
	userauth_none(&authctxt);

	dispatch_init(&input_userauth_error);
	dispatch_set(SSH2_MSG_USERAUTH_SUCCESS, &input_userauth_success);
	dispatch_set(SSH2_MSG_USERAUTH_FAILURE, &input_userauth_failure);
	dispatch_run(DISPATCH_BLOCK, &authctxt.success, &authctxt);	/* loop until success */

	if (authctxt.agent != NULL)
		ssh_close_authentication_connection(authctxt.agent);

	debug("ssh-userauth2 successfull: method %s", authctxt.method->name);
}
void
input_userauth_error(int type, int plen, void *ctxt)
{
	fatal("input_userauth_error: bad message during authentication");
}
void
input_userauth_success(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	if (authctxt == NULL)
		fatal("input_userauth_success: no authentication context");
	authctxt->success = 1;			/* break out */
}
void
input_userauth_failure(int type, int plen, void *ctxt)
{
	Authmethod *method = NULL;
	Authctxt *authctxt = ctxt;
	char *authlist = NULL;
	int partial;

	if (authctxt == NULL)
		fatal("input_userauth_failure: no authentication context");

	authlist = packet_get_string(NULL);
	partial = packet_get_char();
	packet_done();

	if (partial != 0)
		debug("partial success");
	debug("authentications that can continue: %s", authlist);

	for (;;) {
		method = authmethod_get(authlist);
		if (method == NULL)
                        fatal("Unable to find an authentication method");
		authctxt->method = method;
		if (method->userauth(authctxt) != 0) {
			debug2("we sent a %s packet, wait for reply", method->name);
			break;
		} else {
			debug2("we did not send a packet, disable method");
			method->enabled = NULL;
		}
	}	
	xfree(authlist);
}

int
userauth_none(Authctxt *authctxt)
{
	/* initial userauth request */
	packet_start(SSH2_MSG_USERAUTH_REQUEST);
	packet_put_cstring(authctxt->server_user);
	packet_put_cstring(authctxt->service);
	packet_put_cstring(authctxt->method->name);
	packet_send();
	packet_write_wait();
	return 1;
}

int
userauth_passwd(Authctxt *authctxt)
{
	static int attempt = 0;
	char prompt[80];
	char *password;

	if (attempt++ >= options.number_of_password_prompts)
		return 0;

	if(attempt != 1)
		error("Permission denied, please try again.");

	snprintf(prompt, sizeof(prompt), "%.30s@%.40s's password: ",
	    authctxt->server_user, authctxt->host);
	password = read_passphrase(prompt, 0);
	packet_start(SSH2_MSG_USERAUTH_REQUEST);
	packet_put_cstring(authctxt->server_user);
	packet_put_cstring(authctxt->service);
	packet_put_cstring(authctxt->method->name);
	packet_put_char(0);
	ssh_put_password(password);
	memset(password, 0, strlen(password));
	xfree(password);
	packet_send();
	packet_write_wait();
	return 1;
}

int
sign_and_send_pubkey(Authctxt *authctxt, Key *k, sign_cb_fn *sign_callback)
{
	Buffer b;
	unsigned char *blob, *signature;
	int bloblen, slen;
	int skip = 0;
	int ret = -1;
	int have_sig = 1;

	dsa_make_key_blob(k, &blob, &bloblen);

	/* data to be signed */
	buffer_init(&b);
	if (datafellows & SSH_OLD_SESSIONID) {
		buffer_append(&b, session_id2, session_id2_len);
		skip = session_id2_len; 
	} else {
		buffer_put_string(&b, session_id2, session_id2_len);
		skip = buffer_len(&b);
	}
	buffer_put_char(&b, SSH2_MSG_USERAUTH_REQUEST);
	buffer_put_cstring(&b, authctxt->server_user);
	buffer_put_cstring(&b,
	    datafellows & SSH_BUG_PUBKEYAUTH ?
	    "ssh-userauth" :
	    authctxt->service);
	buffer_put_cstring(&b, authctxt->method->name);
	buffer_put_char(&b, have_sig);
	buffer_put_cstring(&b, KEX_DSS); 
	buffer_put_string(&b, blob, bloblen);

	/* generate signature */
	ret = (*sign_callback)(authctxt, k, &signature, &slen, buffer_ptr(&b), buffer_len(&b));
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
		buffer_put_cstring(&b, authctxt->server_user);
		buffer_put_cstring(&b, authctxt->service);
		buffer_put_cstring(&b, authctxt->method->name);
		buffer_put_char(&b, have_sig);
		buffer_put_cstring(&b, KEX_DSS); 
		buffer_put_string(&b, blob, bloblen);
	}
	xfree(blob);
	/* append signature */
	buffer_put_string(&b, signature, slen);
	xfree(signature);

	/* skip session id and packet type */
	if (buffer_len(&b) < skip + 1)
		fatal("userauth_pubkey: internal error");
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

/* sign callback */
int dsa_sign_cb(Authctxt *authctxt, Key *key, unsigned char **sigp, int *lenp,
    unsigned char *data, int datalen)
{
	return dsa_sign(key, sigp, lenp, data, datalen);
}

int
userauth_pubkey_identity(Authctxt *authctxt, char *filename)
{
	Key *k;
	int i, ret, try_next;
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
		     "Enter passphrase for %s key '%.100s': ",
		     key_type(k), filename);
		for (i = 0; i < options.number_of_password_prompts; i++) {
			passphrase = read_passphrase(prompt, 0);
			if (strcmp(passphrase, "") != 0) {
				success = load_private_key(filename, passphrase, k, NULL);
				try_next = 0;
			} else {
				debug2("no passphrase given, try next key");
				try_next = 1;
			}
			memset(passphrase, 0, strlen(passphrase));
			xfree(passphrase);
			if (success || try_next)
				break;
			debug2("bad passphrase given, try again...");
		}
		if (!success) {
			key_free(k);
			return 0;
		}
	}
	ret = sign_and_send_pubkey(authctxt, k, dsa_sign_cb);
	key_free(k);
	return ret;
}

/* sign callback */
int agent_sign_cb(Authctxt *authctxt, Key *key, unsigned char **sigp, int *lenp,
    unsigned char *data, int datalen)
{
	return ssh_agent_sign(authctxt->agent, key, sigp, lenp, data, datalen);
}

int
userauth_pubkey_agent(Authctxt *authctxt)
{
	static int called = 0;
	char *comment;
	Key *k;
	int ret;

	if (called == 0) {
		k = ssh_get_first_identity(authctxt->agent, &comment, 2);
		called = 1;
	} else {
		k = ssh_get_next_identity(authctxt->agent, &comment, 2);
	}
	if (k == NULL) {
		debug2("no more DSA keys from agent");
		return 0;
	}
	debug("trying DSA agent key %s", comment);
	xfree(comment);
	ret = sign_and_send_pubkey(authctxt, k, agent_sign_cb);
	key_free(k);
	return ret;
}

int
userauth_pubkey(Authctxt *authctxt)
{
	static int idx = 0;
	int sent = 0;

	if (authctxt->agent != NULL)
		sent = userauth_pubkey_agent(authctxt);
	while (sent == 0 && idx < options.num_identity_files2)
		sent = userauth_pubkey_identity(authctxt, options.identity_files2[idx++]);
	return sent;
}

/*
 * Send userauth request message specifying keyboard-interactive method.
 */
int
userauth_kbdint(Authctxt *authctxt)
{
	static int attempt = 0;

	if (attempt++ >= options.number_of_password_prompts)
		return 0;

	debug2("userauth_kbdint");
	packet_start(SSH2_MSG_USERAUTH_REQUEST);
	packet_put_cstring(authctxt->server_user);
	packet_put_cstring(authctxt->service);
	packet_put_cstring(authctxt->method->name);
	packet_put_cstring("");					/* lang */
	packet_put_cstring(options.kbd_interactive_devices ?
	    options.kbd_interactive_devices : "");
	packet_send();
	packet_write_wait();

	dispatch_set(SSH2_MSG_USERAUTH_INFO_REQUEST, &input_userauth_info_req);
	return 1;
}

/*
 * parse SSH2_MSG_USERAUTH_INFO_REQUEST, prompt user and send
 * SSH2_MSG_USERAUTH_INFO_RESPONSE
 */
void
input_userauth_info_req(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	char *name = NULL;
	char *inst = NULL;
	char *lang = NULL;
	char *prompt = NULL;
	char *response = NULL;
	unsigned int num_prompts, i;
	int echo = 0;

	debug2("input_userauth_info_req");

	if (authctxt == NULL)
		fatal("input_userauth_info_req: no authentication context");

	name = packet_get_string(NULL);
	inst = packet_get_string(NULL);
	lang = packet_get_string(NULL);

	if (strlen(name) > 0)
		cli_mesg(name);
	xfree(name);

	if (strlen(inst) > 0)
		cli_mesg(inst);
	xfree(inst);
	xfree(lang); 				/* unused */

	num_prompts = packet_get_int();
	/*
	 * Begin to build info response packet based on prompts requested.
	 * We commit to providing the correct number of responses, so if
	 * further on we run into a problem that prevents this, we have to
	 * be sure and clean this up and send a correct error response.
	 */
	packet_start(SSH2_MSG_USERAUTH_INFO_RESPONSE);
	packet_put_int(num_prompts);

	for (i = 0; i < num_prompts; i++) {
		prompt = packet_get_string(NULL);
		echo = packet_get_char();

		response = cli_prompt(prompt, echo);

		ssh_put_password(response);
		memset(response, 0, strlen(response));
		xfree(response);
		xfree(prompt);
	}
	packet_done(); /* done with parsing incoming message. */

	packet_send();
	packet_write_wait();
}

/* find auth method */

#define	DELIM	","

static char *def_authlist = "publickey,password";
static char *authlist_current = NULL;	 /* clean copy used for comparison */
static char *authname_current = NULL;	 /* last used auth method */
static char *authlist_working = NULL;	 /* copy that gets modified by strtok_r() */
static char *authlist_state = NULL;	 /* state variable for strtok_r() */

/*
 * Before starting to use a new authentication method list sent by the
 * server, reset internal variables.  This should also be called when
 * finished processing server list to free resources.
 */
void
authmethod_clear()
{
	if (authlist_current != NULL) {
		xfree(authlist_current);
		authlist_current = NULL;
	}
	if (authlist_working != NULL) {
		xfree(authlist_working);
		authlist_working = NULL;
	}
	if (authname_current != NULL) {
		xfree(authname_current);
		authlist_state = NULL;
	}
	if (authlist_state != NULL)
		authlist_state = NULL;
	return;
}

/*
 * given auth method name, if configurable options permit this method fill
 * in auth_ident field and return true, otherwise return false.
 */
int
authmethod_is_enabled(Authmethod *method)
{
	if (method == NULL)
		return 0;
	/* return false if options indicate this method is disabled */
	if  (method->enabled == NULL || *method->enabled == 0)
		return 0;
	/* return false if batch mode is enabled but method needs interactive mode */
	if  (method->batch_flag != NULL && *method->batch_flag != 0)
		return 0;
	return 1;
}

Authmethod *
authmethod_lookup(const char *name)
{
	Authmethod *method = NULL;
	if (name != NULL)
		for (method = authmethods; method->name != NULL; method++)
			if (strcmp(name, method->name) == 0)
				return method;
	debug2("Unrecognized authentication method name: %s", name ? name : "NULL");
	return NULL;
}

/*
 * Given the authentication method list sent by the server, return the
 * next method we should try.  If the server initially sends a nil list,
 * use a built-in default list.  If the server sends a nil list after
 * previously sending a valid list, continue using the list originally
 * sent.
 */ 

Authmethod *
authmethod_get(char *authlist)
{
	char *name = NULL, *authname_old;
	Authmethod *method = NULL;
	
	/* Use a suitable default if we're passed a nil list.  */
	if (authlist == NULL || strlen(authlist) == 0)
		authlist = def_authlist;

	if (authlist_current == NULL || strcmp(authlist, authlist_current) != 0) {
		/* start over if passed a different list */
		debug3("start over, passed a different list");
		authmethod_clear();
		authlist_current = xstrdup(authlist);
		authlist_working = xstrdup(authlist);
		name = strtok_r(authlist_working, DELIM, &authlist_state);
	} else {
		/*
		 * try to use previously used authentication method
		 * or continue to use previously passed list
		 */
		name = (authname_current != NULL) ?
		    authname_current : strtok_r(NULL, DELIM, &authlist_state);
	}

	while (name != NULL) {
		debug3("authmethod_lookup %s", name);
		method = authmethod_lookup(name);
		if (method != NULL && authmethod_is_enabled(method)) {
			debug3("authmethod_is_enabled %s", name);
			break;
		}
		name = strtok_r(NULL, DELIM, &authlist_state);
		method = NULL;
	}

	authname_old = authname_current;
	if (method != NULL) {
		debug("next auth method to try is %s", name);
		authname_current = xstrdup(name);
	} else {
		debug("no more auth methods to try");
		authname_current = NULL;
	}

	if (authname_old != NULL)
		xfree(authname_old);

	return (method);
}
