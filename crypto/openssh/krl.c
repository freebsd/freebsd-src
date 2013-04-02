/*
 * Copyright (c) 2012 Damien Miller <djm@mindrot.org>
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

/* $OpenBSD: krl.c,v 1.9 2013/01/27 10:06:12 djm Exp $ */

#include "includes.h"

#include <sys/types.h>
#include <sys/param.h>
#include <openbsd-compat/sys-tree.h>
#include <openbsd-compat/sys-queue.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "buffer.h"
#include "key.h"
#include "authfile.h"
#include "misc.h"
#include "log.h"
#include "xmalloc.h"

#include "krl.h"

/* #define DEBUG_KRL */
#ifdef DEBUG_KRL
# define KRL_DBG(x) debug3 x
#else
# define KRL_DBG(x)
#endif

/*
 * Trees of revoked serial numbers, key IDs and keys. This allows
 * quick searching, querying and producing lists in canonical order.
 */

/* Tree of serial numbers. XXX make smarter: really need a real sparse bitmap */
struct revoked_serial {
	u_int64_t lo, hi;
	RB_ENTRY(revoked_serial) tree_entry;
};
static int serial_cmp(struct revoked_serial *a, struct revoked_serial *b);
RB_HEAD(revoked_serial_tree, revoked_serial);
RB_GENERATE_STATIC(revoked_serial_tree, revoked_serial, tree_entry, serial_cmp);

/* Tree of key IDs */
struct revoked_key_id {
	char *key_id;
	RB_ENTRY(revoked_key_id) tree_entry;
};
static int key_id_cmp(struct revoked_key_id *a, struct revoked_key_id *b);
RB_HEAD(revoked_key_id_tree, revoked_key_id);
RB_GENERATE_STATIC(revoked_key_id_tree, revoked_key_id, tree_entry, key_id_cmp);

/* Tree of blobs (used for keys and fingerprints) */
struct revoked_blob {
	u_char *blob;
	u_int len;
	RB_ENTRY(revoked_blob) tree_entry;
};
static int blob_cmp(struct revoked_blob *a, struct revoked_blob *b);
RB_HEAD(revoked_blob_tree, revoked_blob);
RB_GENERATE_STATIC(revoked_blob_tree, revoked_blob, tree_entry, blob_cmp);

/* Tracks revoked certs for a single CA */
struct revoked_certs {
	Key *ca_key;
	struct revoked_serial_tree revoked_serials;
	struct revoked_key_id_tree revoked_key_ids;
	TAILQ_ENTRY(revoked_certs) entry;
};
TAILQ_HEAD(revoked_certs_list, revoked_certs);

struct ssh_krl {
	u_int64_t krl_version;
	u_int64_t generated_date;
	u_int64_t flags;
	char *comment;
	struct revoked_blob_tree revoked_keys;
	struct revoked_blob_tree revoked_sha1s;
	struct revoked_certs_list revoked_certs;
};

/* Return equal if a and b overlap */
static int
serial_cmp(struct revoked_serial *a, struct revoked_serial *b)
{
	if (a->hi >= b->lo && a->lo <= b->hi)
		return 0;
	return a->lo < b->lo ? -1 : 1;
}

static int
key_id_cmp(struct revoked_key_id *a, struct revoked_key_id *b)
{
	return strcmp(a->key_id, b->key_id);
}

static int
blob_cmp(struct revoked_blob *a, struct revoked_blob *b)
{
	int r;

	if (a->len != b->len) {
		if ((r = memcmp(a->blob, b->blob, MIN(a->len, b->len))) != 0)
			return r;
		return a->len > b->len ? 1 : -1;
	} else
		return memcmp(a->blob, b->blob, a->len);
}

struct ssh_krl *
ssh_krl_init(void)
{
	struct ssh_krl *krl;

	if ((krl = calloc(1, sizeof(*krl))) == NULL)
		return NULL;
	RB_INIT(&krl->revoked_keys);
	RB_INIT(&krl->revoked_sha1s);
	TAILQ_INIT(&krl->revoked_certs);
	return krl;
}

static void
revoked_certs_free(struct revoked_certs *rc)
{
	struct revoked_serial *rs, *trs;
	struct revoked_key_id *rki, *trki;

	RB_FOREACH_SAFE(rs, revoked_serial_tree, &rc->revoked_serials, trs) {
		RB_REMOVE(revoked_serial_tree, &rc->revoked_serials, rs);
		free(rs);
	}
	RB_FOREACH_SAFE(rki, revoked_key_id_tree, &rc->revoked_key_ids, trki) {
		RB_REMOVE(revoked_key_id_tree, &rc->revoked_key_ids, rki);
		free(rki->key_id);
		free(rki);
	}
	if (rc->ca_key != NULL)
		key_free(rc->ca_key);
}

void
ssh_krl_free(struct ssh_krl *krl)
{
	struct revoked_blob *rb, *trb;
	struct revoked_certs *rc, *trc;

	if (krl == NULL)
		return;

	free(krl->comment);
	RB_FOREACH_SAFE(rb, revoked_blob_tree, &krl->revoked_keys, trb) {
		RB_REMOVE(revoked_blob_tree, &krl->revoked_keys, rb);
		free(rb->blob);
		free(rb);
	}
	RB_FOREACH_SAFE(rb, revoked_blob_tree, &krl->revoked_sha1s, trb) {
		RB_REMOVE(revoked_blob_tree, &krl->revoked_sha1s, rb);
		free(rb->blob);
		free(rb);
	}
	TAILQ_FOREACH_SAFE(rc, &krl->revoked_certs, entry, trc) {
		TAILQ_REMOVE(&krl->revoked_certs, rc, entry);
		revoked_certs_free(rc);
	}
}

