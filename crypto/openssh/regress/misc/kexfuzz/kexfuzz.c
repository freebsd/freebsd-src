/* 	$OpenBSD: kexfuzz.c,v 1.1 2016/03/04 02:30:37 djm Exp $ */
/*
 * Fuzz harness for KEX code
 *
 * Placed in the public domain
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_ERR_H
# include <err.h>
#endif

#include "ssherr.h"
#include "ssh_api.h"
#include "sshbuf.h"
#include "packet.h"
#include "myproposal.h"
#include "authfile.h"

struct ssh *active_state = NULL; /* XXX - needed for linking */

void kex_tests(void);
static int do_debug = 0;

enum direction { S2C, C2S };

static int
do_send_and_receive(struct ssh *from, struct ssh *to, int mydirection,
    int *packet_count, int trigger_direction, int packet_index,
    const char *dump_path, struct sshbuf *replace_data)
{
	u_char type;
	size_t len, olen;
	const u_char *buf;
	int r;
	FILE *dumpfile;

	for (;;) {
		if ((r = ssh_packet_next(from, &type)) != 0) {
			fprintf(stderr, "ssh_packet_next: %s\n", ssh_err(r));
			return r;
		}
		if (type != 0)
			return 0;
		buf = ssh_output_ptr(from, &len);
		olen = len;
		if (do_debug) {
			printf("%s packet %d type %u len %zu:\n",
			    mydirection == S2C ? "s2c" : "c2s",
			    *packet_count, type, len);
			sshbuf_dump_data(buf, len, stdout);
		}
		if (mydirection == trigger_direction &&
		    packet_index == *packet_count) {
			if (replace_data != NULL) {
				buf = sshbuf_ptr(replace_data);
				len = sshbuf_len(replace_data);
				if (do_debug) {
					printf("***** replaced packet "
					    "len %zu\n", len);
					sshbuf_dump_data(buf, len, stdout);
				}
			} else if (dump_path != NULL) {
				if ((dumpfile = fopen(dump_path, "w+")) == NULL)
					err(1, "fopen %s", dump_path);
				if (len != 0 &&
				    fwrite(buf, len, 1, dumpfile) != 1)
					err(1, "fwrite %s", dump_path);
				if (do_debug)
					printf("***** dumped packet "
					    "len %zu\n", len);
				fclose(dumpfile);
				exit(0);
			}
		}
		(*packet_count)++;
		if (len == 0)
			return 0;
		if ((r = ssh_input_append(to, buf, len)) != 0 ||
		    (r = ssh_output_consume(from, olen)) != 0)
			return r;
	}
}

/* Minimal test_helper.c scaffholding to make this standalone */
const char *in_test = NULL;
#define TEST_START(a)	\
	do { \
		in_test = (a); \
		if (do_debug) \
			fprintf(stderr, "test %s starting\n", in_test); \
	} while (0)
#define TEST_DONE()	\
	do { \
		if (do_debug) \
			fprintf(stderr, "test %s done\n", \
			    in_test ? in_test : "???"); \
		in_test = NULL; \
	} while(0)
