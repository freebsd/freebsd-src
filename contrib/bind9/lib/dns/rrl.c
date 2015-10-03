/*
 * Copyright (C) 2013-2015  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */

/*
 * Rate limit DNS responses.
 */

/* #define ISC_LIST_CHECKINIT */

#include <config.h>
#include <isc/mem.h>
#include <isc/net.h>
#include <isc/netaddr.h>
#include <isc/print.h>

#include <dns/result.h>
#include <dns/rcode.h>
#include <dns/rdatatype.h>
#include <dns/rdataclass.h>
#include <dns/log.h>
#include <dns/rrl.h>
#include <dns/view.h>

static void
log_end(dns_rrl_t *rrl, dns_rrl_entry_t *e, isc_boolean_t early,
	char *log_buf, unsigned int log_buf_len);

/*
 * Get a modulus for a hash function that is tolerably likely to be
 * relatively prime to most inputs.  Of course, we get a prime for for initial
 * values not larger than the square of the last prime.  We often get a prime
 * after that.
 * This works well in practice for hash tables up to at least 100
 * times the square of the last prime and better than a multiplicative hash.
 */
static int
hash_divisor(unsigned int initial) {
	static isc_uint16_t primes[] = {
		  3,   5,   7,  11,  13,  17,  19,  23,  29,  31,  37,  41,
		 43,  47,  53,  59,  61,  67,  71,  73,  79,  83,  89,  97,
#if 0
		101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157,
		163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227,
		229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283,
		293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367,
		373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439,
		443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509,
		521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599,
		601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659, 661,
		673, 677, 683, 691, 701, 709, 719, 727, 733, 739, 743, 751,
		757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829,
		839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919,
		929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997,1009,
#endif
	};
	int divisions, tries;
	unsigned int result;
	isc_uint16_t *pp, p;

	result = initial;

	if (primes[sizeof(primes)/sizeof(primes[0])-1] >= result) {
		pp = primes;
		while (*pp < result)
			++pp;
		return (*pp);
	}

	if ((result & 1) == 0)
		++result;

	divisions = 0;
	tries = 1;
	pp = primes;
	do {
		p = *pp++;
		++divisions;
		if ((result % p) == 0) {
			++tries;
			result += 2;
			pp = primes;
		}
	} while (pp < &primes[sizeof(primes) / sizeof(primes[0])]);

	if (isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DEBUG3))
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
			      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_DEBUG3,
			      "%d hash_divisor() divisions in %d tries"
			      " to get %d from %d",
			      divisions, tries, result, initial);

	return (result);
}

/*
 * Convert a timestamp to a number of seconds in the past.
 */
static inline int
delta_rrl_time(isc_stdtime_t ts, isc_stdtime_t now) {
	int delta;

	delta = now - ts;
	if (delta >= 0)
		return (delta);

	/*
	 * The timestamp is in the future.  That future might result from
	 * re-ordered requests, because we use timestamps on requests
	 * instead of consulting a clock.  Timestamps in the distant future are
	 * assumed to result from clock changes.  When the clock changes to
	 * the past, make existing timestamps appear to be in the past.
	 */
	if (delta < -DNS_RRL_MAX_TIME_TRAVEL)
		return (DNS_RRL_FOREVER);
	return (0);
}

static inline int
get_age(const dns_rrl_t *rrl, const dns_rrl_entry_t *e, isc_stdtime_t now) {
	if (!e->ts_valid)
		return (DNS_RRL_FOREVER);
	return (delta_rrl_time(e->ts + rrl->ts_bases[e->ts_gen], now));
}

static inline void
set_age(dns_rrl_t *rrl, dns_rrl_entry_t *e, isc_stdtime_t now) {
	dns_rrl_entry_t *e_old;
	unsigned int ts_gen;
	int i, ts;

	ts_gen = rrl->ts_gen;
	ts = now - rrl->ts_bases[ts_gen];
	if (ts < 0) {
		if (ts < -DNS_RRL_MAX_TIME_TRAVEL)
			ts = DNS_RRL_FOREVER;
		else
			ts = 0;
	}

	/*
	 * Make a new timestamp base if the current base is too old.
	 * All entries older than DNS_RRL_MAX_WINDOW seconds are ancient,
	 * useless history.  Their timestamps can be treated as if they are
	 * all the same.
	 * We only do arithmetic on more recent timestamps, so bases for
	 * older timestamps can be recycled provided the old timestamps are
	 * marked as ancient history.
	 * This loop is almost always very short because most entries are
	 * recycled after one second and any entries that need to be marked
	 * are older than (DNS_RRL_TS_BASES)*DNS_RRL_MAX_TS seconds.
	 */
	if (ts >= DNS_RRL_MAX_TS) {
		ts_gen = (ts_gen + 1) % DNS_RRL_TS_BASES;
		for (e_old = ISC_LIST_TAIL(rrl->lru), i = 0;
		     e_old != NULL && (e_old->ts_gen == ts_gen ||
				       !ISC_LINK_LINKED(e_old, hlink));
		     e_old = ISC_LIST_PREV(e_old, lru), ++i)
		{
			e_old->ts_valid = ISC_FALSE;
		}
		if (i != 0)
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
				      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_DEBUG1,
				      "rrl new time base scanned %d entries"
				      " at %d for %d %d %d %d",
				      i, now, rrl->ts_bases[ts_gen],
				      rrl->ts_bases[(ts_gen + 1) %
					DNS_RRL_TS_BASES],
				      rrl->ts_bases[(ts_gen + 2) %
					DNS_RRL_TS_BASES],
				      rrl->ts_bases[(ts_gen + 3) %
					DNS_RRL_TS_BASES]);
		rrl->ts_gen = ts_gen;
		rrl->ts_bases[ts_gen] = now;
		ts = 0;
	}

	e->ts_gen = ts_gen;
	e->ts = ts;
	e->ts_valid = ISC_TRUE;
}

