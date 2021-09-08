// libfuzzer driver for key exchange fuzzing.


#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" {

#include "includes.h"
#include "ssherr.h"
#include "ssh_api.h"
#include "sshbuf.h"
#include "packet.h"
#include "myproposal.h"
#include "xmalloc.h"
#include "authfile.h"
#include "log.h"

#include "fixed-keys.h"

// Define if you want to generate traces.
/* #define STANDALONE 1 */

static int prepare_key(struct shared_state *st, int keytype, int bits);

struct shared_state {
	size_t nkeys;
	struct sshkey **privkeys, **pubkeys;
};

struct test_state {
	struct sshbuf *smsgs, *cmsgs; /* output, for standalone mode */
	struct sshbuf *sin, *cin; /* input; setup per-test in do_kex_with_key */
	struct sshbuf *s_template, *c_template; /* main copy of input */
};

static int
do_send_and_receive(struct ssh *from, struct ssh *to,
    struct sshbuf *store, int clobber, size_t *n)
{
	u_char type;
	size_t len;
	const u_char *buf;
	int r;

	for (*n = 0;; (*n)++) {
		if ((r = ssh_packet_next(from, &type)) != 0) {
			debug_fr(r, "ssh_packet_next");
			return r;
		}
		if (type != 0)
			return 0;
		buf = ssh_output_ptr(from, &len);
		debug_f("%zu%s", len, clobber ? " ignore" : "");
		if (len == 0)
			return 0;
		if ((r = ssh_output_consume(from, len)) != 0) {
			debug_fr(r, "ssh_output_consume");
			return r;
		}
		if (store != NULL && (r = sshbuf_put(store, buf, len)) != 0) {
			debug_fr(r, "sshbuf_put");
			return r;
		}
		if (!clobber && (r = ssh_input_append(to, buf, len)) != 0) {
			debug_fr(r, "ssh_input_append");
			return r;
		}
	}
}

static int
run_kex(struct test_state *ts, struct ssh *client, struct ssh *server)
{
	int r = 0;
	size_t cn, sn;

	/* If fuzzing, replace server/client input */
	if (ts->sin != NULL) {
		if ((r = ssh_input_append(server, sshbuf_ptr(ts->sin),
		    sshbuf_len(ts->sin))) != 0) {
			error_fr(r, "ssh_input_append");
			return r;
		}
		sshbuf_reset(ts->sin);
	}
	if (ts->cin != NULL) {
		if ((r = ssh_input_append(client, sshbuf_ptr(ts->cin),
		    sshbuf_len(ts->cin))) != 0) {
			error_fr(r, "ssh_input_append");
			return r;
		}
		sshbuf_reset(ts->cin);
	}
	while (!server->kex->done || !client->kex->done) {
		cn = sn = 0;
		debug_f("S:");
		if ((r = do_send_and_receive(server, client,
		    ts->smsgs, ts->cin != NULL, &sn)) != 0) {
			debug_fr(r, "S->C");
			break;
		}
		debug_f("C:");
		if ((r = do_send_and_receive(client, server,
		    ts->cmsgs, ts->sin != NULL, &cn)) != 0) {
			debug_fr(r, "C->S");
			break;
		}
		if (cn == 0 && sn == 0) {
			debug_f("kex stalled");
			r = SSH_ERR_PROTOCOL_ERROR;
			break;
		}
	}
	debug_fr(r, "done");
	return r;
}

static void
store_key(struct shared_state *st, struct sshkey *pubkey,
    struct sshkey *privkey)
{
	if (st == NULL || pubkey->type < 0 || pubkey->type > INT_MAX ||
	    privkey->type != pubkey->type ||
	    ((size_t)pubkey->type < st->nkeys &&
	     st->pubkeys[pubkey->type] != NULL))
		abort();
	if ((size_t)pubkey->type >= st->nkeys) {
		st->pubkeys = (struct sshkey **)xrecallocarray(st->pubkeys,
		    st->nkeys, pubkey->type + 1, sizeof(*st->pubkeys));
		st->privkeys = (struct sshkey **)xrecallocarray(st->privkeys,
		    st->nkeys, privkey->type + 1, sizeof(*st->privkeys));
		st->nkeys = privkey->type + 1;
	}
	debug_f("store %s at %d", sshkey_ssh_name(pubkey), pubkey->type);
	st->pubkeys[pubkey->type] = pubkey;
	st->privkeys[privkey->type] = privkey;
}