void
ssh_krl_set_version(struct ssh_krl *krl, u_int64_t version)
{
	krl->krl_version = version;
}

void
ssh_krl_set_comment(struct ssh_krl *krl, const char *comment)
{
	free(krl->comment);
	if ((krl->comment = strdup(comment)) == NULL)
		fatal("%s: strdup", __func__);
}

/*
 * Find the revoked_certs struct for a CA key. If allow_create is set then
 * create a new one in the tree if one did not exist already.
 */
static int
revoked_certs_for_ca_key(struct ssh_krl *krl, const Key *ca_key,
    struct revoked_certs **rcp, int allow_create)
{
	struct revoked_certs *rc;

	*rcp = NULL;
	TAILQ_FOREACH(rc, &krl->revoked_certs, entry) {
		if (key_equal(rc->ca_key, ca_key)) {
			*rcp = rc;
			return 0;
		}
	}
	if (!allow_create)
		return 0;
	/* If this CA doesn't exist in the list then add it now */
	if ((rc = calloc(1, sizeof(*rc))) == NULL)
		return -1;
	if ((rc->ca_key = key_from_private(ca_key)) == NULL) {
		free(rc);
		return -1;
	}
	RB_INIT(&rc->revoked_serials);
	RB_INIT(&rc->revoked_key_ids);
	TAILQ_INSERT_TAIL(&krl->revoked_certs, rc, entry);
	debug3("%s: new CA %s", __func__, key_type(ca_key));
	*rcp = rc;
	return 0;
}

static int
insert_serial_range(struct revoked_serial_tree *rt, u_int64_t lo, u_int64_t hi)
{
	struct revoked_serial rs, *ers, *crs, *irs;

	KRL_DBG(("%s: insert %llu:%llu", __func__, lo, hi));
	bzero(&rs, sizeof(rs));
	rs.lo = lo;
	rs.hi = hi;
	ers = RB_NFIND(revoked_serial_tree, rt, &rs);
	if (ers == NULL || serial_cmp(ers, &rs) != 0) {
		/* No entry matches. Just insert */
		if ((irs = malloc(sizeof(rs))) == NULL)
			return -1;
		memcpy(irs, &rs, sizeof(*irs));
		ers = RB_INSERT(revoked_serial_tree, rt, irs);
		if (ers != NULL) {
			KRL_DBG(("%s: bad: ers != NULL", __func__));
			/* Shouldn't happen */
			free(irs);
			return -1;
		}
		ers = irs;
	} else {
		KRL_DBG(("%s: overlap found %llu:%llu", __func__,
		    ers->lo, ers->hi));
		/*
		 * The inserted entry overlaps an existing one. Grow the
		 * existing entry.
		 */
		if (ers->lo > lo)
			ers->lo = lo;
		if (ers->hi < hi)
			ers->hi = hi;
	}
	/*
	 * The inserted or revised range might overlap or abut adjacent ones;
	 * coalesce as necessary.
	 */

	/* Check predecessors */
	while ((crs = RB_PREV(revoked_serial_tree, rt, ers)) != NULL) {
		KRL_DBG(("%s: pred %llu:%llu", __func__, crs->lo, crs->hi));
		if (ers->lo != 0 && crs->hi < ers->lo - 1)
			break;
		/* This entry overlaps. */
		if (crs->lo < ers->lo) {
			ers->lo = crs->lo;
			KRL_DBG(("%s: pred extend %llu:%llu", __func__,
			    ers->lo, ers->hi));
		}
		RB_REMOVE(revoked_serial_tree, rt, crs);
		free(crs);
	}
	/* Check successors */
	while ((crs = RB_NEXT(revoked_serial_tree, rt, ers)) != NULL) {
		KRL_DBG(("%s: succ %llu:%llu", __func__, crs->lo, crs->hi));
		if (ers->hi != (u_int64_t)-1 && crs->lo > ers->hi + 1)
			break;
		/* This entry overlaps. */
		if (crs->hi > ers->hi) {
			ers->hi = crs->hi;
			KRL_DBG(("%s: succ extend %llu:%llu", __func__,
			    ers->lo, ers->hi));
		}
		RB_REMOVE(revoked_serial_tree, rt, crs);
		free(crs);
	}
	KRL_DBG(("%s: done, final %llu:%llu", __func__, ers->lo, ers->hi));
	return 0;
}

int
ssh_krl_revoke_cert_by_serial(struct ssh_krl *krl, const Key *ca_key,
    u_int64_t serial)
{
	return ssh_krl_revoke_cert_by_serial_range(krl, ca_key, serial, serial);
}

int
ssh_krl_revoke_cert_by_serial_range(struct ssh_krl *krl, const Key *ca_key,
    u_int64_t lo, u_int64_t hi)
{
	struct revoked_certs *rc;

	if (lo > hi || lo == 0)
		return -1;
	if (revoked_certs_for_ca_key(krl, ca_key, &rc, 1) != 0)
		return -1;
	return insert_serial_range(&rc->revoked_serials, lo, hi);
}

