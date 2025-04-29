/* 	$OpenBSD: test_kex.c,v 1.9 2024/09/09 03:13:39 djm Exp $ */
/*
 * Regress test KEX
 *
 * Placed in the public domain
 */

#include "includes.h"

#include <sys/types.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "ssherr.h"
#include "ssh_api.h"
#include "sshbuf.h"
#include "packet.h"
#include "myproposal.h"
#include "log.h"

void kex_tests(void);
static int do_debug = 0;

static int
do_send_and_receive(struct ssh *from, struct ssh *to)
{
	u_char type;
	size_t len;
	const u_char *buf;
	int r;

	for (;;) {
		if ((r = ssh_packet_next(from, &type)) != 0) {
			fprintf(stderr, "ssh_packet_next: %s\n", ssh_err(r));
			return r;
		}
		if (type != 0)
			return 0;
		buf = ssh_output_ptr(from, &len);
		if (do_debug)
			printf("%zu", len);
		if (len == 0)
			return 0;
		if ((r = ssh_output_consume(from, len)) != 0 ||
		    (r = ssh_input_append(to, buf, len)) != 0)
			return r;
	}
}

static void
run_kex(struct ssh *client, struct ssh *server)
{
	int r = 0;

	while (!server->kex->done || !client->kex->done) {
		if (do_debug)
			printf(" S:");
		if ((r = do_send_and_receive(server, client)))
			break;
		if (do_debug)
			printf(" C:");
		if ((r = do_send_and_receive(client, server)))
			break;
	}
	if (do_debug)
		printf("done: %s\n", ssh_err(r));
	ASSERT_INT_EQ(r, 0);
	ASSERT_INT_EQ(server->kex->done, 1);
	ASSERT_INT_EQ(client->kex->done, 1);
}