static int
prepare_keys(struct shared_state *st)
{
	if (prepare_key(st, KEY_RSA, 2048) != 0 ||
	    prepare_key(st, KEY_DSA, 1024) != 0 ||
	    prepare_key(st, KEY_ECDSA, 256) != 0 ||
	    prepare_key(st, KEY_ED25519, 256) != 0) {
		error_f("key prepare failed");
		return -1;
	}
	return 0;
}

static struct sshkey *
get_pubkey(struct shared_state *st, int keytype)
{
	if (st == NULL || keytype < 0 || (size_t)keytype >= st->nkeys ||
	    st->pubkeys == NULL || st->pubkeys[keytype] == NULL)
		abort();
	return st->pubkeys[keytype];
}

static struct sshkey *
get_privkey(struct shared_state *st, int keytype)
{
	if (st == NULL || keytype < 0 || (size_t)keytype >= st->nkeys ||
	    st->privkeys == NULL || st->privkeys[keytype] == NULL)
		abort();
	return st->privkeys[keytype];
}

static int
do_kex_with_key(struct shared_state *st, struct test_state *ts,
    const char *kex, int keytype)
{
	struct ssh *client = NULL, *server = NULL;
	struct sshkey *privkey = NULL, *pubkey = NULL;
	struct sshbuf *state = NULL;
	struct kex_params kex_params;
	const char *ccp, *proposal[PROPOSAL_MAX] = { KEX_CLIENT };
	char *myproposal[PROPOSAL_MAX] = {0}, *keyname = NULL;
	int i, r;

	ts->cin = ts->sin = NULL;
	if (ts->c_template != NULL &&
	    (ts->cin = sshbuf_fromb(ts->c_template)) == NULL)
		abort();
	if (ts->s_template != NULL &&
	    (ts->sin = sshbuf_fromb(ts->s_template)) == NULL)
		abort();

	pubkey = get_pubkey(st, keytype);
	privkey = get_privkey(st, keytype);
	keyname = xstrdup(sshkey_ssh_name(privkey));
	if (ts->cin != NULL) {
		debug_f("%s %s clobber client %zu", kex, keyname,
		    sshbuf_len(ts->cin));
	} else if (ts->sin != NULL) {
		debug_f("%s %s clobber server %zu", kex, keyname,
		    sshbuf_len(ts->sin));
	} else
		debug_f("%s %s noclobber", kex, keyname);

	for (i = 0; i < PROPOSAL_MAX; i++) {
		ccp = proposal[i];
#ifdef CIPHER_NONE_AVAIL
		if (i == PROPOSAL_ENC_ALGS_CTOS || i == PROPOSAL_ENC_ALGS_STOC)
			ccp = "none";
#endif
		if (i == PROPOSAL_SERVER_HOST_KEY_ALGS)
			ccp = keyname;
		else if (i == PROPOSAL_KEX_ALGS && kex != NULL)
			ccp = kex;
		if ((myproposal[i] = strdup(ccp)) == NULL) {
			error_f("strdup prop %d", i);
			goto fail;
		}
	}
	memcpy(kex_params.proposal, myproposal, sizeof(myproposal));
	if ((r = ssh_init(&client, 0, &kex_params)) != 0) {
		error_fr(r, "init client");
		goto fail;
	}
	if ((r = ssh_init(&server, 1, &kex_params)) != 0) {
		error_fr(r, "init server");
		goto fail;
	}
	if ((r = ssh_add_hostkey(server, privkey)) != 0 ||
	    (r = ssh_add_hostkey(client, pubkey)) != 0) {
		error_fr(r, "add hostkeys");
		goto fail;
	}
	if ((r = run_kex(ts, client, server)) != 0) {
		error_fr(r, "kex");
		goto fail;
	}
	/* XXX rekex, set_state, etc */
 fail:
	for (i = 0; i < PROPOSAL_MAX; i++)
		free(myproposal[i]);
	sshbuf_free(ts->sin);
	sshbuf_free(ts->cin);
	sshbuf_free(state);
	ssh_free(client);
	ssh_free(server);
	free(keyname);
	return r;
}