static isc_result_t
expand_entries(dns_rrl_t *rrl, int new) {
	unsigned int bsize;
	dns_rrl_block_t *b;
	dns_rrl_entry_t *e;
	double rate;
	int i;

	if (rrl->num_entries + new >= rrl->max_entries &&
	    rrl->max_entries != 0)
	{
		new = rrl->max_entries - rrl->num_entries;
		if (new <= 0)
			return (ISC_R_SUCCESS);
	}

	/*
	 * Log expansions so that the user can tune max-table-size
	 * and min-table-size.
	 */
	if (isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DROP) &&
	    rrl->hash != NULL) {
		rate = rrl->probes;
		if (rrl->searches != 0)
			rate /= rrl->searches;
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
			      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_DROP,
			      "increase from %d to %d RRL entries with"
			      " %d bins; average search length %.1f",
			      rrl->num_entries, rrl->num_entries+new,
			      rrl->hash->length, rate);
	}

	bsize = sizeof(dns_rrl_block_t) + (new-1)*sizeof(dns_rrl_entry_t);
	b = isc_mem_get(rrl->mctx, bsize);
	if (b == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
			      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_FAIL,
			      "isc_mem_get(%d) failed for RRL entries",
			      bsize);
		return (ISC_R_NOMEMORY);
	}
	memset(b, 0, bsize);
	b->size = bsize;

	e = b->entries;
	for (i = 0; i < new; ++i, ++e) {
		ISC_LINK_INIT(e, hlink);
		ISC_LIST_INITANDAPPEND(rrl->lru, e, lru);
	}
	rrl->num_entries += new;
	ISC_LIST_INITANDAPPEND(rrl->blocks, b, link);

	return (ISC_R_SUCCESS);
}

static inline dns_rrl_bin_t *
get_bin(dns_rrl_hash_t *hash, unsigned int hval) {
	INSIST(hash != NULL);
	return (&hash->bins[hval % hash->length]);
}

static void
free_old_hash(dns_rrl_t *rrl) {
	dns_rrl_hash_t *old_hash;
	dns_rrl_bin_t *old_bin;
	dns_rrl_entry_t *e, *e_next;

	old_hash = rrl->old_hash;
	for (old_bin = &old_hash->bins[0];
	     old_bin < &old_hash->bins[old_hash->length];
	     ++old_bin)
	{
		for (e = ISC_LIST_HEAD(*old_bin); e != NULL; e = e_next) {
			e_next = ISC_LIST_NEXT(e, hlink);
			ISC_LINK_INIT(e, hlink);
		}
	}

	isc_mem_put(rrl->mctx, old_hash,
		    sizeof(*old_hash)
		      + (old_hash->length - 1) * sizeof(old_hash->bins[0]));
	rrl->old_hash = NULL;
}

static isc_result_t
expand_rrl_hash(dns_rrl_t *rrl, isc_stdtime_t now) {
	dns_rrl_hash_t *hash;
	int old_bins, new_bins, hsize;
	double rate;

	if (rrl->old_hash != NULL)
		free_old_hash(rrl);

	/*
	 * Most searches fail and so go to the end of the chain.
	 * Use a small hash table load factor.
	 */
	old_bins = (rrl->hash == NULL) ? 0 : rrl->hash->length;
	new_bins = old_bins/8 + old_bins;
	if (new_bins < rrl->num_entries)
		new_bins = rrl->num_entries;
	new_bins = hash_divisor(new_bins);

	hsize = sizeof(dns_rrl_hash_t) + (new_bins-1)*sizeof(hash->bins[0]);
	hash = isc_mem_get(rrl->mctx, hsize);
	if (hash == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
			      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_FAIL,
			      "isc_mem_get(%d) failed for"
			      " RRL hash table",
			      hsize);
		return (ISC_R_NOMEMORY);
	}
	memset(hash, 0, hsize);
	hash->length = new_bins;
	rrl->hash_gen ^= 1;
	hash->gen = rrl->hash_gen;

	if (isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DROP) && old_bins != 0) {
		rate = rrl->probes;
		if (rrl->searches != 0)
			rate /= rrl->searches;
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
			      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_DROP,
			      "increase from %d to %d RRL bins for"
			      " %d entries; average search length %.1f",
			      old_bins, new_bins, rrl->num_entries, rate);
	}

	rrl->old_hash = rrl->hash;
	if (rrl->old_hash != NULL)
		rrl->old_hash->check_time = now;
	rrl->hash = hash;

	return (ISC_R_SUCCESS);
}

static void
ref_entry(dns_rrl_t *rrl, dns_rrl_entry_t *e, int probes, isc_stdtime_t now) {
	/*
	 * Make the entry most recently used.
	 */
	if (ISC_LIST_HEAD(rrl->lru) != e) {
		if (e == rrl->last_logged)
			rrl->last_logged = ISC_LIST_PREV(e, lru);
		ISC_LIST_UNLINK(rrl->lru, e, lru);
		ISC_LIST_PREPEND(rrl->lru, e, lru);
	}

	/*
	 * Expand the hash table if it is time and necessary.
	 * This will leave the newly referenced entry in a chain in the
	 * old hash table.  It will migrate to the new hash table the next
	 * time it is used or be cut loose when the old hash table is destroyed.
	 */
	rrl->probes += probes;
	++rrl->searches;
	if (rrl->searches > 100 &&
	    delta_rrl_time(rrl->hash->check_time, now) > 1) {
		if (rrl->probes/rrl->searches > 2)
			expand_rrl_hash(rrl, now);
		rrl->hash->check_time = now;
		rrl->probes = 0;
		rrl->searches = 0;
	}
}

