/* $OpenBSD: krl.c,v 1.60 2025/02/18 08:02:48 djm Exp $ */
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

#include "includes.h"

#include <sys/types.h>
#include <openbsd-compat/sys-tree.h>
#include <openbsd-compat/sys-queue.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sshbuf.h"
#include "ssherr.h"
#include "sshkey.h"
#include "authfile.h"
#include "misc.h"
#include "log.h"
#include "digest.h"
#include "bitmap.h"
#include "utf8.h"

#include "krl.h"

/* #define DEBUG_KRL */
#ifdef DEBUG_KRL
# define KRL_DBG(x) debug3_f x
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
RB_GENERATE_STATIC(revoked_serial_tree, revoked_serial, tree_entry, serial_cmp)

/* Tree of key IDs */
struct revoked_key_id {
	char *key_id;
	RB_ENTRY(revoked_key_id) tree_entry;
};
static int key_id_cmp(struct revoked_key_id *a, struct revoked_key_id *b);
RB_HEAD(revoked_key_id_tree, revoked_key_id);
RB_GENERATE_STATIC(revoked_key_id_tree, revoked_key_id, tree_entry, key_id_cmp)

/* Tree of blobs (used for keys and fingerprints) */
struct revoked_blob {
	u_char *blob;
	size_t len;
	RB_ENTRY(revoked_blob) tree_entry;
};
static int blob_cmp(struct revoked_blob *a, struct revoked_blob *b);
RB_HEAD(revoked_blob_tree, revoked_blob);
RB_GENERATE_STATIC(revoked_blob_tree, revoked_blob, tree_entry, blob_cmp)

/* Tracks revoked certs for a single CA */
struct revoked_certs {
	struct sshkey *ca_key;
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
	struct revoked_blob_tree revoked_sha256s;
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
		if ((r = memcmp(a->blob, b->blob, MINIMUM(a->len, b->len))) != 0)
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
	RB_INIT(&krl->revoked_sha256s);
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
	sshkey_free(rc->ca_key);
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
	RB_FOREACH_SAFE(rb, revoked_blob_tree, &krl->revoked_sha256s, trb) {
		RB_REMOVE(revoked_blob_tree, &krl->revoked_sha256s, rb);
		free(rb->blob);
		free(rb);
	}
	TAILQ_FOREACH_SAFE(rc, &krl->revoked_certs, entry, trc) {
		TAILQ_REMOVE(&krl->revoked_certs, rc, entry);
		revoked_certs_free(rc);
	}
	free(krl);
}

void
ssh_krl_set_version(struct ssh_krl *krl, u_int64_t version)
{
	krl->krl_version = version;
}