int
ssh_krl_revoke_cert_by_key_id(struct ssh_krl *krl, const Key *ca_key,
    const char *key_id)
{
	struct revoked_key_id *rki, *erki;
	struct revoked_certs *rc;

	if (revoked_certs_for_ca_key(krl, ca_key, &rc, 1) != 0)
		return -1;

	debug3("%s: revoke %s", __func__, key_id);
	if ((rki = calloc(1, sizeof(*rki))) == NULL ||
	    (rki->key_id = strdup(key_id)) == NULL) {
		free(rki);
		fatal("%s: strdup", __func__);
	}
	erki = RB_INSERT(revoked_key_id_tree, &rc->revoked_key_ids, rki);
	if (erki != NULL) {
		free(rki->key_id);
		free(rki);
	}
	return 0;
}

/* Convert "key" to a public key blob without any certificate information */
static int
plain_key_blob(const Key *key, u_char **blob, u_int *blen)
{
	Key *kcopy;
	int r;

	if ((kcopy = key_from_private(key)) == NULL)
		return -1;
	if (key_is_cert(kcopy)) {
		if (key_drop_cert(kcopy) != 0) {
			error("%s: key_drop_cert", __func__);
			key_free(kcopy);
			return -1;
		}
	}
	r = key_to_blob(kcopy, blob, blen);
	free(kcopy);
	return r == 0 ? -1 : 0;
}

/* Revoke a key blob. Ownership of blob is transferred to the tree */
static int
revoke_blob(struct revoked_blob_tree *rbt, u_char *blob, u_int len)
{
	struct revoked_blob *rb, *erb;

	if ((rb = calloc(1, sizeof(*rb))) == NULL)
		return -1;
	rb->blob = blob;
	rb->len = len;
	erb = RB_INSERT(revoked_blob_tree, rbt, rb);
	if (erb != NULL) {
		free(rb->blob);
		free(rb);
	}
	return 0;
}

int
ssh_krl_revoke_key_explicit(struct ssh_krl *krl, const Key *key)
{
	u_char *blob;
	u_int len;

	debug3("%s: revoke type %s", __func__, key_type(key));
	if (plain_key_blob(key, &blob, &len) != 0)
		return -1;
	return revoke_blob(&krl->revoked_keys, blob, len);
}

int
ssh_krl_revoke_key_sha1(struct ssh_krl *krl, const Key *key)
{
	u_char *blob;
	u_int len;

	debug3("%s: revoke type %s by sha1", __func__, key_type(key));
	if ((blob = key_fingerprint_raw(key, SSH_FP_SHA1, &len)) == NULL)
		return -1;
	return revoke_blob(&krl->revoked_sha1s, blob, len);
}

int
ssh_krl_revoke_key(struct ssh_krl *krl, const Key *key)
{
	if (!key_is_cert(key))
		return ssh_krl_revoke_key_sha1(krl, key);

	if (key_cert_is_legacy(key) || key->cert->serial == 0) {
		return ssh_krl_revoke_cert_by_key_id(krl,
		    key->cert->signature_key,
		    key->cert->key_id);
	} else {
		return ssh_krl_revoke_cert_by_serial(krl,
		    key->cert->signature_key,
		    key->cert->serial);
	}
}

/*
 * Select a copact next section type to emit in a KRL based on the
 * current section type, the run length of contiguous revoked serial
 * numbers and the gaps from the last and to the next revoked serial.
 * Applies a mostly-accurate bit cost model to select the section type
 * that will minimise the size of the resultant KRL.
 */
static int
choose_next_state(int current_state, u_int64_t contig, int final,
    u_int64_t last_gap, u_int64_t next_gap, int *force_new_section)
{
	int new_state;
	u_int64_t cost, cost_list, cost_range, cost_bitmap, cost_bitmap_restart;

	/*
	 * Avoid unsigned overflows.
	 * The limits are high enough to avoid confusing the calculations.
	 */
	contig = MIN(contig, 1ULL<<31);
	last_gap = MIN(last_gap, 1ULL<<31);
	next_gap = MIN(next_gap, 1ULL<<31);

	/*
	 * Calculate the cost to switch from the current state to candidates.
	 * NB. range sections only ever contain a single range, so their
	 * switching cost is independent of the current_state.
	 */
	cost_list = cost_bitmap = cost_bitmap_restart = 0;
	cost_range = 8;
	switch (current_state) {
	case KRL_SECTION_CERT_SERIAL_LIST:
		cost_bitmap_restart = cost_bitmap = 8 + 64;
		break;
	case KRL_SECTION_CERT_SERIAL_BITMAP:
		cost_list = 8;
		cost_bitmap_restart = 8 + 64;
		break;
	case KRL_SECTION_CERT_SERIAL_RANGE:
	case 0:
		cost_bitmap_restart = cost_bitmap = 8 + 64;
		cost_list = 8;
	}

	/* Estimate base cost in bits of each section type */
	cost_list += 64 * contig + (final ? 0 : 8+64);
	cost_range += (2 * 64) + (final ? 0 : 8+64);
	cost_bitmap += last_gap + contig + (final ? 0 : MIN(next_gap, 8+64));
	cost_bitmap_restart += contig + (final ? 0 : MIN(next_gap, 8+64));

	/* Convert to byte costs for actual comparison */
	cost_list = (cost_list + 7) / 8;
	cost_bitmap = (cost_bitmap + 7) / 8;
	cost_bitmap_restart = (cost_bitmap_restart + 7) / 8;
	cost_range = (cost_range + 7) / 8;

	/* Now pick the best choice */
	*force_new_section = 0;
	new_state = KRL_SECTION_CERT_SERIAL_BITMAP;
	cost = cost_bitmap;
	if (cost_range < cost) {
		new_state = KRL_SECTION_CERT_SERIAL_RANGE;
		cost = cost_range;
	}
	if (cost_list < cost) {
		new_state = KRL_SECTION_CERT_SERIAL_LIST;
		cost = cost_list;
	}
	if (cost_bitmap_restart < cost) {
		new_state = KRL_SECTION_CERT_SERIAL_BITMAP;
		*force_new_section = 1;
		cost = cost_bitmap_restart;
	}
	debug3("%s: contig %llu last_gap %llu next_gap %llu final %d, costs:"
	    "list %llu range %llu bitmap %llu new bitmap %llu, "
	    "selected 0x%02x%s", __func__, (unsigned long long)contig,
	    (unsigned long long)last_gap, (unsigned long long)next_gap, final,
	    (unsigned long long)cost_list, (unsigned long long)cost_range,
	    (unsigned long long)cost_bitmap,
	    (unsigned long long)cost_bitmap_restart, new_state,
	    *force_new_section ? " restart" : "");
	return new_state;
}