static inline isc_boolean_t
key_cmp(const dns_rrl_key_t *a, const dns_rrl_key_t *b) {
	if (memcmp(a, b, sizeof(dns_rrl_key_t)) == 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static inline isc_uint32_t
hash_key(const dns_rrl_key_t *key) {
	isc_uint32_t hval;
	int i;

	hval = key->w[0];
	for (i = sizeof(key->w) / sizeof(key->w[0]) - 1; i >= 0; --i) {
		hval = key->w[i] + (hval<<1);
	}
	return (hval);
}

/*
 * Construct the hash table key.
 * Use a hash of the DNS query name to save space in the database.
 * Collisions result in legitimate rate limiting responses for one
 * query name also limiting responses for other names to the
 * same client.  This is rare and benign enough given the large
 * space costs compared to keeping the entire name in the database
 * entry or the time costs of dynamic allocation.
 */
static void
make_key(const dns_rrl_t *rrl, dns_rrl_key_t *key,
	 const isc_sockaddr_t *client_addr,
	 dns_rdatatype_t qtype, dns_name_t *qname, dns_rdataclass_t qclass,
	 dns_rrl_rtype_t rtype)
{
	dns_name_t base;
	dns_offsets_t base_offsets;
	int labels, i;

	memset(key, 0, sizeof(*key));

	key->s.rtype = rtype;
	if (rtype == DNS_RRL_RTYPE_QUERY) {
		key->s.qtype = qtype;
		key->s.qclass = qclass & 0xff;
	} else if (rtype == DNS_RRL_RTYPE_REFERRAL ||
		   rtype == DNS_RRL_RTYPE_NODATA) {
		/*
		 * Because there is no qtype in the empty answer sections of
		 * referral and NODATA responses, count them as the same.
		 */
		key->s.qclass = qclass & 0xff;
	}

	if (qname != NULL && qname->labels != 0) {
		/*
		 * Ignore the first label of wildcards.
		 */
		if ((qname->attributes & DNS_NAMEATTR_WILDCARD) != 0 &&
		    (labels = dns_name_countlabels(qname)) > 1)
		{
			dns_name_init(&base, base_offsets);
			dns_name_getlabelsequence(qname, 1, labels-1, &base);
			key->s.qname_hash = dns_name_hashbylabel(&base,
							ISC_FALSE);
		} else {
			key->s.qname_hash = dns_name_hashbylabel(qname,
							ISC_FALSE);
		}
	}

	switch (client_addr->type.sa.sa_family) {
	case AF_INET:
		key->s.ip[0] = (client_addr->type.sin.sin_addr.s_addr &
			      rrl->ipv4_mask);
		break;
	case AF_INET6:
		key->s.ipv6 = ISC_TRUE;
		memmove(key->s.ip, &client_addr->type.sin6.sin6_addr,
			sizeof(key->s.ip));
		for (i = 0; i < DNS_RRL_MAX_PREFIX/32; ++i)
			key->s.ip[i] &= rrl->ipv6_mask[i];
		break;
	}
}

static inline dns_rrl_rate_t *
get_rate(dns_rrl_t *rrl, dns_rrl_rtype_t rtype) {
	switch (rtype) {
	case DNS_RRL_RTYPE_QUERY:
		return (&rrl->responses_per_second);
	case DNS_RRL_RTYPE_REFERRAL:
		return (&rrl->referrals_per_second);
	case DNS_RRL_RTYPE_NODATA:
		return (&rrl->nodata_per_second);
	case DNS_RRL_RTYPE_NXDOMAIN:
		return (&rrl->nxdomains_per_second);
	case DNS_RRL_RTYPE_ERROR:
		return (&rrl->errors_per_second);
	case DNS_RRL_RTYPE_ALL:
		return (&rrl->all_per_second);
	default:
		INSIST(0);
	}
	return (NULL);
}

static int
response_balance(dns_rrl_t *rrl, const dns_rrl_entry_t *e, int age) {
	dns_rrl_rate_t *ratep;
	int balance, rate;

	if (e->key.s.rtype == DNS_RRL_RTYPE_TCP) {
		rate = 1;
	} else {
		ratep = get_rate(rrl, e->key.s.rtype);
		rate = ratep->scaled;
	}

	balance = e->responses + age * rate;
	if (balance > rate)
		balance = rate;
	return (balance);
}

/*
 * Search for an entry for a response and optionally create it.
 */
static dns_rrl_entry_t *
get_entry(dns_rrl_t *rrl, const isc_sockaddr_t *client_addr,
	  dns_rdataclass_t qclass, dns_rdatatype_t qtype, dns_name_t *qname,
	  dns_rrl_rtype_t rtype, isc_stdtime_t now, isc_boolean_t create,
	  char *log_buf, unsigned int log_buf_len)
{
	dns_rrl_key_t key;
	isc_uint32_t hval;
	dns_rrl_entry_t *e;
	dns_rrl_hash_t *hash;
	dns_rrl_bin_t *new_bin, *old_bin;
	int probes, age;

	make_key(rrl, &key, client_addr, qtype, qname, qclass, rtype);
	hval = hash_key(&key);

	/*
	 * Look for the entry in the current hash table.
	 */
	new_bin = get_bin(rrl->hash, hval);
	probes = 1;
	e = ISC_LIST_HEAD(*new_bin);
	while (e != NULL) {
		if (key_cmp(&e->key, &key)) {
			ref_entry(rrl, e, probes, now);
			return (e);
		}
		++probes;
		e = ISC_LIST_NEXT(e, hlink);
	}

	/*
	 * Look in the old hash table.
	 */
	if (rrl->old_hash != NULL) {
		old_bin = get_bin(rrl->old_hash, hval);
		e = ISC_LIST_HEAD(*old_bin);
		while (e != NULL) {
			if (key_cmp(&e->key, &key)) {
				ISC_LIST_UNLINK(*old_bin, e, hlink);
				ISC_LIST_PREPEND(*new_bin, e, hlink);
				e->hash_gen = rrl->hash_gen;
				ref_entry(rrl, e, probes, now);
				return (e);
			}
			e = ISC_LIST_NEXT(e, hlink);
		}

		/*
		 * Discard prevous hash table when all of its entries are old.
		 */
		age = delta_rrl_time(rrl->old_hash->check_time, now);
		if (age > rrl->window)
			free_old_hash(rrl);
	}

	if (!create)
		return (NULL);

	/*
	 * The entry does not exist, so create it by finding a free entry.
	 * Keep currently penalized and logged entries.
	 * Try to make more entries if none are idle.
	 * Steal the oldest entry if we cannot create more.
	 */
	for (e = ISC_LIST_TAIL(rrl->lru);
	     e != NULL;
	     e = ISC_LIST_PREV(e, lru))
	{
		if (!ISC_LINK_LINKED(e, hlink))
			break;
		age = get_age(rrl, e, now);
		if (age <= 1) {
			e = NULL;
			break;
		}
		if (!e->logged && response_balance(rrl, e, age) > 0)
			break;
	}
	if (e == NULL) {
		expand_entries(rrl, ISC_MIN((rrl->num_entries+1)/2, 1000));
		e = ISC_LIST_TAIL(rrl->lru);
	}
	if (e->logged)
		log_end(rrl, e, ISC_TRUE, log_buf, log_buf_len);
	if (ISC_LINK_LINKED(e, hlink)) {
		if (e->hash_gen == rrl->hash_gen)
			hash = rrl->hash;
		else
			hash = rrl->old_hash;
		old_bin = get_bin(hash, hash_key(&e->key));
		ISC_LIST_UNLINK(*old_bin, e, hlink);
	}
	ISC_LIST_PREPEND(*new_bin, e, hlink);
	e->hash_gen = rrl->hash_gen;
	e->key = key;
	e->ts_valid = ISC_FALSE;
	ref_entry(rrl, e, probes, now);
	return (e);
}

static void
debit_log(const dns_rrl_entry_t *e, int age, const char *action) {
	char buf[sizeof("age=12345678")];
	const char *age_str;

	if (age == DNS_RRL_FOREVER) {
		age_str = "";
	} else {
		snprintf(buf, sizeof(buf), "age=%d", age);
		age_str = buf;
	}
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
		      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_DEBUG3,
		      "rrl %08x %6s  responses=%-3d %s",
		      hash_key(&e->key), age_str, e->responses, action);
}

static inline dns_rrl_result_t
debit_rrl_entry(dns_rrl_t *rrl, dns_rrl_entry_t *e, double qps, double scale,
		const isc_sockaddr_t *client_addr, isc_stdtime_t now,
		char *log_buf, unsigned int log_buf_len)
{
	int rate, new_rate, slip, new_slip, age, log_secs, min;
	dns_rrl_rate_t *ratep;
	dns_rrl_entry_t const *credit_e;

	/*
	 * Pick the rate counter.
	 * Optionally adjust the rate by the estimated query/second rate.
	 */
	ratep = get_rate(rrl, e->key.s.rtype);
	rate = ratep->r;
	if (rate == 0)
		return (DNS_RRL_RESULT_OK);

	if (scale < 1.0) {
		/*
		 * The limit for clients that have used TCP is not scaled.
		 */
		credit_e = get_entry(rrl, client_addr,
				     0, dns_rdatatype_none, NULL,
				     DNS_RRL_RTYPE_TCP, now, ISC_FALSE,
				     log_buf, log_buf_len);
		if (credit_e != NULL) {
			age = get_age(rrl, e, now);
			if (age < rrl->window)
				scale = 1.0;
		}
	}
	if (scale < 1.0) {
		new_rate = (int) (rate * scale);
		if (new_rate < 1)
			new_rate = 1;
		if (ratep->scaled != new_rate) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
				      DNS_LOGMODULE_REQUEST,
				      DNS_RRL_LOG_DEBUG1,
				      "%d qps scaled %s by %.2f"
				      " from %d to %d",
				      (int)qps, ratep->str, scale,
				      rate, new_rate);
			rate = new_rate;
			ratep->scaled = rate;
		}
	}

	min = -rrl->window * rate;

	/*
	 * Treat time jumps into the recent past as no time.
	 * Treat entries older than the window as if they were just created
	 * Credit other entries.
	 */
	age = get_age(rrl, e, now);
	if (age > 0) {
		/*
		 * Credit tokens earned during elapsed time.
		 */
		if (age > rrl->window) {
			e->responses = rate;
			e->slip_cnt = 0;
		} else {
			e->responses += rate*age;
			if (e->responses > rate) {
				e->responses = rate;
				e->slip_cnt = 0;
			}
		}
		/*
		 * Find the seconds since last log message without overflowing
		 * small counter.  This counter is reset when an entry is
		 * created.  It is not necessarily reset when some requests
		 * are answered provided other requests continue to be dropped
		 * or slipped.  This can happen when the request rate is just
		 * at the limit.
		 */
		if (e->logged) {
			log_secs = e->log_secs;
			log_secs += age;
			if (log_secs > DNS_RRL_MAX_LOG_SECS || log_secs < 0)
				log_secs = DNS_RRL_MAX_LOG_SECS;
			e->log_secs = log_secs;
		}
	}
	set_age(rrl, e, now);

	/*
	 * Debit the entry for this response.
	 */
	if (--e->responses >= 0) {
		if (isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DEBUG3))
			debit_log(e, age, "");
		return (DNS_RRL_RESULT_OK);
	}

	if (e->responses < min)
		e->responses = min;

	/*
	 * Drop this response unless it should slip or leak.
	 */
	slip = rrl->slip.r;
	if (slip > 2 && scale < 1.0) {
		new_slip = (int) (slip * scale);
		if (new_slip < 2)
			new_slip = 2;
		if (rrl->slip.scaled != new_slip) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
				      DNS_LOGMODULE_REQUEST,
				      DNS_RRL_LOG_DEBUG1,
				      "%d qps scaled slip"
				      " by %.2f from %d to %d",
				      (int)qps, scale,
				      slip, new_slip);
			slip = new_slip;
			rrl->slip.scaled = slip;
		}
	}
	if (slip != 0 && e->key.s.rtype != DNS_RRL_RTYPE_ALL) {
		if (e->slip_cnt++ == 0) {
			if ((int) e->slip_cnt >= slip)
				e->slip_cnt = 0;
			if (isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DEBUG3))
				debit_log(e, age, "slip");
			return (DNS_RRL_RESULT_SLIP);
		} else if ((int) e->slip_cnt >= slip) {
			e->slip_cnt = 0;
		}
	}

	if (isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DEBUG3))
		debit_log(e, age, "drop");
	return (DNS_RRL_RESULT_DROP);
}