static int
prepare_key(struct shared_state *st, int kt, int bits)
{
	const char *pubstr = NULL;
	const char *privstr = NULL;
	char *tmp, *cp;
	struct sshkey *privkey = NULL, *pubkey = NULL;
	struct sshbuf *b = NULL;
	int r;

	switch (kt) {
	case KEY_RSA:
		pubstr = PUB_RSA;
		privstr = PRIV_RSA;
		break;
	case KEY_DSA:
		pubstr = PUB_DSA;
		privstr = PRIV_DSA;
		break;
	case KEY_ECDSA:
		pubstr = PUB_ECDSA;
		privstr = PRIV_ECDSA;
		break;
	case KEY_ED25519:
		pubstr = PUB_ED25519;
		privstr = PRIV_ED25519;
		break;
	default:
		abort();
	}
	if ((b = sshbuf_from(privstr, strlen(privstr))) == NULL)
		abort();
	if ((r = sshkey_parse_private_fileblob(b, "", &privkey, NULL)) != 0) {
		error_fr(r, "priv %d", kt);
		abort();
	}
	sshbuf_free(b);
	tmp = cp = xstrdup(pubstr);
	if ((pubkey = sshkey_new(KEY_UNSPEC)) == NULL)
		abort();
	if ((r = sshkey_read(pubkey, &cp)) != 0) {
		error_fr(r, "pub %d", kt);
		abort();
	}
	free(tmp);

	store_key(st, pubkey, privkey);
	return 0;
}

#if defined(STANDALONE)

#if 0 /* use this if generating new keys to embed above */
static int
prepare_key(struct shared_state *st, int keytype, int bits)
{
	struct sshkey *privkey = NULL, *pubkey = NULL;
	int r;

	if ((r = sshkey_generate(keytype, bits, &privkey)) != 0) {
		error_fr(r, "generate");
		abort();
	}
	if ((r = sshkey_from_private(privkey, &pubkey)) != 0) {
		error_fr(r, "make pubkey");
		abort();
	}
	store_key(st, pubkey, privkey);
	return 0;
}
#endif

int main(void)
{
	static struct shared_state *st;
	struct test_state *ts;
	const int keytypes[] = { KEY_RSA, KEY_DSA, KEY_ECDSA, KEY_ED25519, -1 };
	const char *kextypes[] = {
		"sntrup761x25519-sha512@openssh.com",
		"curve25519-sha256@libssh.org",
		"ecdh-sha2-nistp256",
		"diffie-hellman-group1-sha1",
		"diffie-hellman-group-exchange-sha1",
		NULL,
	};
	int i, j;
	char *path;
	FILE *f;

	log_init("kex_fuzz", SYSLOG_LEVEL_DEBUG3, SYSLOG_FACILITY_AUTH, 1);

	if (st == NULL) {
		st = (struct shared_state *)xcalloc(1, sizeof(*st));
		prepare_keys(st);
	}
	/* Run each kex method for each key and save client/server packets */
	for (i = 0; keytypes[i] != -1; i++) {
		for (j = 0; kextypes[j] != NULL; j++) {
			ts = (struct test_state *)xcalloc(1, sizeof(*ts));
			ts->smsgs = sshbuf_new();
			ts->cmsgs = sshbuf_new();
			do_kex_with_key(st, ts, kextypes[j], keytypes[i]);
			xasprintf(&path, "S2C-%s-%s",
			    kextypes[j], sshkey_type(st->pubkeys[keytypes[i]]));
			debug_f("%s", path);
			if ((f = fopen(path, "wb+")) == NULL)
				abort();
			if (fwrite(sshbuf_ptr(ts->smsgs), 1,
			    sshbuf_len(ts->smsgs), f) != sshbuf_len(ts->smsgs))
				abort();
			fclose(f);
			free(path);
			//sshbuf_dump(ts->smsgs, stderr);
			xasprintf(&path, "C2S-%s-%s",
			    kextypes[j], sshkey_type(st->pubkeys[keytypes[i]]));
			debug_f("%s", path);
			if ((f = fopen(path, "wb+")) == NULL)
				abort();
			if (fwrite(sshbuf_ptr(ts->cmsgs), 1,
			    sshbuf_len(ts->cmsgs), f) != sshbuf_len(ts->cmsgs))
				abort();
			fclose(f);
			free(path);
			//sshbuf_dump(ts->cmsgs, stderr);
			sshbuf_free(ts->smsgs);
			sshbuf_free(ts->cmsgs);
			free(ts);
		}
	}
	for (i = 0; keytypes[i] != -1; i++) {
		xasprintf(&path, "%s.priv",
		    sshkey_type(st->privkeys[keytypes[i]]));
		debug_f("%s", path);
		if (sshkey_save_private(st->privkeys[keytypes[i]], path,
		    "", "", SSHKEY_PRIVATE_OPENSSH, NULL, 0) != 0)
			abort();
		free(path);
		xasprintf(&path, "%s.pub",
		    sshkey_type(st->pubkeys[keytypes[i]]));
		debug_f("%s", path);
		if (sshkey_save_public(st->pubkeys[keytypes[i]], path, "") != 0)
			abort();
		free(path);
	}
}
#else /* !STANDALONE */
static void
do_kex(struct shared_state *st, struct test_state *ts, const char *kex)
{
	do_kex_with_key(st, ts, kex, KEY_RSA);
	do_kex_with_key(st, ts, kex, KEY_DSA);
	do_kex_with_key(st, ts, kex, KEY_ECDSA);
	do_kex_with_key(st, ts, kex, KEY_ED25519);
}