/* Generate a KRL_SECTION_CERTIFICATES KRL section */
static int
revoked_certs_generate(struct revoked_certs *rc, Buffer *buf)
{
	int final, force_new_sect, r = -1;
	u_int64_t i, contig, gap, last = 0, bitmap_start = 0;
	struct revoked_serial *rs, *nrs;
	struct revoked_key_id *rki;
	int next_state, state = 0;
	Buffer sect;
	u_char *kblob = NULL;
	u_int klen;
	BIGNUM *bitmap = NULL;

	/* Prepare CA scope key blob if we have one supplied */
	if (key_to_blob(rc->ca_key, &kblob, &klen) == 0)
		return -1;

	buffer_init(&sect);

	/* Store the header */
	buffer_put_string(buf, kblob, klen);
	buffer_put_string(buf, NULL, 0); /* Reserved */

	free(kblob);

	/* Store the revoked serials.  */
	for (rs = RB_MIN(revoked_serial_tree, &rc->revoked_serials);
	     rs != NULL;
	     rs = RB_NEXT(revoked_serial_tree, &rc->revoked_serials, rs)) {
		debug3("%s: serial %llu:%llu state 0x%02x", __func__,
		    (unsigned long long)rs->lo, (unsigned long long)rs->hi,
		    state);

		/* Check contiguous length and gap to next section (if any) */
		nrs = RB_NEXT(revoked_serial_tree, &rc->revoked_serials, rs);
		final = nrs == NULL;
		gap = nrs == NULL ? 0 : nrs->lo - rs->hi;
		contig = 1 + (rs->hi - rs->lo);

		/* Choose next state based on these */
		next_state = choose_next_state(state, contig, final,
		    state == 0 ? 0 : rs->lo - last, gap, &force_new_sect);

		/*
		 * If the current section is a range section or has a different
		 * type to the next section, then finish it off now.
		 */
		if (state != 0 && (force_new_sect || next_state != state ||
		    state == KRL_SECTION_CERT_SERIAL_RANGE)) {
			debug3("%s: finish state 0x%02x", __func__, state);
			switch (state) {
			case KRL_SECTION_CERT_SERIAL_LIST:
			case KRL_SECTION_CERT_SERIAL_RANGE:
				break;
			case KRL_SECTION_CERT_SERIAL_BITMAP:
				buffer_put_bignum2(&sect, bitmap);
				BN_free(bitmap);
				bitmap = NULL;
				break;
			}
			buffer_put_char(buf, state);
			buffer_put_string(buf,
			    buffer_ptr(&sect), buffer_len(&sect));
		}

		/* If we are starting a new section then prepare it now */
		if (next_state != state || force_new_sect) {
			debug3("%s: start state 0x%02x", __func__, next_state);
			state = next_state;
			buffer_clear(&sect);
			switch (state) {
			case KRL_SECTION_CERT_SERIAL_LIST:
			case KRL_SECTION_CERT_SERIAL_RANGE:
				break;
			case KRL_SECTION_CERT_SERIAL_BITMAP:
				if ((bitmap = BN_new()) == NULL)
					goto out;
				bitmap_start = rs->lo;
				buffer_put_int64(&sect, bitmap_start);
				break;
			}
		}

		/* Perform section-specific processing */
		switch (state) {
		case KRL_SECTION_CERT_SERIAL_LIST:
			for (i = 0; i < contig; i++)
				buffer_put_int64(&sect, rs->lo + i);
			break;
		case KRL_SECTION_CERT_SERIAL_RANGE:
			buffer_put_int64(&sect, rs->lo);
			buffer_put_int64(&sect, rs->hi);
			break;
		case KRL_SECTION_CERT_SERIAL_BITMAP:
			if (rs->lo - bitmap_start > INT_MAX) {
				error("%s: insane bitmap gap", __func__);
				goto out;
			}
			for (i = 0; i < contig; i++) {
				if (BN_set_bit(bitmap,
				    rs->lo + i - bitmap_start) != 1)
					goto out;
			}
			break;
		}
		last = rs->hi;
	}
	/* Flush the remaining section, if any */
	if (state != 0) {
		debug3("%s: serial final flush for state 0x%02x",
		    __func__, state);
		switch (state) {
		case KRL_SECTION_CERT_SERIAL_LIST:
		case KRL_SECTION_CERT_SERIAL_RANGE:
			break;
		case KRL_SECTION_CERT_SERIAL_BITMAP:
			buffer_put_bignum2(&sect, bitmap);
			BN_free(bitmap);
			bitmap = NULL;
			break;
		}
		buffer_put_char(buf, state);
		buffer_put_string(buf,
		    buffer_ptr(&sect), buffer_len(&sect));
	}
	debug3("%s: serial done ", __func__);

	/* Now output a section for any revocations by key ID */
	buffer_clear(&sect);
	RB_FOREACH(rki, revoked_key_id_tree, &rc->revoked_key_ids) {
		debug3("%s: key ID %s", __func__, rki->key_id);
		buffer_put_cstring(&sect, rki->key_id);
	}
	if (buffer_len(&sect) != 0) {
		buffer_put_char(buf, KRL_SECTION_CERT_KEY_ID);
		buffer_put_string(buf, buffer_ptr(&sect),
		    buffer_len(&sect));
	}
	r = 0;
 out:
	if (bitmap != NULL)
		BN_free(bitmap);
	buffer_free(&sect);
	return r;
}