static inline dns_rrl_qname_buf_t *
get_qname(dns_rrl_t *rrl, const dns_rrl_entry_t *e) {
	dns_rrl_qname_buf_t *qbuf;

	qbuf = rrl->qnames[e->log_qname];
	if (qbuf == NULL || qbuf->e != e)
		return (NULL);
	return (qbuf);
}

static inline void
free_qname(dns_rrl_t *rrl, dns_rrl_entry_t *e) {
	dns_rrl_qname_buf_t *qbuf;

	qbuf = get_qname(rrl, e);
	if (qbuf != NULL) {
		qbuf->e = NULL;
		ISC_LIST_APPEND(rrl->qname_free, qbuf, link);
	}
}

static void
add_log_str(isc_buffer_t *lb, const char *str, unsigned int str_len) {
	isc_region_t region;

	isc_buffer_availableregion(lb, &region);
	if (str_len >= region.length) {
		if (region.length <= 0)
			return;
		str_len = region.length;
	}
	memmove(region.base, str, str_len);
	isc_buffer_add(lb, str_len);
}

#define ADD_LOG_CSTR(eb, s) add_log_str(eb, s, sizeof(s)-1)

/*
 * Build strings for the logs
 */
static void
make_log_buf(dns_rrl_t *rrl, dns_rrl_entry_t *e,
	     const char *str1, const char *str2, isc_boolean_t plural,
	     dns_name_t *qname, isc_boolean_t save_qname,
	     dns_rrl_result_t rrl_result, isc_result_t resp_result,
	     char *log_buf, unsigned int log_buf_len)
{
	isc_buffer_t lb;
	dns_rrl_qname_buf_t *qbuf;
	isc_netaddr_t cidr;
	char strbuf[ISC_MAX(sizeof("/123"), sizeof("  (12345678)"))];
	const char *rstr;
	isc_result_t msg_result;

	if (log_buf_len <= 1) {
		if (log_buf_len == 1)
			log_buf[0] = '\0';
		return;
	}
	isc_buffer_init(&lb, log_buf, log_buf_len-1);

	if (str1 != NULL)
		add_log_str(&lb, str1, strlen(str1));
	if (str2 != NULL)
		add_log_str(&lb, str2, strlen(str2));

	switch (rrl_result) {
	case DNS_RRL_RESULT_OK:
		break;
	case DNS_RRL_RESULT_DROP:
		ADD_LOG_CSTR(&lb, "drop ");
		break;
	case DNS_RRL_RESULT_SLIP:
		ADD_LOG_CSTR(&lb, "slip ");
		break;
	default:
		INSIST(0);
		break;
	}

	switch (e->key.s.rtype) {
	case DNS_RRL_RTYPE_QUERY:
		break;
	case DNS_RRL_RTYPE_REFERRAL:
		ADD_LOG_CSTR(&lb, "referral ");
		break;
	case DNS_RRL_RTYPE_NODATA:
		ADD_LOG_CSTR(&lb, "NODATA ");
		break;
	case DNS_RRL_RTYPE_NXDOMAIN:
		ADD_LOG_CSTR(&lb, "NXDOMAIN ");
		break;
	case DNS_RRL_RTYPE_ERROR:
		if (resp_result == ISC_R_SUCCESS) {
			ADD_LOG_CSTR(&lb, "error ");
		} else {
			rstr = isc_result_totext(resp_result);
			add_log_str(&lb, rstr, strlen(rstr));
			ADD_LOG_CSTR(&lb, " error ");
		}
		break;
	case DNS_RRL_RTYPE_ALL:
		ADD_LOG_CSTR(&lb, "all ");
		break;
	default:
		INSIST(0);
	}

	if (plural)
		ADD_LOG_CSTR(&lb, "responses to ");
	else
		ADD_LOG_CSTR(&lb, "response to ");

	memset(&cidr, 0, sizeof(cidr));
	if (e->key.s.ipv6) {
		snprintf(strbuf, sizeof(strbuf), "/%d", rrl->ipv6_prefixlen);
		cidr.family = AF_INET6;
		memset(&cidr.type.in6, 0,  sizeof(cidr.type.in6));
		memmove(&cidr.type.in6, e->key.s.ip, sizeof(e->key.s.ip));
	} else {
		snprintf(strbuf, sizeof(strbuf), "/%d", rrl->ipv4_prefixlen);
		cidr.family = AF_INET;
		cidr.type.in.s_addr = e->key.s.ip[0];
	}
	msg_result = isc_netaddr_totext(&cidr, &lb);
	if (msg_result != ISC_R_SUCCESS)
		ADD_LOG_CSTR(&lb, "?");
	add_log_str(&lb, strbuf, strlen(strbuf));

	if (e->key.s.rtype == DNS_RRL_RTYPE_QUERY ||
	    e->key.s.rtype == DNS_RRL_RTYPE_REFERRAL ||
	    e->key.s.rtype == DNS_RRL_RTYPE_NODATA ||
	    e->key.s.rtype == DNS_RRL_RTYPE_NXDOMAIN) {
		qbuf = get_qname(rrl, e);
		if (save_qname && qbuf == NULL &&
		    qname != NULL && dns_name_isabsolute(qname)) {
			/*
			 * Capture the qname for the "stop limiting" message.
			 */
			qbuf = ISC_LIST_TAIL(rrl->qname_free);
			if (qbuf != NULL) {
				ISC_LIST_UNLINK(rrl->qname_free, qbuf, link);
			} else if (rrl->num_qnames < DNS_RRL_QNAMES) {
				qbuf = isc_mem_get(rrl->mctx, sizeof(*qbuf));
				if (qbuf != NULL) {
					memset(qbuf, 0, sizeof(*qbuf));
					ISC_LINK_INIT(qbuf, link);
					qbuf->index = rrl->num_qnames;
					rrl->qnames[rrl->num_qnames++] = qbuf;
				} else {
					isc_log_write(dns_lctx,
						      DNS_LOGCATEGORY_RRL,
						      DNS_LOGMODULE_REQUEST,
						      DNS_RRL_LOG_FAIL,
						      "isc_mem_get(%d)"
						      " failed for RRL qname",
						      (int)sizeof(*qbuf));
				}
			}
			if (qbuf != NULL) {
				e->log_qname = qbuf->index;
				qbuf->e = e;
				dns_fixedname_init(&qbuf->qname);
				dns_name_copy(qname,
					      dns_fixedname_name(&qbuf->qname),
					      NULL);
			}
		}
		if (qbuf != NULL)
			qname = dns_fixedname_name(&qbuf->qname);
		if (qname != NULL) {
			ADD_LOG_CSTR(&lb, " for ");
			(void)dns_name_totext(qname, ISC_TRUE, &lb);
		} else {
			ADD_LOG_CSTR(&lb, " for (?)");
		}
		if (e->key.s.rtype != DNS_RRL_RTYPE_NXDOMAIN) {
			ADD_LOG_CSTR(&lb, " ");
			(void)dns_rdataclass_totext(e->key.s.qclass, &lb);
			if (e->key.s.rtype == DNS_RRL_RTYPE_QUERY) {
				ADD_LOG_CSTR(&lb, " ");
				(void)dns_rdatatype_totext(e->key.s.qtype, &lb);
			}
		}
		snprintf(strbuf, sizeof(strbuf), "  (%08x)",
			 e->key.s.qname_hash);
		add_log_str(&lb, strbuf, strlen(strbuf));
	}

	/*
	 * We saved room for '\0'.
	 */
	log_buf[isc_buffer_usedlength(&lb)] = '\0';
}