#define ASSERT_INT_EQ(a, b) \
	do { \
		if ((int)(a) != (int)(b)) { \
			fprintf(stderr, "%s %s:%d " \
			    "%s (%d) != expected %s (%d)\n", \
			    in_test ? in_test : "(none)", \
			    __func__, __LINE__, #a, (int)(a), #b, (int)(b)); \
			exit(2); \
		} \
	} while (0)
#define ASSERT_INT_GE(a, b) \
	do { \
		if ((int)(a) < (int)(b)) { \
			fprintf(stderr, "%s %s:%d " \
			    "%s (%d) < expected %s (%d)\n", \
			    in_test ? in_test : "(none)", \
			    __func__, __LINE__, #a, (int)(a), #b, (int)(b)); \
			exit(2); \
		} \
	} while (0)
#define ASSERT_PTR_NE(a, b) \
	do { \
		if ((a) == (b)) { \
			fprintf(stderr, "%s %s:%d " \
			    "%s (%p) != expected %s (%p)\n", \
			    in_test ? in_test : "(none)", \
			    __func__, __LINE__, #a, (a), #b, (b)); \
			exit(2); \
		} \
	} while (0)


static void
run_kex(struct ssh *client, struct ssh *server, int *s2c, int *c2s,
    int direction, int packet_index,
    const char *dump_path, struct sshbuf *replace_data)
{
	int r = 0;

	while (!server->kex->done || !client->kex->done) {
		if ((r = do_send_and_receive(server, client, S2C, s2c,
		    direction, packet_index, dump_path, replace_data)))
			break;
		if ((r = do_send_and_receive(client, server, C2S, c2s,
		    direction, packet_index, dump_path, replace_data)))
			break;
	}
	if (do_debug)
		printf("done: %s\n", ssh_err(r));
	ASSERT_INT_EQ(r, 0);
	ASSERT_INT_EQ(server->kex->done, 1);
	ASSERT_INT_EQ(client->kex->done, 1);
}

static void
do_kex_with_key(const char *kex, struct sshkey *prvkey, int *c2s, int *s2c,
    int direction, int packet_index,
    const char *dump_path, struct sshbuf *replace_data)
{
	struct ssh *client = NULL, *server = NULL, *server2 = NULL;
	struct sshkey *pubkey = NULL;
	struct sshbuf *state;
	struct kex_params kex_params;
	char *myproposal[PROPOSAL_MAX] = { KEX_CLIENT };
	char *keyname = NULL;

	TEST_START("sshkey_from_private");
	ASSERT_INT_EQ(sshkey_from_private(prvkey, &pubkey), 0);
	TEST_DONE();

	TEST_START("ssh_init");
	memcpy(kex_params.proposal, myproposal, sizeof(myproposal));
	if (kex != NULL)
		kex_params.proposal[PROPOSAL_KEX_ALGS] = strdup(kex);
	keyname = strdup(sshkey_ssh_name(prvkey));
	ASSERT_PTR_NE(keyname, NULL);
	kex_params.proposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = keyname;
	ASSERT_INT_EQ(ssh_init(&client, 0, &kex_params), 0);
	ASSERT_INT_EQ(ssh_init(&server, 1, &kex_params), 0);
	ASSERT_PTR_NE(client, NULL);
	ASSERT_PTR_NE(server, NULL);
	TEST_DONE();

	TEST_START("ssh_add_hostkey");
	ASSERT_INT_EQ(ssh_add_hostkey(server, prvkey), 0);
	ASSERT_INT_EQ(ssh_add_hostkey(client, pubkey), 0);
	TEST_DONE();

	TEST_START("kex");
	run_kex(client, server, s2c, c2s, direction, packet_index,
	    dump_path, replace_data);
	TEST_DONE();

	TEST_START("rekeying client");
	ASSERT_INT_EQ(kex_send_kexinit(client), 0);
	run_kex(client, server, s2c, c2s, direction, packet_index,
	    dump_path, replace_data);
	TEST_DONE();

	TEST_START("rekeying server");
	ASSERT_INT_EQ(kex_send_kexinit(server), 0);
	run_kex(client, server, s2c, c2s, direction, packet_index,
	    dump_path, replace_data);
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
	ASSERT_INT_EQ(ssh_add_hostkey(server2, prvkey), 0);
	kex_free(server2->kex);	/* XXX or should ssh_packet_set_state()? */
	ASSERT_INT_EQ(ssh_packet_set_state(server2, state), 0);
	ASSERT_INT_EQ(sshbuf_len(state), 0);
	sshbuf_free(state);
	ASSERT_PTR_NE(server2->kex, NULL);
	/* XXX we need to set the callbacks */
	server2->kex->kex[KEX_DH_GRP1_SHA1] = kexdh_server;
	server2->kex->kex[KEX_DH_GRP14_SHA1] = kexdh_server;
	server2->kex->kex[KEX_DH_GEX_SHA1] = kexgex_server;
	server2->kex->kex[KEX_DH_GEX_SHA256] = kexgex_server;
#ifdef OPENSSL_HAS_ECC
	server2->kex->kex[KEX_ECDH_SHA2] = kexecdh_server;
#endif
	server2->kex->kex[KEX_C25519_SHA256] = kexc25519_server;
	server2->kex->load_host_public_key = server->kex->load_host_public_key;
	server2->kex->load_host_private_key = server->kex->load_host_private_key;
	server2->kex->sign = server->kex->sign;
	TEST_DONE();

	TEST_START("rekeying server2");
	ASSERT_INT_EQ(kex_send_kexinit(server2), 0);
	run_kex(client, server2, s2c, c2s, direction, packet_index,
	    dump_path, replace_data);
	ASSERT_INT_EQ(kex_send_kexinit(client), 0);
	run_kex(client, server2, s2c, c2s, direction, packet_index,
	    dump_path, replace_data);
	TEST_DONE();

	TEST_START("cleanup");
	sshkey_free(pubkey);
	ssh_free(client);
	ssh_free(server);
	ssh_free(server2);
	free(keyname);
	TEST_DONE();
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: kexfuzz [-hcdrv] [-D direction] [-f data_file]\n"
	    "               [-K kex_alg] [-k private_key] [-i packet_index]\n"
	    "\n"
	    "Options:\n"
	    "    -h               Display this help\n"
	    "    -c               Count packets sent during KEX\n"
	    "    -d               Dump mode: record KEX packet to data file\n"
	    "    -r               Replace mode: replace packet with data file\n"
	    "    -v               Turn on verbose logging\n"
	    "    -D S2C|C2S       Packet direction for replacement or dump\n"
	    "    -f data_file     Path to data file for replacement or dump\n"
	    "    -K kex_alg       Name of KEX algorithm to test (see below)\n"
	    "    -k private_key   Path to private key file\n"
	    "    -i packet_index  Index of packet to replace or dump (from 0)\n"
	    "\n"
	    "Available KEX algorithms: %s\n", kex_alg_list(' '));
}

static void
badusage(const char *bad)
{
	fprintf(stderr, "Invalid options\n");
	fprintf(stderr, "%s\n", bad);
	usage();
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch, fd, r;
	int count_flag = 0, dump_flag = 0, replace_flag = 0;
	int packet_index = -1, direction = -1;
	int s2c = 0, c2s = 0; /* packet counts */
	const char *kex = NULL, *kpath = NULL, *data_path = NULL;
	struct sshkey *key = NULL;
	struct sshbuf *replace_data = NULL;

	setvbuf(stdout, NULL, _IONBF, 0);
	while ((ch = getopt(argc, argv, "hcdrvD:f:K:k:i:")) != -1) {
		switch (ch) {
		case 'h':
			usage();
			return 0;
		case 'c':
			count_flag = 1;
			break;
		case 'd':
			dump_flag = 1;
			break;
		case 'r':
			replace_flag = 1;
			break;
		case 'v':
			do_debug = 1;
			break;

		case 'D':
			if (strcasecmp(optarg, "s2c") == 0)
				direction = S2C;
			else if (strcasecmp(optarg, "c2s") == 0)
				direction = C2S;
			else
				badusage("Invalid direction (-D)");
			break;
		case 'f':
			data_path = optarg;
			break;
		case 'K':
			kex = optarg;
			break;
		case 'k':
			kpath = optarg;
			break;
		case 'i':
			packet_index = atoi(optarg);
			if (packet_index < 0)
				badusage("Invalid packet index");
			break;
		default:
			badusage("unsupported flag");
		}
	}
	argc -= optind;
	argv += optind;

	/* Must select a single mode */
	if ((count_flag + dump_flag + replace_flag) != 1)
		badusage("Must select one mode: -c, -d or -r");
	/* KEX type is mandatory */
	if (kex == NULL || !kex_names_valid(kex) || strchr(kex, ',') != NULL)
		badusage("Missing or invalid kex type (-K flag)");
	/* Valid key is mandatory */
	if (kpath == NULL)
		badusage("Missing private key (-k flag)");
	if ((fd = open(kpath, O_RDONLY)) == -1)
		err(1, "open %s", kpath);
	if ((r = sshkey_load_private_type_fd(fd, KEY_UNSPEC, NULL,
	    &key, NULL)) != 0)
		errx(1, "Unable to load key %s: %s", kpath, ssh_err(r));
	close(fd);
	/* XXX check that it is a private key */
	/* XXX support certificates */
	if (key == NULL || key->type == KEY_UNSPEC || key->type == KEY_RSA1)
		badusage("Invalid key file (-k flag)");

	/* Replace (fuzz) mode */
	if (replace_flag) {
		if (packet_index == -1 || direction == -1 || data_path == NULL)
			badusage("Replace (-r) mode must specify direction "
			    "(-D) packet index (-i) and data path (-f)");
		if ((fd = open(data_path, O_RDONLY)) == -1)
			err(1, "open %s", data_path);
		replace_data = sshbuf_new();
		if ((r = sshkey_load_file(fd, replace_data)) != 0)
			errx(1, "read %s: %s", data_path, ssh_err(r));
		close(fd);
	}

	/* Dump mode */
	if (dump_flag) {
		if (packet_index == -1 || direction == -1 || data_path == NULL)
			badusage("Dump (-d) mode must specify direction "
			    "(-D), packet index (-i) and data path (-f)");
	}

	/* Count mode needs no further flags */

	do_kex_with_key(kex, key, &c2s, &s2c,
	    direction, packet_index,
	    dump_flag ? data_path : NULL,
	    replace_flag ? replace_data : NULL);
	sshkey_free(key);
	sshbuf_free(replace_data);

	if (count_flag) {
		printf("S2C: %d\n", s2c);
		printf("C2S: %d\n", c2s);
	}

	return 0;
}