int
ssh_krl_to_blob(struct ssh_krl *krl, Buffer *buf, const Key **sign_keys,
    u_int nsign_keys)
{
	int r = -1;
	struct revoked_certs *rc;
	struct revoked_blob *rb;
	Buffer sect;
	u_char *kblob = NULL, *sblob = NULL;
	u_int klen, slen, i;

	if (krl->generated_date == 0)
		krl->generated_date = time(NULL);

	buffer_init(&sect);

	/* Store the header */
	buffer_append(buf, KRL_MAGIC, sizeof(KRL_MAGIC) - 1);
	buffer_put_int(buf, KRL_FORMAT_VERSION);
	buffer_put_int64(buf, krl->krl_version);
	buffer_put_int64(buf, krl->generated_date);
	buffer_put_int64(buf, krl->flags);
	buffer_put_string(buf, NULL, 0);
	buffer_put_cstring(buf, krl->comment ? krl->comment : "");

	/* Store sections for revoked certificates */
	TAILQ_FOREACH(rc, &krl->revoked_certs, entry) {
		if (revoked_certs_generate(rc, &sect) != 0)
			goto out;
		buffer_put_char(buf, KRL_SECTION_CERTIFICATES);
		buffer_put_string(buf, buffer_ptr(&sect),
		    buffer_len(&sect));
	}

	/* Finally, output sections for revocations by public key/hash */
	buffer_clear(&sect);
	RB_FOREACH(rb, revoked_blob_tree, &krl->revoked_keys) {
		debug3("%s: key len %u ", __func__, rb->len);
		buffer_put_string(&sect, rb->blob, rb->len);
	}
	if (buffer_len(&sect) != 0) {
		buffer_put_char(buf, KRL_SECTION_EXPLICIT_KEY);
		buffer_put_string(buf, buffer_ptr(&sect),
		    buffer_len(&sect));
	}
	buffer_clear(&sect);
	RB_FOREACH(rb, revoked_blob_tree, &krl->revoked_sha1s) {
		debug3("%s: hash len %u ", __func__, rb->len);
		buffer_put_string(&sect, rb->blob, rb->len);
	}
	if (buffer_len(&sect) != 0) {
		buffer_put_char(buf, KRL_SECTION_FINGERPRINT_SHA1);
		buffer_put_string(buf, buffer_ptr(&sect),
		    buffer_len(&sect));
	}

	for (i = 0; i < nsign_keys; i++) {
		if (key_to_blob(sign_keys[i], &kblob, &klen) == 0)
			goto out;

		debug3("%s: signature key len %u", __func__, klen);
		buffer_put_char(buf, KRL_SECTION_SIGNATURE);
		buffer_put_string(buf, kblob, klen);

		if (key_sign(sign_keys[i], &sblob, &slen,
		    buffer_ptr(buf), buffer_len(buf)) == -1)
			goto out;
		debug3("%s: signature sig len %u", __func__, slen);
		buffer_put_string(buf, sblob, slen);
	}

	r = 0;
 out:
	free(kblob);
	free(sblob);
	buffer_free(&sect);
	return r;
}

static void
format_timestamp(u_int64_t timestamp, char *ts, size_t nts)
{
	time_t t;
	struct tm *tm;

	t = timestamp;
	tm = localtime(&t);
	*ts = '\0';
	strftime(ts, nts, "%Y%m%dT%H%M%S", tm);
}