static void
log_end(dns_rrl_t *rrl, dns_rrl_entry_t *e, isc_boolean_t early,
	char *log_buf, unsigned int log_buf_len)
{
	if (e->logged) {
		make_log_buf(rrl, e,
			     early ? "*" : NULL,
			     rrl->log_only ? "would stop limiting "
					   : "stop limiting ",
			     ISC_TRUE, NULL, ISC_FALSE,
			     DNS_RRL_RESULT_OK, ISC_R_SUCCESS,
			     log_buf, log_buf_len);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
			      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_DROP,
			      "%s", log_buf);
		free_qname(rrl, e);
		e->logged = ISC_FALSE;
		--rrl->num_logged;
	}
}

/*
 * Log messages for streams that have stopped being rate limited.
 */
static void
log_stops(dns_rrl_t *rrl, isc_stdtime_t now, int limit,
	  char *log_buf, unsigned int log_buf_len)
{
	dns_rrl_entry_t *e;
	int age;

	for (e = rrl->last_logged; e != NULL; e = ISC_LIST_PREV(e, lru)) {
		if (!e->logged)
			continue;
		if (now != 0) {
			age = get_age(rrl, e, now);
			if (age < DNS_RRL_STOP_LOG_SECS ||
			    response_balance(rrl, e, age) < 0)
				break;
		}

		log_end(rrl, e, now == 0, log_buf, log_buf_len);
		if (rrl->num_logged <= 0)
			break;

		/*
		 * Too many messages could stall real work.
		 */
		if (--limit < 0) {
			rrl->last_logged = ISC_LIST_PREV(e, lru);
			return;
		}
	}
	if (e == NULL) {
		INSIST(rrl->num_logged == 0);
		rrl->log_stops_time = now;
	}
	rrl->last_logged = e;
}