static void
do_kex_with_key(char *kex, int keytype, int bits)
{
	struct ssh *client = NULL, *server = NULL, *server2 = NULL;
	struct sshkey *private, *public;
	struct sshbuf *state;
	struct kex_params kex_params;
	char *myproposal[PROPOSAL_MAX] = { KEX_CLIENT };
	char *keyname = NULL;

	TEST_START("sshkey_generate");
	ASSERT_INT_EQ(sshkey_generate(keytype, bits, &private), 0);
	TEST_DONE();

	TEST_START("sshkey_from_private");
	ASSERT_INT_EQ(sshkey_from_private(private, &public), 0);
	TEST_DONE();

	TEST_START("ssh_init");
	memcpy(kex_params.proposal, myproposal, sizeof(myproposal));
	if (kex != NULL)
		kex_params.proposal[PROPOSAL_KEX_ALGS] = kex;
	keyname = strdup(sshkey_ssh_name(private));
	ASSERT_PTR_NE(keyname, NULL);
	kex_params.proposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = keyname;
	ASSERT_INT_EQ(ssh_init(&client, 0, &kex_params), 0);
	ASSERT_INT_EQ(ssh_init(&server, 1, &kex_params), 0);
	ASSERT_PTR_NE(client, NULL);
	ASSERT_PTR_NE(server, NULL);
	TEST_DONE();

	TEST_START("ssh_add_hostkey");
	ASSERT_INT_EQ(ssh_add_hostkey(server, private), 0);
	ASSERT_INT_EQ(ssh_add_hostkey(client, public), 0);
	TEST_DONE();

	TEST_START("kex");
	run_kex(client, server);
	TEST_DONE();

	TEST_START("rekeying client");
	ASSERT_INT_EQ(kex_send_kexinit(client), 0);
	run_kex(client, server);
	TEST_DONE();

	TEST_START("rekeying server");
	ASSERT_INT_EQ(kex_send_kexinit(server), 0);
	run_kex(client, server);
	TEST_DONE();

	TEST_START("ssh_packet_get_state");
	state = sshbuf_new();
	ASSERT_PTR_NE(state, NULL);
	ASSERT_INT_EQ(ssh_packet_get_state(server, state), 0);
	ASSERT_INT_GE(sshbuf_len(state), 1);
	TEST_DONE();

	TEST_START("ssh_packet_set_state");
	server2 = NULL;
	ASSERT_INT_EQ(ssh_init(&server2, 1, NULL), 0);
	ASSERT_PTR_NE(server2, NULL);
	ASSERT_INT_EQ(ssh_add_hostkey(server2, private), 0);
	ASSERT_INT_EQ(ssh_packet_set_state(server2, state), 0);
	ASSERT_INT_EQ(sshbuf_len(state), 0);
	sshbuf_free(state);
	ASSERT_PTR_NE(server2->kex, NULL);
	/* XXX we need to set the callbacks */
#ifdef WITH_OPENSSL
	server2->kex->kex[KEX_DH_GRP1_SHA1] = kex_gen_server;
	server2->kex->kex[KEX_DH_GRP14_SHA1] = kex_gen_server;
	server2->kex->kex[KEX_DH_GEX_SHA1] = kexgex_server;
	server2->kex->kex[KEX_DH_GEX_SHA256] = kexgex_server;
#ifdef OPENSSL_HAS_ECC
	server2->kex->kex[KEX_ECDH_SHA2] = kex_gen_server;
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	server2->kex->kex[KEX_C25519_SHA256] = kex_gen_server;
	server2->kex->kex[KEX_KEM_SNTRUP761X25519_SHA512] = kex_gen_server;
	server2->kex->kex[KEX_KEM_MLKEM768X25519_SHA256] = kex_gen_server;
	server2->kex->load_host_public_key = server->kex->load_host_public_key;
	server2->kex->load_host_private_key = server->kex->load_host_private_key;
	server2->kex->sign = server->kex->sign;
	TEST_DONE();

	TEST_START("rekeying server2");
	ASSERT_INT_EQ(kex_send_kexinit(server2), 0);
	run_kex(client, server2);
	ASSERT_INT_EQ(kex_send_kexinit(client), 0);
	run_kex(client, server2);
	TEST_DONE();

	TEST_START("cleanup");
	sshkey_free(private);
	sshkey_free(public);
	ssh_free(client);
	ssh_free(server);
	ssh_free(server2);
	free(keyname);
	TEST_DONE();
}

static void
do_kex(char *kex)
{
#if 0
	log_init("test_kex", SYSLOG_LEVEL_DEBUG3, SYSLOG_FACILITY_AUTH, 1);
#endif
#ifdef WITH_OPENSSL
	do_kex_with_key(kex, KEY_RSA, 2048);
#ifdef WITH_DSA
	do_kex_with_key(kex, KEY_DSA, 1024);
#endif
#ifdef OPENSSL_HAS_ECC
	do_kex_with_key(kex, KEY_ECDSA, 256);
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	do_kex_with_key(kex, KEY_ED25519, 256);
}

void
kex_tests(void)
{
	do_kex("curve25519-sha256@libssh.org");
#ifdef WITH_OPENSSL
#ifdef OPENSSL_HAS_ECC
	do_kex("ecdh-sha2-nistp256");
	do_kex("ecdh-sha2-nistp384");
	do_kex("ecdh-sha2-nistp521");
#endif /* OPENSSL_HAS_ECC */
	do_kex("diffie-hellman-group-exchange-sha256");
	do_kex("diffie-hellman-group-exchange-sha1");
	do_kex("diffie-hellman-group14-sha1");
	do_kex("diffie-hellman-group1-sha1");
# ifdef USE_MLKEM768X25519
	do_kex("mlkem768x25519-sha256");
# endif /* USE_MLKEM768X25519 */
# ifdef USE_SNTRUP761X25519
	do_kex("sntrup761x25519-sha512@openssh.com");
# endif /* USE_SNTRUP761X25519 */
#endif /* WITH_OPENSSL */
}