static int
parse_revoked_certs(Buffer *buf, struct ssh_krl *krl)
{
	int ret = -1, nbits;
	u_char type, *blob;
	u_int blen;
	Buffer subsect;
	u_int64_t serial, serial_lo, serial_hi;
	BIGNUM *bitmap = NULL;
	char *key_id = NULL;
	Key *ca_key = NULL;

	buffer_init(&subsect);

	if ((blob = buffer_get_string_ptr_ret(buf, &blen)) == NULL ||
	    buffer_get_string_ptr_ret(buf, NULL) == NULL) { /* reserved */
		error("%s: buffer error", __func__);
		goto out;
	}
	if ((ca_key = key_from_blob(blob, blen)) == NULL)
		goto out;

	while (buffer_len(buf) > 0) {
		if (buffer_get_char_ret(&type, buf) != 0 ||
		    (blob = buffer_get_string_ptr_ret(buf, &blen)) == NULL) {
			error("%s: buffer error", __func__);
			goto out;
		}
		buffer_clear(&subsect);
		buffer_append(&subsect, blob, blen);
		debug3("%s: subsection type 0x%02x", __func__, type);
		/* buffer_dump(&subsect); */

		switch (type) {
		case KRL_SECTION_CERT_SERIAL_LIST:
			while (buffer_len(&subsect) > 0) {
				if (buffer_get_int64_ret(&serial,
				    &subsect) != 0) {
					error("%s: buffer error", __func__);
					goto out;
				}
				if (ssh_krl_revoke_cert_by_serial(krl, ca_key,
				    serial) != 0) {
					error("%s: update failed", __func__);
					goto out;
				}
			}
			break;
		case KRL_SECTION_CERT_SERIAL_RANGE:
			if (buffer_get_int64_ret(&serial_lo, &subsect) != 0 ||
			    buffer_get_int64_ret(&serial_hi, &subsect) != 0) {
				error("%s: buffer error", __func__);
				goto out;
			}
			if (ssh_krl_revoke_cert_by_serial_range(krl, ca_key,
			    serial_lo, serial_hi) != 0) {
				error("%s: update failed", __func__);
				goto out;
			}
			break;
		case KRL_SECTION_CERT_SERIAL_BITMAP:
			if ((bitmap = BN_new()) == NULL) {
				error("%s: BN_new", __func__);
				goto out;
			}
			if (buffer_get_int64_ret(&serial_lo, &subsect) != 0 ||
			    buffer_get_bignum2_ret(&subsect, bitmap) != 0) {
				error("%s: buffer error", __func__);
				goto out;
			}
			if ((nbits = BN_num_bits(bitmap)) < 0) {
				error("%s: bitmap bits < 0", __func__);
				goto out;
			}
			for (serial = 0; serial < (u_int)nbits; serial++) {
				if (serial > 0 && serial_lo + serial == 0) {
					error("%s: bitmap wraps u64", __func__);
					goto out;
				}
				if (!BN_is_bit_set(bitmap, serial))
					continue;
				if (ssh_krl_revoke_cert_by_serial(krl, ca_key,
				    serial_lo + serial) != 0) {
					error("%s: update failed", __func__);
					goto out;
				}
			}
			BN_free(bitmap);
			bitmap = NULL;
			break;
		case KRL_SECTION_CERT_KEY_ID:
			while (buffer_len(&subsect) > 0) {
				if ((key_id = buffer_get_cstring_ret(&subsect,
				    NULL)) == NULL) {
					error("%s: buffer error", __func__);
					goto out;
				}
				if (ssh_krl_revoke_cert_by_key_id(krl, ca_key,
				    key_id) != 0) {
					error("%s: update failed", __func__);
					goto out;
				}
				free(key_id);
				key_id = NULL;
			}
			break;
		default:
			error("Unsupported KRL certificate section %u", type);
			goto out;
		}
		if (buffer_len(&subsect) > 0) {
			error("KRL certificate section contains unparsed data");
			goto out;
		}
	}

	ret = 0;
 out:
	if (ca_key != NULL)
		key_free(ca_key);
	if (bitmap != NULL)
		BN_free(bitmap);
	free(key_id);
	buffer_free(&subsect);
	return ret;
}