/*
 * Main rate limit interface.
 */
dns_rrl_result_t
dns_rrl(dns_view_t *view,
	const isc_sockaddr_t *client_addr, isc_boolean_t is_tcp,
	dns_rdataclass_t qclass, dns_rdatatype_t qtype,
	dns_name_t *qname, isc_result_t resp_result, isc_stdtime_t now,
	isc_boolean_t wouldlog, char *log_buf, unsigned int log_buf_len)
{
	dns_rrl_t *rrl;
	dns_rrl_rtype_t rtype;
	dns_rrl_entry_t *e;
	isc_netaddr_t netclient;
	int secs;
	double qps, scale;
	int exempt_match;
	isc_result_t result;
	dns_rrl_result_t rrl_result;

	INSIST(log_buf != NULL && log_buf_len > 0);

	rrl = view->rrl;
	if (rrl->exempt != NULL) {
		isc_netaddr_fromsockaddr(&netclient, client_addr);
		result = dns_acl_match(&netclient, NULL, rrl->exempt,
				       &view->aclenv, &exempt_match, NULL);
		if (result == ISC_R_SUCCESS && exempt_match > 0)
			return (DNS_RRL_RESULT_OK);
	}

	LOCK(&rrl->lock);

	/*
	 * Estimate total query per second rate when scaling by qps.
	 */
	if (rrl->qps_scale == 0) {
		qps = 0.0;
		scale = 1.0;
	} else {
		++rrl->qps_responses;
		secs = delta_rrl_time(rrl->qps_time, now);
		if (secs <= 0) {
			qps = rrl->qps;
		} else {
			qps = (1.0*rrl->qps_responses) / secs;
			if (secs >= rrl->window) {
				if (isc_log_wouldlog(dns_lctx,
						     DNS_RRL_LOG_DEBUG3))
					isc_log_write(dns_lctx,
						      DNS_LOGCATEGORY_RRL,
						      DNS_LOGMODULE_REQUEST,
						      DNS_RRL_LOG_DEBUG3,
						      "%d responses/%d seconds"
						      " = %d qps",
						      rrl->qps_responses, secs,
						      (int)qps);
				rrl->qps = qps;
				rrl->qps_responses = 0;
				rrl->qps_time = now;
			} else if (qps < rrl->qps) {
				qps = rrl->qps;
			}
		}
		scale = rrl->qps_scale / qps;
	}

	/*
	 * Do maintenance once per second.
	 */
	if (rrl->num_logged > 0 && rrl->log_stops_time != now)
		log_stops(rrl, now, 8, log_buf, log_buf_len);

	/*
	 * Notice TCP responses when scaling limits by qps.
	 * Do not try to rate limit TCP responses.
	 */
	if (is_tcp) {
		if (scale < 1.0) {
			e = get_entry(rrl, client_addr,
				      0, dns_rdatatype_none, NULL,
				      DNS_RRL_RTYPE_TCP, now, ISC_TRUE,
				      log_buf, log_buf_len);
			if (e != NULL) {
				e->responses = -(rrl->window+1);
				set_age(rrl, e, now);
			}
		}
		UNLOCK(&rrl->lock);
		return (ISC_R_SUCCESS);
	}

	/*
	 * Find the right kind of entry, creating it if necessary.
	 * If that is impossible, then nothing more can be done
	 */
	switch (resp_result) {
	case ISC_R_SUCCESS:
		rtype = DNS_RRL_RTYPE_QUERY;
		break;
	case DNS_R_DELEGATION:
		rtype = DNS_RRL_RTYPE_REFERRAL;
		break;
	case DNS_R_NXRRSET:
		rtype = DNS_RRL_RTYPE_NODATA;
		break;
	case DNS_R_NXDOMAIN:
		rtype = DNS_RRL_RTYPE_NXDOMAIN;
		break;
	default:
		rtype = DNS_RRL_RTYPE_ERROR;
		break;
	}
	e = get_entry(rrl, client_addr, qclass, qtype, qname, rtype,
		      now, ISC_TRUE, log_buf, log_buf_len);
	if (e == NULL) {
		UNLOCK(&rrl->lock);
		return (DNS_RRL_RESULT_OK);
	}

	if (isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DEBUG1)) {
		/*
		 * Do not worry about speed or releasing the lock.
		 * This message appears before messages from debit_rrl_entry().
		 */
		make_log_buf(rrl, e, "consider limiting ", NULL, ISC_FALSE,
			     qname, ISC_FALSE, DNS_RRL_RESULT_OK, resp_result,
			     log_buf, log_buf_len);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
			      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_DEBUG1,
			      "%s", log_buf);
	}

	rrl_result = debit_rrl_entry(rrl, e, qps, scale, client_addr, now,
				     log_buf, log_buf_len);

	if (rrl->all_per_second.r != 0) {
		/*
		 * We must debit the all-per-second token bucket if we have
		 * an all-per-second limit for the IP address.
		 * The all-per-second limit determines the log message
		 * when both limits are hit.
		 * The response limiting must continue if the
		 * all-per-second limiting lapses.
		 */
		dns_rrl_entry_t *e_all;
		dns_rrl_result_t rrl_all_result;

		e_all = get_entry(rrl, client_addr,
				  0, dns_rdatatype_none, NULL,
				  DNS_RRL_RTYPE_ALL, now, ISC_TRUE,
				  log_buf, log_buf_len);
		if (e_all == NULL) {
			UNLOCK(&rrl->lock);
			return (DNS_RRL_RESULT_OK);
		}
		rrl_all_result = debit_rrl_entry(rrl, e_all, qps, scale,
						 client_addr, now,
						 log_buf, log_buf_len);
		if (rrl_all_result != DNS_RRL_RESULT_OK) {
			e = e_all;
			rrl_result = rrl_all_result;
			if (isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DEBUG1)) {
				make_log_buf(rrl, e,
					     "prefer all-per-second limiting ",
					     NULL, ISC_TRUE, qname, ISC_FALSE,
					     DNS_RRL_RESULT_OK, resp_result,
					     log_buf, log_buf_len);
				isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
					      DNS_LOGMODULE_REQUEST,
					      DNS_RRL_LOG_DEBUG1,
					      "%s", log_buf);
			}
		}
	}

	if (rrl_result == DNS_RRL_RESULT_OK) {
		UNLOCK(&rrl->lock);
		return (DNS_RRL_RESULT_OK);
	}

	/*
	 * Log occassionally in the rate-limit category.
	 */
	if ((!e->logged || e->log_secs >= DNS_RRL_MAX_LOG_SECS) &&
	    isc_log_wouldlog(dns_lctx, DNS_RRL_LOG_DROP)) {
		make_log_buf(rrl, e, rrl->log_only ? "would " : NULL,
			     e->logged ? "continue limiting " : "limit ",
			     ISC_TRUE, qname, ISC_TRUE,
			     DNS_RRL_RESULT_OK, resp_result,
			     log_buf, log_buf_len);
		if (!e->logged) {
			e->logged = ISC_TRUE;
			if (++rrl->num_logged <= 1)
				rrl->last_logged = e;
		}
		e->log_secs = 0;

		/*
		 * Avoid holding the lock.
		 */
		if (!wouldlog) {
			UNLOCK(&rrl->lock);
			e = NULL;
		}
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_RRL,
			      DNS_LOGMODULE_REQUEST, DNS_RRL_LOG_DROP,
			      "%s", log_buf);
	}

	/*
	 * Make a log message for the caller.
	 */
	if (wouldlog)
		make_log_buf(rrl, e,
			     rrl->log_only ? "would rate limit " : "rate limit ",
			     NULL, ISC_FALSE, qname, ISC_FALSE,
			     rrl_result, resp_result, log_buf, log_buf_len);

	if (e != NULL) {
		/*
		 * Do not save the qname unless we might need it for
		 * the ending log message.
		 */
		if (!e->logged)
			free_qname(rrl, e);
		UNLOCK(&rrl->lock);
	}

	return (rrl_result);
}