int
ssh_krl_set_comment(struct ssh_krl *krl, const char *comment)
{
	free(krl->comment);
	if ((krl->comment = strdup(comment)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	return 0;
}

/*
 * Find the revoked_certs struct for a CA key. If allow_create is set then
 * create a new one in the tree if one did not exist already.
 */
static int
revoked_certs_for_ca_key(struct ssh_krl *krl, const struct sshkey *ca_key,
    struct revoked_certs **rcp, int allow_create)
{
	struct revoked_certs *rc;
	int r;

	*rcp = NULL;
	TAILQ_FOREACH(rc, &krl->revoked_certs, entry) {
		if ((ca_key == NULL && rc->ca_key == NULL) ||
		    sshkey_equal(rc->ca_key, ca_key)) {
			*rcp = rc;
			return 0;
		}
	}
	if (!allow_create)
		return 0;
	/* If this CA doesn't exist in the list then add it now */
	if ((rc = calloc(1, sizeof(*rc))) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (ca_key == NULL)
		rc->ca_key = NULL;
	else if ((r = sshkey_from_private(ca_key, &rc->ca_key)) != 0) {
		free(rc);
		return r;
	}
	RB_INIT(&rc->revoked_serials);
	RB_INIT(&rc->revoked_key_ids);
	TAILQ_INSERT_TAIL(&krl->revoked_certs, rc, entry);
	KRL_DBG(("new CA %s", ca_key == NULL ? "*" : sshkey_type(ca_key)));
	*rcp = rc;
	return 0;
}

static int
insert_serial_range(struct revoked_serial_tree *rt, u_int64_t lo, u_int64_t hi)
{
	struct revoked_serial rs, *ers, *crs, *irs;

	KRL_DBG(("insert %llu:%llu", lo, hi));
	memset(&rs, 0, sizeof(rs));
	rs.lo = lo;
	rs.hi = hi;
	ers = RB_NFIND(revoked_serial_tree, rt, &rs);
	if (ers == NULL || serial_cmp(ers, &rs) != 0) {
		/* No entry matches. Just insert */
		if ((irs = malloc(sizeof(rs))) == NULL)
			return SSH_ERR_ALLOC_FAIL;
		memcpy(irs, &rs, sizeof(*irs));
		ers = RB_INSERT(revoked_serial_tree, rt, irs);
		if (ers != NULL) {
			KRL_DBG(("bad: ers != NULL"));
			/* Shouldn't happen */
			free(irs);
			return SSH_ERR_INTERNAL_ERROR;
		}
		ers = irs;
	} else {
		KRL_DBG(("overlap found %llu:%llu", ers->lo, ers->hi));
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
		KRL_DBG(("pred %llu:%llu", crs->lo, crs->hi));
		if (ers->lo != 0 && crs->hi < ers->lo - 1)
			break;
		/* This entry overlaps. */
		if (crs->lo < ers->lo) {
			ers->lo = crs->lo;
			KRL_DBG(("pred extend %llu:%llu", ers->lo, ers->hi));
		}
		RB_REMOVE(revoked_serial_tree, rt, crs);
		free(crs);
	}
	/* Check successors */
	while ((crs = RB_NEXT(revoked_serial_tree, rt, ers)) != NULL) {
		KRL_DBG(("succ %llu:%llu", crs->lo, crs->hi));
		if (ers->hi != (u_int64_t)-1 && crs->lo > ers->hi + 1)
			break;
		/* This entry overlaps. */
		if (crs->hi > ers->hi) {
			ers->hi = crs->hi;
			KRL_DBG(("succ extend %llu:%llu", ers->lo, ers->hi));
		}
		RB_REMOVE(revoked_serial_tree, rt, crs);
		free(crs);
	}
	KRL_DBG(("done, final %llu:%llu", ers->lo, ers->hi));
	return 0;
}

int
ssh_krl_revoke_cert_by_serial(struct ssh_krl *krl, const struct sshkey *ca_key,
    u_int64_t serial)
{
	return ssh_krl_revoke_cert_by_serial_range(krl, ca_key, serial, serial);
}

int
ssh_krl_revoke_cert_by_serial_range(struct ssh_krl *krl,
    const struct sshkey *ca_key, u_int64_t lo, u_int64_t hi)
{
	struct revoked_certs *rc;
	int r;

	if (lo > hi || lo == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = revoked_certs_for_ca_key(krl, ca_key, &rc, 1)) != 0)
		return r;
	return insert_serial_range(&rc->revoked_serials, lo, hi);
}

int
ssh_krl_revoke_cert_by_key_id(struct ssh_krl *krl, const struct sshkey *ca_key,
    const char *key_id)
{
	struct revoked_key_id *rki, *erki;
	struct revoked_certs *rc;
	int r;

	if ((r = revoked_certs_for_ca_key(krl, ca_key, &rc, 1)) != 0)
		return r;

	KRL_DBG(("revoke %s", key_id));
	if ((rki = calloc(1, sizeof(*rki))) == NULL ||
	    (rki->key_id = strdup(key_id)) == NULL) {
		free(rki);
		return SSH_ERR_ALLOC_FAIL;
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
plain_key_blob(const struct sshkey *key, u_char **blob, size_t *blen)
{
	struct sshkey *kcopy;
	int r;

	if ((r = sshkey_from_private(key, &kcopy)) != 0)
		return r;
	if (sshkey_is_cert(kcopy)) {
		if ((r = sshkey_drop_cert(kcopy)) != 0) {
			sshkey_free(kcopy);
			return r;
		}
	}
	r = sshkey_to_blob(kcopy, blob, blen);
	sshkey_free(kcopy);
	return r;
}

/* Revoke a key blob. Ownership of blob is transferred to the tree */
static int
revoke_blob(struct revoked_blob_tree *rbt, u_char *blob, size_t len)
{
	struct revoked_blob *rb, *erb;

	if ((rb = calloc(1, sizeof(*rb))) == NULL)
		return SSH_ERR_ALLOC_FAIL;
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
ssh_krl_revoke_key_explicit(struct ssh_krl *krl, const struct sshkey *key)
{
	u_char *blob;
	size_t len;
	int r;

	debug3_f("revoke type %s", sshkey_type(key));
	if ((r = plain_key_blob(key, &blob, &len)) != 0)
		return r;
	return revoke_blob(&krl->revoked_keys, blob, len);
}

static int
revoke_by_hash(struct revoked_blob_tree *target, const u_char *p, size_t len)
{
	u_char *blob;
	int r;

	/* need to copy hash, as revoke_blob steals ownership */
	if ((blob = malloc(len)) == NULL)
		return SSH_ERR_SYSTEM_ERROR;
	memcpy(blob, p, len);
	if ((r = revoke_blob(target, blob, len)) != 0) {
		free(blob);
		return r;
	}
	return 0;
}

int
ssh_krl_revoke_key_sha1(struct ssh_krl *krl, const u_char *p, size_t len)
{
	debug3_f("revoke by sha1");
	if (len != 20)
		return SSH_ERR_INVALID_FORMAT;
	return revoke_by_hash(&krl->revoked_sha1s, p, len);
}

int
ssh_krl_revoke_key_sha256(struct ssh_krl *krl, const u_char *p, size_t len)
{
	debug3_f("revoke by sha256");
	if (len != 32)
		return SSH_ERR_INVALID_FORMAT;
	return revoke_by_hash(&krl->revoked_sha256s, p, len);
}

int
ssh_krl_revoke_key(struct ssh_krl *krl, const struct sshkey *key)
{
	/* XXX replace with SHA256? */
	if (!sshkey_is_cert(key))
		return ssh_krl_revoke_key_explicit(krl, key);

	if (key->cert->serial == 0) {
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
 * Select the most compact section type to emit next in a KRL based on
 * the current section type, the run length of contiguous revoked serial
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
	contig = MINIMUM(contig, 1ULL<<31);
	last_gap = MINIMUM(last_gap, 1ULL<<31);
	next_gap = MINIMUM(next_gap, 1ULL<<31);

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
	cost_bitmap += last_gap + contig + (final ? 0 : MINIMUM(next_gap, 8+64));
	cost_bitmap_restart += contig + (final ? 0 : MINIMUM(next_gap, 8+64));

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
	KRL_DBG(("contig %llu last_gap %llu next_gap %llu final %d, costs:"
	    "list %llu range %llu bitmap %llu new bitmap %llu, "
	    "selected 0x%02x%s", (long long unsigned)contig,
	    (long long unsigned)last_gap, (long long unsigned)next_gap, final,
	    (long long unsigned)cost_list, (long long unsigned)cost_range,
	    (long long unsigned)cost_bitmap,
	    (long long unsigned)cost_bitmap_restart, new_state,
	    *force_new_section ? " restart" : ""));
	return new_state;
}

static int
put_bitmap(struct sshbuf *buf, struct bitmap *bitmap)
{
	size_t len;
	u_char *blob;
	int r;

	len = bitmap_nbytes(bitmap);
	if ((blob = malloc(len)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (bitmap_to_string(bitmap, blob, len) != 0) {
		free(blob);
		return SSH_ERR_INTERNAL_ERROR;
	}
	r = sshbuf_put_bignum2_bytes(buf, blob, len);
	free(blob);
	return r;
}

/* Generate a KRL_SECTION_CERTIFICATES KRL section */
static int
revoked_certs_generate(struct revoked_certs *rc, struct sshbuf *buf)
{
	int final, force_new_sect, r = SSH_ERR_INTERNAL_ERROR;
	u_int64_t i, contig, gap, last = 0, bitmap_start = 0;
	struct revoked_serial *rs, *nrs;
	struct revoked_key_id *rki;
	int next_state, state = 0;
	struct sshbuf *sect;
	struct bitmap *bitmap = NULL;

	if ((sect = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	/* Store the header: optional CA scope key, reserved */
	if (rc->ca_key == NULL) {
		if ((r = sshbuf_put_string(buf, NULL, 0)) != 0)
			goto out;
	} else {
		if ((r = sshkey_puts(rc->ca_key, buf)) != 0)
			goto out;
	}
	if ((r = sshbuf_put_string(buf, NULL, 0)) != 0)
		goto out;

	/* Store the revoked serials.  */
	for (rs = RB_MIN(revoked_serial_tree, &rc->revoked_serials);
	     rs != NULL;
	     rs = RB_NEXT(revoked_serial_tree, &rc->revoked_serials, rs)) {
		KRL_DBG(("serial %llu:%llu state 0x%02x",
		    (long long unsigned)rs->lo, (long long unsigned)rs->hi,
		    state));

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
			KRL_DBG(("finish state 0x%02x", state));
			switch (state) {
			case KRL_SECTION_CERT_SERIAL_LIST:
			case KRL_SECTION_CERT_SERIAL_RANGE:
				break;
			case KRL_SECTION_CERT_SERIAL_BITMAP:
				if ((r = put_bitmap(sect, bitmap)) != 0)
					goto out;
				bitmap_free(bitmap);
				bitmap = NULL;
				break;
			}
			if ((r = sshbuf_put_u8(buf, state)) != 0 ||
			    (r = sshbuf_put_stringb(buf, sect)) != 0)
				goto out;
			sshbuf_reset(sect);
		}

		/* If we are starting a new section then prepare it now */
		if (next_state != state || force_new_sect) {
			KRL_DBG(("start state 0x%02x",
			    next_state));
			state = next_state;
			sshbuf_reset(sect);
			switch (state) {
			case KRL_SECTION_CERT_SERIAL_LIST:
			case KRL_SECTION_CERT_SERIAL_RANGE:
				break;
			case KRL_SECTION_CERT_SERIAL_BITMAP:
				if ((bitmap = bitmap_new()) == NULL) {
					r = SSH_ERR_ALLOC_FAIL;
					goto out;
				}
				bitmap_start = rs->lo;
				if ((r = sshbuf_put_u64(sect,
				    bitmap_start)) != 0)
					goto out;
				break;
			}
		}

		/* Perform section-specific processing */
		switch (state) {
		case KRL_SECTION_CERT_SERIAL_LIST:
			for (i = 0; i < contig; i++) {
				if ((r = sshbuf_put_u64(sect, rs->lo + i)) != 0)
					goto out;
			}
			break;
		case KRL_SECTION_CERT_SERIAL_RANGE:
			if ((r = sshbuf_put_u64(sect, rs->lo)) != 0 ||
			    (r = sshbuf_put_u64(sect, rs->hi)) != 0)
				goto out;
			break;
		case KRL_SECTION_CERT_SERIAL_BITMAP:
			if (rs->lo - bitmap_start > INT_MAX) {
				r = SSH_ERR_INVALID_FORMAT;
				error_f("insane bitmap gap");
				goto out;
			}
			for (i = 0; i < contig; i++) {
				if (bitmap_set_bit(bitmap,
				    rs->lo + i - bitmap_start) != 0) {
					r = SSH_ERR_ALLOC_FAIL;
					goto out;
				}
			}
			break;
		}
		last = rs->hi;
	}
	/* Flush the remaining section, if any */
	if (state != 0) {
		KRL_DBG(("serial final flush for state 0x%02x", state));
		switch (state) {
		case KRL_SECTION_CERT_SERIAL_LIST:
		case KRL_SECTION_CERT_SERIAL_RANGE:
			break;
		case KRL_SECTION_CERT_SERIAL_BITMAP:
			if ((r = put_bitmap(sect, bitmap)) != 0)
				goto out;
			bitmap_free(bitmap);
			bitmap = NULL;
			break;
		}
		if ((r = sshbuf_put_u8(buf, state)) != 0 ||
		    (r = sshbuf_put_stringb(buf, sect)) != 0)
			goto out;
	}
	KRL_DBG(("serial done "));

	/* Now output a section for any revocations by key ID */
	sshbuf_reset(sect);
	RB_FOREACH(rki, revoked_key_id_tree, &rc->revoked_key_ids) {
		KRL_DBG(("key ID %s", rki->key_id));
		if ((r = sshbuf_put_cstring(sect, rki->key_id)) != 0)
			goto out;
	}
	if (sshbuf_len(sect) != 0) {
		if ((r = sshbuf_put_u8(buf, KRL_SECTION_CERT_KEY_ID)) != 0 ||
		    (r = sshbuf_put_stringb(buf, sect)) != 0)
			goto out;
	}
	r = 0;
 out:
	bitmap_free(bitmap);
	sshbuf_free(sect);
	return r;
}

int
ssh_krl_to_blob(struct ssh_krl *krl, struct sshbuf *buf)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	struct revoked_certs *rc;
	struct revoked_blob *rb;
	struct sshbuf *sect;
	u_char *sblob = NULL;

	if (krl->generated_date == 0)
		krl->generated_date = time(NULL);

	if ((sect = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	/* Store the header */
	if ((r = sshbuf_put(buf, KRL_MAGIC, sizeof(KRL_MAGIC) - 1)) != 0 ||
	    (r = sshbuf_put_u32(buf, KRL_FORMAT_VERSION)) != 0 ||
	    (r = sshbuf_put_u64(buf, krl->krl_version)) != 0 ||
	    (r = sshbuf_put_u64(buf, krl->generated_date)) != 0 ||
	    (r = sshbuf_put_u64(buf, krl->flags)) != 0 ||
	    (r = sshbuf_put_string(buf, NULL, 0)) != 0 ||
	    (r = sshbuf_put_cstring(buf, krl->comment)) != 0)
		goto out;

	/* Store sections for revoked certificates */
	TAILQ_FOREACH(rc, &krl->revoked_certs, entry) {
		sshbuf_reset(sect);
		if ((r = revoked_certs_generate(rc, sect)) != 0)
			goto out;
		if ((r = sshbuf_put_u8(buf, KRL_SECTION_CERTIFICATES)) != 0 ||
		    (r = sshbuf_put_stringb(buf, sect)) != 0)
			goto out;
	}

	/* Finally, output sections for revocations by public key/hash */
	sshbuf_reset(sect);
	RB_FOREACH(rb, revoked_blob_tree, &krl->revoked_keys) {
		KRL_DBG(("key len %zu ", rb->len));
		if ((r = sshbuf_put_string(sect, rb->blob, rb->len)) != 0)
			goto out;
	}
	if (sshbuf_len(sect) != 0) {
		if ((r = sshbuf_put_u8(buf, KRL_SECTION_EXPLICIT_KEY)) != 0 ||
		    (r = sshbuf_put_stringb(buf, sect)) != 0)
			goto out;
	}
	sshbuf_reset(sect);
	RB_FOREACH(rb, revoked_blob_tree, &krl->revoked_sha1s) {
		KRL_DBG(("hash len %zu ", rb->len));
		if ((r = sshbuf_put_string(sect, rb->blob, rb->len)) != 0)
			goto out;
	}
	if (sshbuf_len(sect) != 0) {
		if ((r = sshbuf_put_u8(buf,
		    KRL_SECTION_FINGERPRINT_SHA1)) != 0 ||
		    (r = sshbuf_put_stringb(buf, sect)) != 0)
			goto out;
	}
	sshbuf_reset(sect);
	RB_FOREACH(rb, revoked_blob_tree, &krl->revoked_sha256s) {
		KRL_DBG(("hash len %zu ", rb->len));
		if ((r = sshbuf_put_string(sect, rb->blob, rb->len)) != 0)
			goto out;
	}
	if (sshbuf_len(sect) != 0) {
		if ((r = sshbuf_put_u8(buf,
		    KRL_SECTION_FINGERPRINT_SHA256)) != 0 ||
		    (r = sshbuf_put_stringb(buf, sect)) != 0)
			goto out;
	}
	/* success */
	r = 0;
 out:
	free(sblob);
	sshbuf_free(sect);
	return r;
}

static void
format_timestamp(u_int64_t timestamp, char *ts, size_t nts)
{
	time_t t;
	struct tm *tm;

	t = timestamp;
	tm = localtime(&t);
	if (tm == NULL)
		strlcpy(ts, "<INVALID>", nts);
	else {
		*ts = '\0';
		strftime(ts, nts, "%Y%m%dT%H%M%S", tm);
	}
}

static int
cert_extension_subsection(struct sshbuf *subsect, struct ssh_krl *krl)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	u_char critical = 1;
	struct sshbuf *value = NULL;
	char *name = NULL;

	if ((r = sshbuf_get_cstring(subsect, &name, NULL)) != 0 ||
	    (r = sshbuf_get_u8(subsect, &critical)) != 0 ||
	    (r = sshbuf_froms(subsect, &value)) != 0) {
		debug_fr(r, "parse");
		error("KRL has invalid certificate extension subsection");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_len(subsect) != 0) {
		error("KRL has invalid certificate extension subsection: "
		    "trailing data");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	debug_f("cert extension %s critical %u len %zu",
	    name, critical, sshbuf_len(value));
	/* no extensions are currently supported */
	if (critical) {
		error("KRL contains unsupported critical certificate "
		    "subsection \"%s\"", name);
		r = SSH_ERR_FEATURE_UNSUPPORTED;
		goto out;
	}
	/* success */
	r = 0;
 out:
	free(name);
	sshbuf_free(value);
	return r;
}

static int
parse_revoked_certs(struct sshbuf *buf, struct ssh_krl *krl)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	u_char type;
	const u_char *blob;
	size_t blen, nbits;
	struct sshbuf *subsect = NULL;
	u_int64_t serial, serial_lo, serial_hi;
	struct bitmap *bitmap = NULL;
	char *key_id = NULL;
	struct sshkey *ca_key = NULL;

	if ((subsect = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	/* Header: key, reserved */
	if ((r = sshbuf_get_string_direct(buf, &blob, &blen)) != 0 ||
	    (r = sshbuf_skip_string(buf)) != 0)
		goto out;
	if (blen != 0 && (r = sshkey_from_blob(blob, blen, &ca_key)) != 0)
		goto out;

	while (sshbuf_len(buf) > 0) {
		sshbuf_free(subsect);
		subsect = NULL;
		if ((r = sshbuf_get_u8(buf, &type)) != 0 ||
		    (r = sshbuf_froms(buf, &subsect)) != 0)
			goto out;
		KRL_DBG(("subsection type 0x%02x", type));
		/* sshbuf_dump(subsect, stderr); */

		switch (type) {
		case KRL_SECTION_CERT_SERIAL_LIST:
			while (sshbuf_len(subsect) > 0) {
				if ((r = sshbuf_get_u64(subsect, &serial)) != 0)
					goto out;
				if ((r = ssh_krl_revoke_cert_by_serial(krl,
				    ca_key, serial)) != 0)
					goto out;
			}
			break;
		case KRL_SECTION_CERT_SERIAL_RANGE:
			if ((r = sshbuf_get_u64(subsect, &serial_lo)) != 0 ||
			    (r = sshbuf_get_u64(subsect, &serial_hi)) != 0)
				goto out;
			if ((r = ssh_krl_revoke_cert_by_serial_range(krl,
			    ca_key, serial_lo, serial_hi)) != 0)
				goto out;
			break;
		case KRL_SECTION_CERT_SERIAL_BITMAP:
			if ((bitmap = bitmap_new()) == NULL) {
				r = SSH_ERR_ALLOC_FAIL;
				goto out;
			}
			if ((r = sshbuf_get_u64(subsect, &serial_lo)) != 0 ||
			    (r = sshbuf_get_bignum2_bytes_direct(subsect,
			    &blob, &blen)) != 0)
				goto out;
			if (bitmap_from_string(bitmap, blob, blen) != 0) {
				r = SSH_ERR_INVALID_FORMAT;
				goto out;
			}
			nbits = bitmap_nbits(bitmap);
			for (serial = 0; serial < (u_int64_t)nbits; serial++) {
				if (serial > 0 && serial_lo + serial == 0) {
					error_f("bitmap wraps u64");
					r = SSH_ERR_INVALID_FORMAT;
					goto out;
				}
				if (!bitmap_test_bit(bitmap, serial))
					continue;
				if ((r = ssh_krl_revoke_cert_by_serial(krl,
				    ca_key, serial_lo + serial)) != 0)
					goto out;
			}
			bitmap_free(bitmap);
			bitmap = NULL;
			break;
		case KRL_SECTION_CERT_KEY_ID:
			while (sshbuf_len(subsect) > 0) {
				if ((r = sshbuf_get_cstring(subsect,
				    &key_id, NULL)) != 0)
					goto out;
				if ((r = ssh_krl_revoke_cert_by_key_id(krl,
				    ca_key, key_id)) != 0)
					goto out;
				free(key_id);
				key_id = NULL;
			}
			break;
		case KRL_SECTION_CERT_EXTENSION:
			if ((r = cert_extension_subsection(subsect, krl)) != 0)
				goto out;
			break;
		default:
			error("Unsupported KRL certificate section %u", type);
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		if (sshbuf_len(subsect) > 0) {
			error("KRL certificate section contains unparsed data");
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
	}

	r = 0;
 out:
	if (bitmap != NULL)
		bitmap_free(bitmap);
	free(key_id);
	sshkey_free(ca_key);
	sshbuf_free(subsect);
	return r;
}

static int
blob_section(struct sshbuf *sect, struct revoked_blob_tree *target_tree,
    size_t expected_len)
{
	u_char *rdata = NULL;
	size_t rlen = 0;
	int r;

	while (sshbuf_len(sect) > 0) {
		if ((r = sshbuf_get_string(sect, &rdata, &rlen)) != 0)
			return r;
		if (expected_len != 0 && rlen != expected_len) {
			error_f("bad length");
			free(rdata);
			return SSH_ERR_INVALID_FORMAT;
		}
		if ((r = revoke_blob(target_tree, rdata, rlen)) != 0) {
			free(rdata);
			return r;
		}
	}
	return 0;
}

static int
extension_section(struct sshbuf *sect, struct ssh_krl *krl)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	u_char critical = 1;
	struct sshbuf *value = NULL;
	char *name = NULL;

	if ((r = sshbuf_get_cstring(sect, &name, NULL)) != 0 ||
	    (r = sshbuf_get_u8(sect, &critical)) != 0 ||
	    (r = sshbuf_froms(sect, &value)) != 0) {
		debug_fr(r, "parse");
		error("KRL has invalid extension section");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_len(sect) != 0) {
		error("KRL has invalid extension section: trailing data");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	debug_f("extension %s critical %u len %zu",
	    name, critical, sshbuf_len(value));
	/* no extensions are currently supported */
	if (critical) {
		error("KRL contains unsupported critical section \"%s\"", name);
		r = SSH_ERR_FEATURE_UNSUPPORTED;
		goto out;
	}
	/* success */
	r = 0;
 out:
	free(name);
	sshbuf_free(value);
	return r;
}

/* Attempt to parse a KRL */
int
ssh_krl_from_blob(struct sshbuf *buf, struct ssh_krl **krlp)
{
	struct sshbuf *copy = NULL, *sect = NULL;
	struct ssh_krl *krl = NULL;
	char timestamp[64];
	int r = SSH_ERR_INTERNAL_ERROR;
	u_char type;
	u_int format_version;

	*krlp = NULL;

	/* KRL must begin with magic string */
	if ((r = sshbuf_cmp(buf, 0, KRL_MAGIC, sizeof(KRL_MAGIC) - 1)) != 0) {
		debug2_f("bad KRL magic header");
		return SSH_ERR_KRL_BAD_MAGIC;
	}

	if ((krl = ssh_krl_init()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		error_f("alloc failed");
		goto out;
	}
	/* Don't modify buffer */
	if ((copy = sshbuf_fromb(buf)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_consume(copy, sizeof(KRL_MAGIC) - 1)) != 0 ||
	    (r = sshbuf_get_u32(copy, &format_version)) != 0)
		goto out;
	if (format_version != KRL_FORMAT_VERSION) {
		error_f("unsupported KRL format version %u", format_version);
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((r = sshbuf_get_u64(copy, &krl->krl_version)) != 0 ||
	    (r = sshbuf_get_u64(copy, &krl->generated_date)) != 0 ||
	    (r = sshbuf_get_u64(copy, &krl->flags)) != 0 ||
	    (r = sshbuf_skip_string(copy)) != 0 ||
	    (r = sshbuf_get_cstring(copy, &krl->comment, NULL)) != 0) {
		error_fr(r, "parse KRL header");
		goto out;
	}
	format_timestamp(krl->generated_date, timestamp, sizeof(timestamp));
	debug("KRL version %llu generated at %s%s%s",
	    (long long unsigned)krl->krl_version, timestamp,
	    *krl->comment ? ": " : "", krl->comment);

	/* Parse and load the KRL sections. */
	while (sshbuf_len(copy) > 0) {
		sshbuf_free(sect);
		sect = NULL;
		if ((r = sshbuf_get_u8(copy, &type)) != 0 ||
		    (r = sshbuf_froms(copy, &sect)) != 0)
			goto out;
		KRL_DBG(("section 0x%02x", type));

		switch (type) {
		case KRL_SECTION_CERTIFICATES:
			if ((r = parse_revoked_certs(sect, krl)) != 0)
				goto out;
			break;
		case KRL_SECTION_EXPLICIT_KEY:
			if ((r = blob_section(sect,
			    &krl->revoked_keys, 0)) != 0)
				goto out;
			break;
		case KRL_SECTION_FINGERPRINT_SHA1:
			if ((r = blob_section(sect,
			    &krl->revoked_sha1s, 20)) != 0)
				goto out;
			break;
		case KRL_SECTION_FINGERPRINT_SHA256:
			if ((r = blob_section(sect,
			    &krl->revoked_sha256s, 32)) != 0)
				goto out;
			break;
		case KRL_SECTION_EXTENSION:
			if ((r = extension_section(sect, krl)) != 0)
				goto out;
			break;
		case KRL_SECTION_SIGNATURE:
			/* Handled above, but still need to stay in synch */
			sshbuf_free(sect);
			sect = NULL;
			if ((r = sshbuf_skip_string(copy)) != 0)
				goto out;
			break;
		default:
			error("Unsupported KRL section %u", type);
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		if (sect != NULL && sshbuf_len(sect) > 0) {
			error("KRL section contains unparsed data");
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
	}

	/* Success */
	*krlp = krl;
	r = 0;
 out:
	if (r != 0)
		ssh_krl_free(krl);
	sshbuf_free(copy);
	sshbuf_free(sect);
	return r;
}

/* Checks certificate serial number and key ID revocation */
static int
is_cert_revoked(const struct sshkey *key, struct revoked_certs *rc)
{
	struct revoked_serial rs, *ers;
	struct revoked_key_id rki, *erki;

	/* Check revocation by cert key ID */
	memset(&rki, 0, sizeof(rki));
	rki.key_id = key->cert->key_id;
	erki = RB_FIND(revoked_key_id_tree, &rc->revoked_key_ids, &rki);
	if (erki != NULL) {
		KRL_DBG(("revoked by key ID"));
		return SSH_ERR_KEY_REVOKED;
	}

	/*
	 * Zero serials numbers are ignored (it's the default when the
	 * CA doesn't specify one).
	 */
	if (key->cert->serial == 0)
		return 0;

	memset(&rs, 0, sizeof(rs));
	rs.lo = rs.hi = key->cert->serial;
	ers = RB_FIND(revoked_serial_tree, &rc->revoked_serials, &rs);
	if (ers != NULL) {
		KRL_DBG(("revoked serial %llu matched %llu:%llu",
		    key->cert->serial, ers->lo, ers->hi));
		return SSH_ERR_KEY_REVOKED;
	}
	return 0;
}

/* Checks whether a given key/cert is revoked. Does not check its CA */
static int
is_key_revoked(struct ssh_krl *krl, const struct sshkey *key)
{
	struct revoked_blob rb, *erb;
	struct revoked_certs *rc;
	int r;

	/* Check explicitly revoked hashes first */
	memset(&rb, 0, sizeof(rb));
	if ((r = sshkey_fingerprint_raw(key, SSH_DIGEST_SHA1,
	    &rb.blob, &rb.len)) != 0)
		return r;
	erb = RB_FIND(revoked_blob_tree, &krl->revoked_sha1s, &rb);
	free(rb.blob);
	if (erb != NULL) {
		KRL_DBG(("revoked by key SHA1"));
		return SSH_ERR_KEY_REVOKED;
	}
	memset(&rb, 0, sizeof(rb));
	if ((r = sshkey_fingerprint_raw(key, SSH_DIGEST_SHA256,
	    &rb.blob, &rb.len)) != 0)
		return r;
	erb = RB_FIND(revoked_blob_tree, &krl->revoked_sha256s, &rb);
	free(rb.blob);
	if (erb != NULL) {
		KRL_DBG(("revoked by key SHA256"));
		return SSH_ERR_KEY_REVOKED;
	}

	/* Next, explicit keys */
	memset(&rb, 0, sizeof(rb));
	if ((r = plain_key_blob(key, &rb.blob, &rb.len)) != 0)
		return r;
	erb = RB_FIND(revoked_blob_tree, &krl->revoked_keys, &rb);
	free(rb.blob);
	if (erb != NULL) {
		KRL_DBG(("revoked by explicit key"));
		return SSH_ERR_KEY_REVOKED;
	}

	if (!sshkey_is_cert(key))
		return 0;

	/* Check cert revocation for the specified CA */
	if ((r = revoked_certs_for_ca_key(krl, key->cert->signature_key,
	    &rc, 0)) != 0)
		return r;
	if (rc != NULL) {
		if ((r = is_cert_revoked(key, rc)) != 0)
			return r;
	}
	/* Check cert revocation for the wildcard CA */
	if ((r = revoked_certs_for_ca_key(krl, NULL, &rc, 0)) != 0)
		return r;
	if (rc != NULL) {
		if ((r = is_cert_revoked(key, rc)) != 0)
			return r;
	}

	KRL_DBG(("%llu no match", key->cert->serial));
	return 0;
}

int
ssh_krl_check_key(struct ssh_krl *krl, const struct sshkey *key)
{
	int r;

	KRL_DBG(("checking key"));
	if ((r = is_key_revoked(krl, key)) != 0)
		return r;
	if (sshkey_is_cert(key)) {
		debug2_f("checking CA key");
		if ((r = is_key_revoked(krl, key->cert->signature_key)) != 0)
			return r;
	}
	KRL_DBG(("key okay"));
	return 0;
}

int
ssh_krl_file_contains_key(const char *path, const struct sshkey *key)
{
	struct sshbuf *krlbuf = NULL;
	struct ssh_krl *krl = NULL;
	int oerrno = 0, r;

	if (path == NULL)
		return 0;
	if ((r = sshbuf_load_file(path, &krlbuf)) != 0) {
		oerrno = errno;
		goto out;
	}
	if ((r = ssh_krl_from_blob(krlbuf, &krl)) != 0)
		goto out;
	debug2_f("checking KRL %s", path);
	r = ssh_krl_check_key(krl, key);
 out:
	sshbuf_free(krlbuf);
	ssh_krl_free(krl);
	if (r != 0)
		errno = oerrno;
	return r;
}

int
krl_dump(struct ssh_krl *krl, FILE *f)
{
	struct sshkey *key = NULL;
	struct revoked_blob *rb;
	struct revoked_certs *rc;
	struct revoked_serial *rs;
	struct revoked_key_id *rki;
	int r, ret = 0;
	char *fp, timestamp[64];

	/* Try to print in a KRL spec-compatible format */
	format_timestamp(krl->generated_date, timestamp, sizeof(timestamp));
	fprintf(f, "# KRL version %llu\n",
	    (unsigned long long)krl->krl_version);
	fprintf(f, "# Generated at %s\n", timestamp);
	if (krl->comment != NULL && *krl->comment != '\0') {
		r = INT_MAX;
		asmprintf(&fp, INT_MAX, &r, "%s", krl->comment);
		fprintf(f, "# Comment: %s\n", fp);
		free(fp);
	}
	fputc('\n', f);

	RB_FOREACH(rb, revoked_blob_tree, &krl->revoked_keys) {
		if ((r = sshkey_from_blob(rb->blob, rb->len, &key)) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			error_r(r, "parse KRL key");
			continue;
		}
		if ((fp = sshkey_fingerprint(key, SSH_FP_HASH_DEFAULT,
		    SSH_FP_DEFAULT)) == NULL) {
			ret = SSH_ERR_INVALID_FORMAT;
			error("sshkey_fingerprint failed");
			continue;
		}
		fprintf(f, "hash: %s # %s\n", fp, sshkey_ssh_name(key));
		free(fp);
		free(key);
	}
	RB_FOREACH(rb, revoked_blob_tree, &krl->revoked_sha256s) {
		fp = tohex(rb->blob, rb->len);
		fprintf(f, "hash: SHA256:%s\n", fp);
		free(fp);
	}
	RB_FOREACH(rb, revoked_blob_tree, &krl->revoked_sha1s) {
		/*
		 * There is not KRL spec keyword for raw SHA1 hashes, so
		 * print them as comments.
		 */
		fp = tohex(rb->blob, rb->len);
		fprintf(f, "# hash SHA1:%s\n", fp);
		free(fp);
	}

	TAILQ_FOREACH(rc, &krl->revoked_certs, entry) {
		fputc('\n', f);
		if (rc->ca_key == NULL)
			fprintf(f, "# Wildcard CA\n");
		else {
			if ((fp = sshkey_fingerprint(rc->ca_key,
			    SSH_FP_HASH_DEFAULT, SSH_FP_DEFAULT)) == NULL) {
				ret = SSH_ERR_INVALID_FORMAT;
				error("sshkey_fingerprint failed");
				continue;
			}
			fprintf(f, "# CA key %s %s\n",
			    sshkey_ssh_name(rc->ca_key), fp);
			free(fp);
		}
		RB_FOREACH(rs, revoked_serial_tree, &rc->revoked_serials) {
			if (rs->lo == rs->hi) {
				fprintf(f, "serial: %llu\n",
				    (unsigned long long)rs->lo);
			} else {
				fprintf(f, "serial: %llu-%llu\n",
				    (unsigned long long)rs->lo,
				    (unsigned long long)rs->hi);
			}
		}
		RB_FOREACH(rki, revoked_key_id_tree, &rc->revoked_key_ids) {
			/*
			 * We don't want key IDs with embedded newlines to
			 * mess up the display.
			 */
			r = INT_MAX;
			asmprintf(&fp, INT_MAX, &r, "%s", rki->key_id);
			fprintf(f, "id: %s\n", fp);
			free(fp);
		}
	}
	return ret;
}