/* Attempt to parse a KRL, checking its signature (if any) with sign_ca_keys. */
int
ssh_krl_from_blob(Buffer *buf, struct ssh_krl **krlp,
    const Key **sign_ca_keys, u_int nsign_ca_keys)
{
	Buffer copy, sect;
	struct ssh_krl *krl;
	char timestamp[64];
	int ret = -1, r, sig_seen;
	Key *key = NULL, **ca_used = NULL;
	u_char type, *blob;
	u_int i, j, sig_off, sects_off, blen, format_version, nca_used = 0;

	*krlp = NULL;
	if (buffer_len(buf) < sizeof(KRL_MAGIC) - 1 ||
	    memcmp(buffer_ptr(buf), KRL_MAGIC, sizeof(KRL_MAGIC) - 1) != 0) {
		debug3("%s: not a KRL", __func__);
		/*
		 * Return success but a NULL *krlp here to signal that the
		 * file might be a simple list of keys.
		 */
		return 0;
	}

	/* Take a copy of the KRL buffer so we can verify its signature later */
	buffer_init(&copy);
	buffer_append(&copy, buffer_ptr(buf), buffer_len(buf));

	buffer_init(&sect);
	buffer_consume(&copy, sizeof(KRL_MAGIC) - 1);

	if ((krl = ssh_krl_init()) == NULL) {
		error("%s: alloc failed", __func__);
		goto out;
	}

	if (buffer_get_int_ret(&format_version, &copy) != 0) {
		error("%s: KRL truncated", __func__);
		goto out;
	}
	if (format_version != KRL_FORMAT_VERSION) {
		error("%s: KRL unsupported format version %u",
		    __func__, format_version);
		goto out;
	}
	if (buffer_get_int64_ret(&krl->krl_version, &copy) != 0 ||
	    buffer_get_int64_ret(&krl->generated_date, &copy) != 0 ||
	    buffer_get_int64_ret(&krl->flags, &copy) != 0 ||
	    buffer_get_string_ptr_ret(&copy, NULL) == NULL || /* reserved */
	    (krl->comment = buffer_get_cstring_ret(&copy, NULL)) == NULL) {
		error("%s: buffer error", __func__);
		goto out;
	}

	format_timestamp(krl->generated_date, timestamp, sizeof(timestamp));
	debug("KRL version %llu generated at %s%s%s",
	    (unsigned long long)krl->krl_version, timestamp,
	    *krl->comment ? ": " : "", krl->comment);

	/*
	 * 1st pass: verify signatures, if any. This is done to avoid
	 * detailed parsing of data whose provenance is unverified.
	 */
	sig_seen = 0;
	sects_off = buffer_len(buf) - buffer_len(&copy);
	while (buffer_len(&copy) > 0) {
		if (buffer_get_char_ret(&type, &copy) != 0 ||
		    (blob = buffer_get_string_ptr_ret(&copy, &blen)) == NULL) {
			error("%s: buffer error", __func__);
			goto out;
		}
		debug3("%s: first pass, section 0x%02x", __func__, type);
		if (type != KRL_SECTION_SIGNATURE) {
			if (sig_seen) {
				error("KRL contains non-signature section "
				    "after signature");
				goto out;
			}
			/* Not interested for now. */
			continue;
		}
		sig_seen = 1;
		/* First string component is the signing key */
		if ((key = key_from_blob(blob, blen)) == NULL) {
			error("%s: invalid signature key", __func__);
			goto out;
		}
		sig_off = buffer_len(buf) - buffer_len(&copy);
		/* Second string component is the signature itself */
		if ((blob = buffer_get_string_ptr_ret(&copy, &blen)) == NULL) {
			error("%s: buffer error", __func__);
			goto out;
		}
		/* Check signature over entire KRL up to this point */
		if (key_verify(key, blob, blen,
		    buffer_ptr(buf), buffer_len(buf) - sig_off) == -1) {
			error("bad signaure on KRL");
			goto out;
		}
		/* Check if this key has already signed this KRL */
		for (i = 0; i < nca_used; i++) {
			if (key_equal(ca_used[i], key)) {
				error("KRL signed more than once with "
				    "the same key");
				goto out;
			}
		}
		/* Record keys used to sign the KRL */
		ca_used = xrealloc(ca_used, nca_used + 1, sizeof(*ca_used));
		ca_used[nca_used++] = key;
		key = NULL;
		break;
	}

	/*
	 * 2nd pass: parse and load the KRL, skipping the header to the point
	 * where the section start.
	 */
	buffer_append(&copy, (u_char*)buffer_ptr(buf) + sects_off,
	    buffer_len(buf) - sects_off);
	while (buffer_len(&copy) > 0) {
		if (buffer_get_char_ret(&type, &copy) != 0 ||
		    (blob = buffer_get_string_ptr_ret(&copy, &blen)) == NULL) {
			error("%s: buffer error", __func__);
			goto out;
		}
		debug3("%s: second pass, section 0x%02x", __func__, type);
		buffer_clear(&sect);
		buffer_append(&sect, blob, blen);

		switch (type) {
		case KRL_SECTION_CERTIFICATES:
			if ((r = parse_revoked_certs(&sect, krl)) != 0)
				goto out;
			break;
		case KRL_SECTION_EXPLICIT_KEY:
		case KRL_SECTION_FINGERPRINT_SHA1:
			while (buffer_len(&sect) > 0) {
				if ((blob = buffer_get_string_ret(&sect,
				    &blen)) == NULL) {
					error("%s: buffer error", __func__);
					goto out;
				}
				if (type == KRL_SECTION_FINGERPRINT_SHA1 &&
				    blen != 20) {
					error("%s: bad SHA1 length", __func__);
					goto out;
				}
				if (revoke_blob(
				    type == KRL_SECTION_EXPLICIT_KEY ?
				    &krl->revoked_keys : &krl->revoked_sha1s,
				    blob, blen) != 0)
					goto out; /* revoke_blob frees blob */
			}
			break;
		case KRL_SECTION_SIGNATURE:
			/* Handled above, but still need to stay in synch */
			buffer_clear(&sect);
			if ((blob = buffer_get_string_ptr_ret(&copy,
			    &blen)) == NULL) {
				error("%s: buffer error", __func__);
				goto out;
			}
			break;
		default:
			error("Unsupported KRL section %u", type);
			goto out;
		}
		if (buffer_len(&sect) > 0) {
			error("KRL section contains unparsed data");
			goto out;
		}
	}

	/* Check that the key(s) used to sign the KRL weren't revoked */
	sig_seen = 0;
	for (i = 0; i < nca_used; i++) {
		if (ssh_krl_check_key(krl, ca_used[i]) == 0)
			sig_seen = 1;
		else {
			key_free(ca_used[i]);
			ca_used[i] = NULL;
		}
	}
	if (nca_used && !sig_seen) {
		error("All keys used to sign KRL were revoked");
		goto out;
	}

	/* If we have CA keys, then verify that one was used to sign the KRL */
	if (sig_seen && nsign_ca_keys != 0) {
		sig_seen = 0;
		for (i = 0; !sig_seen && i < nsign_ca_keys; i++) {
			for (j = 0; j < nca_used; j++) {
				if (ca_used[j] == NULL)
					continue;
				if (key_equal(ca_used[j], sign_ca_keys[i])) {
					sig_seen = 1;
					break;
				}
			}
		}
		if (!sig_seen) {
			error("KRL not signed with any trusted key");
			goto out;
		}
	}

	*krlp = krl;
	ret = 0;
 out:
	if (ret != 0)
		ssh_krl_free(krl);
	for (i = 0; i < nca_used; i++) {
		if (ca_used[i] != NULL)
			key_free(ca_used[i]);
	}
	free(ca_used);
	if (key != NULL)
		key_free(key);
	buffer_free(&copy);
	buffer_free(&sect);
	return ret;
}