void
dns_rrl_view_destroy(dns_view_t *view) {
	dns_rrl_t *rrl;
	dns_rrl_block_t *b;
	dns_rrl_hash_t *h;
	char log_buf[DNS_RRL_LOG_BUF_LEN];
	int i;

	rrl = view->rrl;
	if (rrl == NULL)
		return;
	view->rrl = NULL;

	/*
	 * Assume the caller takes care of locking the view and anything else.
	 */

	if (rrl->num_logged > 0)
		log_stops(rrl, 0, ISC_INT32_MAX, log_buf, sizeof(log_buf));

	for (i = 0; i < DNS_RRL_QNAMES; ++i) {
		if (rrl->qnames[i] == NULL)
			break;
		isc_mem_put(rrl->mctx, rrl->qnames[i], sizeof(*rrl->qnames[i]));
	}

	if (rrl->exempt != NULL)
		dns_acl_detach(&rrl->exempt);

	DESTROYLOCK(&rrl->lock);

	while (!ISC_LIST_EMPTY(rrl->blocks)) {
		b = ISC_LIST_HEAD(rrl->blocks);
		ISC_LIST_UNLINK(rrl->blocks, b, link);
		isc_mem_put(rrl->mctx, b, b->size);
	}

	h = rrl->hash;
	if (h != NULL)
		isc_mem_put(rrl->mctx, h,
			    sizeof(*h) + (h->length - 1) * sizeof(h->bins[0]));

	h = rrl->old_hash;
	if (h != NULL)
		isc_mem_put(rrl->mctx, h,
			    sizeof(*h) + (h->length - 1) * sizeof(h->bins[0]));

	isc_mem_putanddetach(&rrl->mctx, rrl, sizeof(*rrl));
}

isc_result_t
dns_rrl_init(dns_rrl_t **rrlp, dns_view_t *view, int min_entries) {
	dns_rrl_t *rrl;
	isc_result_t result;

	*rrlp = NULL;

	rrl = isc_mem_get(view->mctx, sizeof(*rrl));
	if (rrl == NULL)
		return (ISC_R_NOMEMORY);
	memset(rrl, 0, sizeof(*rrl));
	isc_mem_attach(view->mctx, &rrl->mctx);
	result = isc_mutex_init(&rrl->lock);
	if (result != ISC_R_SUCCESS) {
		isc_mem_putanddetach(&rrl->mctx, rrl, sizeof(*rrl));
		return (result);
	}
	isc_stdtime_get(&rrl->ts_bases[0]);

	view->rrl = rrl;

	result = expand_entries(rrl, min_entries);
	if (result != ISC_R_SUCCESS) {
		dns_rrl_view_destroy(view);
		return (result);
	}
	result = expand_rrl_hash(rrl, 0);
	if (result != ISC_R_SUCCESS) {
		dns_rrl_view_destroy(view);
		return (result);
	}

	*rrlp = rrl;
	return (ISC_R_SUCCESS);
}