static void
kex_tests(struct shared_state *st, struct test_state *ts)
{
	do_kex(st, ts, "sntrup761x25519-sha512@openssh.com");
	do_kex(st, ts, "curve25519-sha256@libssh.org");
	do_kex(st, ts, "ecdh-sha2-nistp256");
	do_kex(st, ts, "diffie-hellman-group1-sha1");
	do_kex(st, ts, "diffie-hellman-group-exchange-sha1");
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	static struct shared_state *st;
	struct test_state *ts;
	u_char crbuf[SSH_MAX_PRE_BANNER_LINES * 4];
	u_char zbuf[4096] = {0};
	static LogLevel loglevel = SYSLOG_LEVEL_INFO;

	if (st == NULL) {
		if (getenv("DEBUG") != NULL || getenv("KEX_FUZZ_DEBUG") != NULL)
			loglevel = SYSLOG_LEVEL_DEBUG3;
		log_init("kex_fuzz",
		    loglevel, SYSLOG_FACILITY_AUTH, 1);
		st = (struct shared_state *)xcalloc(1, sizeof(*st));
		prepare_keys(st);
	}

	/* Ensure that we can complete (fail) banner exchange at least */
	memset(crbuf, '\n', sizeof(crbuf));

	ts = (struct test_state *)xcalloc(1, sizeof(*ts));
	if ((ts->s_template = sshbuf_new()) == NULL ||
	    sshbuf_put(ts->s_template, data, size) != 0 ||
	    sshbuf_put(ts->s_template, crbuf, sizeof(crbuf)) != 0 ||
	    sshbuf_put(ts->s_template, zbuf, sizeof(zbuf)) != 0)
		abort();
	kex_tests(st, ts);
	sshbuf_free(ts->s_template);
	free(ts);

	ts = (struct test_state *)xcalloc(1, sizeof(*ts));
	if ((ts->c_template = sshbuf_new()) == NULL ||
	    sshbuf_put(ts->c_template, data, size) != 0 ||
	    sshbuf_put(ts->c_template, crbuf, sizeof(crbuf)) != 0 ||
	    sshbuf_put(ts->c_template, zbuf, sizeof(zbuf)) != 0)
		abort();
	kex_tests(st, ts);
	sshbuf_free(ts->c_template);
	free(ts);

	return 0;
}
#endif /* STANDALONE */
} /* extern "C" */