/* Checks whether a given key/cert is revoked. Does not check its CA */
static int
is_key_revoked(struct ssh_krl *krl, const Key *key)
{
	struct revoked_blob rb, *erb;
	struct revoked_serial rs, *ers;
	struct revoked_key_id rki, *erki;
	struct revoked_certs *rc;

	/* Check explicitly revoked hashes first */
	bzero(&rb, sizeof(rb));
	if ((rb.blob = key_fingerprint_raw(key, SSH_FP_SHA1, &rb.len)) == NULL)
		return -1;
	erb = RB_FIND(revoked_blob_tree, &krl->revoked_sha1s, &rb);
	free(rb.blob);
	if (erb != NULL) {
		debug("%s: revoked by key SHA1", __func__);
		return -1;
	}

	/* Next, explicit keys */
	bzero(&rb, sizeof(rb));
	if (plain_key_blob(key, &rb.blob, &rb.len) != 0)
		return -1;
	erb = RB_FIND(revoked_blob_tree, &krl->revoked_keys, &rb);
	free(rb.blob);
	if (erb != NULL) {
		debug("%s: revoked by explicit key", __func__);
		return -1;
	}

	if (!key_is_cert(key))
		return 0;

	/* Check cert revocation */
	if (revoked_certs_for_ca_key(krl, key->cert->signature_key,
	    &rc, 0) != 0)
		return -1;
	if (rc == NULL)
		return 0; /* No entry for this CA */

	/* Check revocation by cert key ID */
	bzero(&rki, sizeof(rki));
	rki.key_id = key->cert->key_id;
	erki = RB_FIND(revoked_key_id_tree, &rc->revoked_key_ids, &rki);
	if (erki != NULL) {
		debug("%s: revoked by key ID", __func__);
		return -1;
	}

	/*
	 * Legacy cert formats lack serial numbers. Zero serials numbers
	 * are ignored (it's the default when the CA doesn't specify one).
	 */
	if (key_cert_is_legacy(key) || key->cert->serial == 0)
		return 0;

	bzero(&rs, sizeof(rs));
	rs.lo = rs.hi = key->cert->serial;
	ers = RB_FIND(revoked_serial_tree, &rc->revoked_serials, &rs);
	if (ers != NULL) {
		KRL_DBG(("%s: %llu matched %llu:%llu", __func__,
		    key->cert->serial, ers->lo, ers->hi));
		debug("%s: revoked by serial", __func__);
		return -1;
	}
	KRL_DBG(("%s: %llu no match", __func__, key->cert->serial));

	return 0;
}

int
ssh_krl_check_key(struct ssh_krl *krl, const Key *key)
{
	int r;

	debug2("%s: checking key", __func__);
	if ((r = is_key_revoked(krl, key)) != 0)
		return r;
	if (key_is_cert(key)) {
		debug2("%s: checking CA key", __func__);
		if ((r = is_key_revoked(krl, key->cert->signature_key)) != 0)
			return r;
	}
	debug3("%s: key okay", __func__);
	return 0;
}

/* Returns 0 on success, -1 on error or key revoked, -2 if path is not a KRL */
int
ssh_krl_file_contains_key(const char *path, const Key *key)
{
	Buffer krlbuf;
	struct ssh_krl *krl;
	int revoked, fd;

	if (path == NULL)
		return 0;

	if ((fd = open(path, O_RDONLY)) == -1) {
		error("open %s: %s", path, strerror(errno));
		error("Revoked keys file not accessible - refusing public key "
		    "authentication");
		return -1;
	}
	buffer_init(&krlbuf);
	if (!key_load_file(fd, path, &krlbuf)) {
		close(fd);
		buffer_free(&krlbuf);
		error("Revoked keys file not readable - refusing public key "
		    "authentication");
		return -1;
	}
	close(fd);
	if (ssh_krl_from_blob(&krlbuf, &krl, NULL, 0) != 0) {
		buffer_free(&krlbuf);
		error("Invalid KRL, refusing public key "
		    "authentication");
		return -1;
	}
	buffer_free(&krlbuf);
	if (krl == NULL) {
		debug3("%s: %s is not a KRL file", __func__, path);
		return -2;
	}
	debug2("%s: checking KRL %s", __func__, path);
	revoked = ssh_krl_check_key(krl, key) != 0;
	ssh_krl_free(krl);
	return revoked ? -1 : 0;
}
