/*
 * Portions Copyright (C) 2004-2015  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */

#include <config.h>

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <isc/app.h>
#include <isc/base32.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/event.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/hex.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/os.h>
#include <isc/print.h>
#include <isc/random.h>
#include <isc/rwlock.h>
#include <isc/serial.h>
#include <isc/safe.h>
#include <isc/stdio.h>
#include <isc/stdlib.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/time.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/diff.h>
#include <dns/dnssec.h>
#include <dns/ds.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/master.h>
#include <dns/masterdump.h>
#include <dns/nsec.h>
#include <dns/nsec3.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdataclass.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/soa.h>
#include <dns/time.h>

#include <dst/dst.h>

#include "dnssectool.h"

#ifndef PATH_MAX
#define PATH_MAX 1024   /* AIX, WIN32, and others don't define this. */
#endif

const char *program = "dnssec-signzone";
int verbose;

typedef struct hashlist hashlist_t;

static int nsec_datatype = dns_rdatatype_nsec;

#define IS_NSEC3	(nsec_datatype == dns_rdatatype_nsec3)
#define OPTOUT(x)	(((x) & DNS_NSEC3FLAG_OPTOUT) != 0)

#define REVOKE(x) ((dst_key_flags(x) & DNS_KEYFLAG_REVOKE) != 0)

#define BUFSIZE 2048
#define MAXDSKEYS 8

#define SIGNER_EVENTCLASS	ISC_EVENTCLASS(0x4453)
#define SIGNER_EVENT_WRITE	(SIGNER_EVENTCLASS + 0)
#define SIGNER_EVENT_WORK	(SIGNER_EVENTCLASS + 1)

#define SOA_SERIAL_KEEP		0
#define SOA_SERIAL_INCREMENT	1
#define SOA_SERIAL_UNIXTIME	2

typedef struct signer_event sevent_t;
struct signer_event {
	ISC_EVENT_COMMON(sevent_t);
	dns_fixedname_t *fname;
	dns_dbnode_t *node;
};

static dns_dnsseckeylist_t keylist;
static unsigned int keycount = 0;
isc_rwlock_t keylist_lock;
static isc_stdtime_t starttime = 0, endtime = 0, dnskey_endtime = 0, now;
static int cycle = -1;
static int jitter = 0;
static isc_boolean_t tryverify = ISC_FALSE;
static isc_boolean_t printstats = ISC_FALSE;
static isc_mem_t *mctx = NULL;
static isc_entropy_t *ectx = NULL;
static dns_ttl_t zone_soa_min_ttl;
static dns_ttl_t soa_ttl;
static FILE *outfp = NULL;
static char *tempfile = NULL;
static const dns_master_style_t *masterstyle;
static dns_masterformat_t inputformat = dns_masterformat_text;
static dns_masterformat_t outputformat = dns_masterformat_text;
static isc_uint32_t rawversion = 1, serialnum = 0;
static isc_boolean_t snset = ISC_FALSE;
static unsigned int nsigned = 0, nretained = 0, ndropped = 0;
static unsigned int nverified = 0, nverifyfailed = 0;
static const char *directory = NULL, *dsdir = NULL;
static isc_mutex_t namelock, statslock;
static isc_taskmgr_t *taskmgr = NULL;
static dns_db_t *gdb;			/* The database */
static dns_dbversion_t *gversion;	/* The database version */
static dns_dbiterator_t *gdbiter;	/* The database iterator */
static dns_rdataclass_t gclass;		/* The class */
static dns_name_t *gorigin;		/* The database origin */
static int nsec3flags = 0;
static dns_iterations_t nsec3iter = 10U;
static unsigned char saltbuf[255];
static unsigned char *gsalt = saltbuf;
static size_t salt_length = 0;
static isc_task_t *master = NULL;
static unsigned int ntasks = 0;
static isc_boolean_t shuttingdown = ISC_FALSE, finished = ISC_FALSE;
static isc_boolean_t nokeys = ISC_FALSE;
static isc_boolean_t removefile = ISC_FALSE;
static isc_boolean_t generateds = ISC_FALSE;
static isc_boolean_t ignore_kskflag = ISC_FALSE;
static isc_boolean_t keyset_kskonly = ISC_FALSE;
static dns_name_t *dlv = NULL;
static dns_fixedname_t dlv_fixed;
static dns_master_style_t *dsstyle = NULL;
static unsigned int serialformat = SOA_SERIAL_KEEP;
static unsigned int hash_length = 0;
static isc_boolean_t unknownalg = ISC_FALSE;
static isc_boolean_t disable_zone_check = ISC_FALSE;
static isc_boolean_t update_chain = ISC_FALSE;
static isc_boolean_t set_keyttl = ISC_FALSE;
static dns_ttl_t keyttl;
static isc_boolean_t smartsign = ISC_FALSE;
static isc_boolean_t remove_orphansigs = ISC_FALSE;
static isc_boolean_t remove_inactkeysigs = ISC_FALSE;
static isc_boolean_t output_dnssec_only = ISC_FALSE;
static isc_boolean_t output_stdout = ISC_FALSE;

#define INCSTAT(counter)		\
	if (printstats) {		\
		LOCK(&statslock);	\
		counter++;		\
		UNLOCK(&statslock);	\
	}

static void
sign(isc_task_t *task, isc_event_t *event);

static void
dumpnode(dns_name_t *name, dns_dbnode_t *node) {
	dns_rdataset_t rds;
	dns_rdatasetiter_t *iter = NULL;
	isc_buffer_t *buffer = NULL;
	isc_region_t r;
	isc_result_t result;
	unsigned bufsize = 4096;

	if (outputformat != dns_masterformat_text)
		return;

	if (!output_dnssec_only) {
		result = dns_master_dumpnodetostream(mctx, gdb, gversion, node,
						     name, masterstyle, outfp);
		check_result(result, "dns_master_dumpnodetostream");
		return;
	}

	result = dns_db_allrdatasets(gdb, node, gversion, 0, &iter);
	check_result(result, "dns_db_allrdatasets");

	dns_rdataset_init(&rds);

	result = isc_buffer_allocate(mctx, &buffer, bufsize);
	check_result(result, "isc_buffer_allocate");

	for (result = dns_rdatasetiter_first(iter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(iter)) {

		dns_rdatasetiter_current(iter, &rds);

		if (rds.type != dns_rdatatype_rrsig &&
		    rds.type != dns_rdatatype_nsec &&
		    rds.type != dns_rdatatype_nsec3 &&
		    rds.type != dns_rdatatype_nsec3param &&
		    (!smartsign || rds.type != dns_rdatatype_dnskey)) {
			dns_rdataset_disassociate(&rds);
			continue;
		}

		for (;;) {
			result = dns_master_rdatasettotext(name, &rds,
							   masterstyle, buffer);
			if (result != ISC_R_NOSPACE)
				break;

			bufsize <<= 1;
			isc_buffer_free(&buffer);
			result = isc_buffer_allocate(mctx, &buffer, bufsize);
			check_result(result, "isc_buffer_allocate");
		}
		check_result(result, "dns_master_rdatasettotext");

		isc_buffer_usedregion(buffer, &r);
		result = isc_stdio_write(r.base, 1, r.length, outfp, NULL);
		check_result(result, "isc_stdio_write");
		isc_buffer_clear(buffer);

		dns_rdataset_disassociate(&rds);
	}

	isc_buffer_free(&buffer);
	dns_rdatasetiter_destroy(&iter);
}

/*%
 * Sign the given RRset with given key, and add the signature record to the
 * given tuple.
 */
static void
signwithkey(dns_name_t *name, dns_rdataset_t *rdataset, dst_key_t *key,
	    dns_ttl_t ttl, dns_diff_t *add, const char *logmsg)
{
	isc_result_t result;
	isc_stdtime_t jendtime, expiry;
	char keystr[DST_KEY_FORMATSIZE];
	dns_rdata_t trdata = DNS_RDATA_INIT;
	unsigned char array[BUFSIZE];
	isc_buffer_t b;
	dns_difftuple_t *tuple;

	dst_key_format(key, keystr, sizeof(keystr));
	vbprintf(1, "\t%s %s\n", logmsg, keystr);

	if (rdataset->type == dns_rdatatype_dnskey)
		expiry = dnskey_endtime;
	else
		expiry = endtime;

	jendtime = (jitter != 0) ? isc_random_jitter(expiry, jitter) : expiry;
	isc_buffer_init(&b, array, sizeof(array));
	result = dns_dnssec_sign(name, rdataset, key, &starttime, &jendtime,
				 mctx, &b, &trdata);
	isc_entropy_stopcallbacksources(ectx);
	if (result != ISC_R_SUCCESS) {
		fatal("dnskey '%s' failed to sign data: %s",
		      keystr, isc_result_totext(result));
	}
	INCSTAT(nsigned);

	if (tryverify) {
		result = dns_dnssec_verify(name, rdataset, key,
					   ISC_TRUE, mctx, &trdata);
		if (result == ISC_R_SUCCESS) {
			vbprintf(3, "\tsignature verified\n");
			INCSTAT(nverified);
		} else {
			vbprintf(3, "\tsignature failed to verify\n");
			INCSTAT(nverifyfailed);
		}
	}

	tuple = NULL;
	result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD, name, ttl, &trdata,
				      &tuple);
	check_result(result, "dns_difftuple_create");
	dns_diff_append(add, &tuple);
}

static inline isc_boolean_t
issigningkey(dns_dnsseckey_t *key) {
	return (key->force_sign || key->hint_sign);
}

static inline isc_boolean_t
ispublishedkey(dns_dnsseckey_t *key) {
	return ((key->force_publish || key->hint_publish) &&
		!key->hint_remove);
}

static inline isc_boolean_t
iszonekey(dns_dnsseckey_t *key) {
	return (ISC_TF(dns_name_equal(dst_key_name(key->key), gorigin) &&
		       dst_key_iszonekey(key->key)));
}

static inline isc_boolean_t
isksk(dns_dnsseckey_t *key) {
	return (key->ksk);
}

static inline isc_boolean_t
iszsk(dns_dnsseckey_t *key) {
	return (ignore_kskflag || !key->ksk);
}

/*%
 * Find the key that generated an RRSIG, if it is in the key list.  If
 * so, return a pointer to it, otherwise return NULL.
 *
 * No locking is performed here, this must be done by the caller.
 */
static dns_dnsseckey_t *
keythatsigned_unlocked(dns_rdata_rrsig_t *rrsig) {
	dns_dnsseckey_t *key;

	for (key = ISC_LIST_HEAD(keylist);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link)) {
		if (rrsig->keyid == dst_key_id(key->key) &&
		    rrsig->algorithm == dst_key_alg(key->key) &&
		    dns_name_equal(&rrsig->signer, dst_key_name(key->key)))
			return (key);
	}
	return (NULL);
}

/*%
 * Finds the key that generated a RRSIG, if possible.  First look at the keys
 * that we've loaded already, and then see if there's a key on disk.
 */
static dns_dnsseckey_t *
keythatsigned(dns_rdata_rrsig_t *rrsig) {
	isc_result_t result;
	dst_key_t *pubkey = NULL, *privkey = NULL;
	dns_dnsseckey_t *key = NULL;

	isc_rwlock_lock(&keylist_lock, isc_rwlocktype_read);
	key = keythatsigned_unlocked(rrsig);
	isc_rwlock_unlock(&keylist_lock, isc_rwlocktype_read);
	if (key != NULL)
		return (key);

	/*
	 * We did not find the key in our list.  Get a write lock now, since
	 * we may be modifying the bits.  We could do the tryupgrade() dance,
	 * but instead just get a write lock and check once again to see if
	 * it is on our list.  It's possible someone else may have added it
	 * after all.
	 */
	isc_rwlock_lock(&keylist_lock, isc_rwlocktype_write);
	key = keythatsigned_unlocked(rrsig);
	if (key != NULL) {
		isc_rwlock_unlock(&keylist_lock, isc_rwlocktype_write);
		return (key);
	}

	result = dst_key_fromfile(&rrsig->signer, rrsig->keyid,
				  rrsig->algorithm, DST_TYPE_PUBLIC,
				  directory, mctx, &pubkey);
	if (result != ISC_R_SUCCESS) {
		isc_rwlock_unlock(&keylist_lock, isc_rwlocktype_write);
		return (NULL);
	}

	result = dst_key_fromfile(&rrsig->signer, rrsig->keyid,
				  rrsig->algorithm,
				  DST_TYPE_PUBLIC | DST_TYPE_PRIVATE,
				  directory, mctx, &privkey);
	if (result == ISC_R_SUCCESS) {
		dst_key_free(&pubkey);
		result = dns_dnsseckey_create(mctx, &privkey, &key);
	} else
		result = dns_dnsseckey_create(mctx, &pubkey, &key);

	if (result == ISC_R_SUCCESS) {
		key->force_publish = ISC_FALSE;
		key->force_sign = ISC_FALSE;
		key->index = keycount++;
		ISC_LIST_APPEND(keylist, key, link);
	}

	isc_rwlock_unlock(&keylist_lock, isc_rwlocktype_write);
	return (key);
}

/*%
 * Check to see if we expect to find a key at this name.  If we see a RRSIG
 * and can't find the signing key that we expect to find, we drop the rrsig.
 * I'm not sure if this is completely correct, but it seems to work.
 */
static isc_boolean_t
expecttofindkey(dns_name_t *name) {
	unsigned int options = DNS_DBFIND_NOWILD;
	dns_fixedname_t fname;
	isc_result_t result;
	char namestr[DNS_NAME_FORMATSIZE];

	dns_fixedname_init(&fname);
	result = dns_db_find(gdb, name, gversion, dns_rdatatype_dnskey, options,
			     0, NULL, dns_fixedname_name(&fname), NULL, NULL);
	switch (result) {
	case ISC_R_SUCCESS:
	case DNS_R_NXDOMAIN:
	case DNS_R_NXRRSET:
		return (ISC_TRUE);
	case DNS_R_DELEGATION:
	case DNS_R_CNAME:
	case DNS_R_DNAME:
		return (ISC_FALSE);
	}
	dns_name_format(name, namestr, sizeof(namestr));
	fatal("failure looking for '%s DNSKEY' in database: %s",
	      namestr, isc_result_totext(result));
	/* NOTREACHED */
	return (ISC_FALSE); /* removes a warning */
}

static inline isc_boolean_t
setverifies(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
	    dns_rdata_t *rrsig)
{
	isc_result_t result;
	result = dns_dnssec_verify(name, set, key, ISC_FALSE, mctx, rrsig);
	if (result == ISC_R_SUCCESS) {
		INCSTAT(nverified);
		return (ISC_TRUE);
	} else {
		INCSTAT(nverifyfailed);
		return (ISC_FALSE);
	}
}

/*%
 * Signs a set.  Goes through contortions to decide if each RRSIG should
 * be dropped or retained, and then determines if any new SIGs need to
 * be generated.
 */
static void
signset(dns_diff_t *del, dns_diff_t *add, dns_dbnode_t *node, dns_name_t *name,
	dns_rdataset_t *set)
{
	dns_rdataset_t sigset;
	dns_rdata_t sigrdata = DNS_RDATA_INIT;
	dns_rdata_rrsig_t rrsig;
	dns_dnsseckey_t *key;
	isc_result_t result;
	isc_boolean_t nosigs = ISC_FALSE;
	isc_boolean_t *wassignedby, *nowsignedby;
	int arraysize;
	dns_difftuple_t *tuple;
	dns_ttl_t ttl;
	int i;
	char namestr[DNS_NAME_FORMATSIZE];
	char typestr[TYPE_FORMATSIZE];
	char sigstr[SIG_FORMATSIZE];

	dns_name_format(name, namestr, sizeof(namestr));
	type_format(set->type, typestr, sizeof(typestr));

	ttl = ISC_MIN(set->ttl, endtime - starttime);

	dns_rdataset_init(&sigset);
	result = dns_db_findrdataset(gdb, node, gversion, dns_rdatatype_rrsig,
				     set->type, 0, &sigset, NULL);
	if (result == ISC_R_NOTFOUND) {
		vbprintf(2, "no existing signatures for %s/%s\n",
			 namestr, typestr);
		result = ISC_R_SUCCESS;
		nosigs = ISC_TRUE;
	}
	if (result != ISC_R_SUCCESS)
		fatal("failed while looking for '%s RRSIG %s': %s",
		      namestr, typestr, isc_result_totext(result));

	vbprintf(1, "%s/%s:\n", namestr, typestr);

	arraysize = keycount;
	if (!nosigs)
		arraysize += dns_rdataset_count(&sigset);
	wassignedby = isc_mem_get(mctx, arraysize * sizeof(isc_boolean_t));
	nowsignedby = isc_mem_get(mctx, arraysize * sizeof(isc_boolean_t));
	if (wassignedby == NULL || nowsignedby == NULL)
		fatal("out of memory");

	for (i = 0; i < arraysize; i++)
		wassignedby[i] = nowsignedby[i] = ISC_FALSE;

	if (nosigs)
		result = ISC_R_NOMORE;
	else
		result = dns_rdataset_first(&sigset);

	while (result == ISC_R_SUCCESS) {
		isc_boolean_t expired, future;
		isc_boolean_t keep = ISC_FALSE, resign = ISC_FALSE;

		dns_rdataset_current(&sigset, &sigrdata);

		result = dns_rdata_tostruct(&sigrdata, &rrsig, NULL);
		check_result(result, "dns_rdata_tostruct");

		future = isc_serial_lt(now, rrsig.timesigned);

		key = keythatsigned(&rrsig);
		sig_format(&rrsig, sigstr, sizeof(sigstr));
		if (key != NULL && issigningkey(key))
			expired = isc_serial_gt(now + cycle, rrsig.timeexpire);
		else
			expired = isc_serial_gt(now, rrsig.timeexpire);

		if (isc_serial_gt(rrsig.timesigned, rrsig.timeexpire)) {
			/* rrsig is dropped and not replaced */
			vbprintf(2, "\trrsig by %s dropped - "
				 "invalid validity period\n",
				 sigstr);
		} else if (key == NULL && !future &&
			   expecttofindkey(&rrsig.signer)) {
			/* rrsig is dropped and not replaced */
			vbprintf(2, "\trrsig by %s dropped - "
				 "private dnskey not found\n",
				 sigstr);
		} else if (key == NULL || future) {
			keep = (!expired && !remove_orphansigs);
			vbprintf(2, "\trrsig by %s %s - dnskey not found\n",
				 keep ? "retained" : "dropped", sigstr);
		} else if (!dns_dnssec_keyactive(key->key, now) &&
			   remove_inactkeysigs) {
			keep = ISC_FALSE;
			vbprintf(2, "\trrsig by %s dropped - key inactive\n",
				 sigstr);
		} else if (issigningkey(key)) {
			wassignedby[key->index] = ISC_TRUE;

			if (!expired && rrsig.originalttl == set->ttl &&
			    setverifies(name, set, key->key, &sigrdata)) {
				vbprintf(2, "\trrsig by %s retained\n", sigstr);
				keep = ISC_TRUE;
			} else {
				vbprintf(2, "\trrsig by %s dropped - %s\n",
					 sigstr, expired ? "expired" :
					 rrsig.originalttl != set->ttl ?
					 "ttl change" : "failed to verify");
				resign = ISC_TRUE;
			}
		} else if (!ispublishedkey(key) && remove_orphansigs) {
			vbprintf(2, "\trrsig by %s dropped - dnskey removed\n",
				 sigstr);
		} else if (iszonekey(key)) {
			wassignedby[key->index] = ISC_TRUE;

			if (!expired && rrsig.originalttl == set->ttl &&
			    setverifies(name, set, key->key, &sigrdata)) {
				vbprintf(2, "\trrsig by %s retained\n", sigstr);
				keep = ISC_TRUE;
			} else {
				vbprintf(2, "\trrsig by %s dropped - %s\n",
					 sigstr, expired ? "expired" :
					 rrsig.originalttl != set->ttl ?
					 "ttl change" : "failed to verify");
			}
		} else if (!expired) {
			vbprintf(2, "\trrsig by %s retained\n", sigstr);
			keep = ISC_TRUE;
		} else {
			vbprintf(2, "\trrsig by %s expired\n", sigstr);
		}

		if (keep) {
			if (key != NULL)
				nowsignedby[key->index] = ISC_TRUE;
			INCSTAT(nretained);
			if (sigset.ttl != ttl) {
				vbprintf(2, "\tfixing ttl %s\n", sigstr);
				tuple = NULL;
				result = dns_difftuple_create(mctx,
							      DNS_DIFFOP_DEL,
							      name, sigset.ttl,
							      &sigrdata,
							      &tuple);
				check_result(result, "dns_difftuple_create");
				dns_diff_append(del, &tuple);
				result = dns_difftuple_create(mctx,
							      DNS_DIFFOP_ADD,
							      name, ttl,
							      &sigrdata,
							      &tuple);
				check_result(result, "dns_difftuple_create");
				dns_diff_append(add, &tuple);
			}
		} else {
			tuple = NULL;
			vbprintf(2, "removing signature by %s\n", sigstr);
			result = dns_difftuple_create(mctx, DNS_DIFFOP_DEL,
						      name, sigset.ttl,
						      &sigrdata, &tuple);
			check_result(result, "dns_difftuple_create");
			dns_diff_append(del, &tuple);
			INCSTAT(ndropped);
		}

		if (resign) {
			INSIST(!keep);

			signwithkey(name, set, key->key, ttl, add,
				    "resigning with dnskey");
			nowsignedby[key->index] = ISC_TRUE;
		}

		dns_rdata_reset(&sigrdata);
		dns_rdata_freestruct(&rrsig);
		result = dns_rdataset_next(&sigset);
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;

	check_result(result, "dns_rdataset_first/next");
	if (dns_rdataset_isassociated(&sigset))
		dns_rdataset_disassociate(&sigset);

	for (key = ISC_LIST_HEAD(keylist);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link))
	{
		if (nowsignedby[key->index])
			continue;

		if (!issigningkey(key))
			continue;

		if (set->type == dns_rdatatype_dnskey &&
		     dns_name_equal(name, gorigin)) {
			isc_boolean_t have_ksk;
			dns_dnsseckey_t *tmpkey;

			have_ksk = isksk(key);
			for (tmpkey = ISC_LIST_HEAD(keylist);
			     tmpkey != NULL;
			     tmpkey = ISC_LIST_NEXT(tmpkey, link)) {
				if (dst_key_alg(key->key) !=
				    dst_key_alg(tmpkey->key))
					continue;
				if (REVOKE(tmpkey->key))
					continue;
				if (isksk(tmpkey))
					have_ksk = ISC_TRUE;
			}
			if (isksk(key) || !have_ksk ||
			    (iszsk(key) && !keyset_kskonly))
				signwithkey(name, set, key->key, ttl, add,
					    "signing with dnskey");
		} else if (set->type == dns_rdatatype_cds ||
			   set->type == dns_rdatatype_cdnskey ||
			   iszsk(key)) {
			signwithkey(name, set, key->key, ttl, add,
				    "signing with dnskey");
		}
	}

	isc_mem_put(mctx, wassignedby, arraysize * sizeof(isc_boolean_t));
	isc_mem_put(mctx, nowsignedby, arraysize * sizeof(isc_boolean_t));
}

struct hashlist {
	unsigned char *hashbuf;
	size_t entries;
	size_t size;
	size_t length;
};

static void
hashlist_init(hashlist_t *l, unsigned int nodes, unsigned int length) {

	l->entries = 0;
	l->length = length + 1;

	if (nodes != 0) {
		l->size = nodes;
		l->hashbuf = malloc(l->size * l->length);
		if (l->hashbuf == NULL)
			l->size = 0;
	} else {
		l->size = 0;
		l->hashbuf = NULL;
	}
}

static void
hashlist_add(hashlist_t *l, const unsigned char *hash, size_t len)
{

	REQUIRE(len <= l->length);

	if (l->entries == l->size) {
		l->size = l->size * 2 + 100;
		l->hashbuf = realloc(l->hashbuf, l->size * l->length);
		if (l->hashbuf == NULL)
			fatal("unable to grow hashlist: out of memory");
	}
	memset(l->hashbuf + l->entries * l->length, 0, l->length);
	memmove(l->hashbuf + l->entries * l->length, hash, len);
	l->entries++;
}

static void
hashlist_add_dns_name(hashlist_t *l, /*const*/ dns_name_t *name,
		      unsigned int hashalg, unsigned int iterations,
		      const unsigned char *salt, size_t salt_len,
		      isc_boolean_t speculative)
{
	char nametext[DNS_NAME_FORMATSIZE];
	unsigned char hash[NSEC3_MAX_HASH_LENGTH + 1];
	unsigned int len;
	size_t i;

	len = isc_iterated_hash(hash, hashalg, iterations,
				salt, (int)salt_len,
				name->ndata, name->length);
	if (verbose) {
		dns_name_format(name, nametext, sizeof nametext);
		for (i = 0 ; i < len; i++)
			fprintf(stderr, "%02x", hash[i]);
		fprintf(stderr, " %s\n", nametext);
	}
	hash[len++] = speculative ? 1 : 0;
	hashlist_add(l, hash, len);
}

static int
hashlist_comp(const void *a, const void *b) {
	return (isc_safe_memcompare(a, b, hash_length + 1));
}

static void
hashlist_sort(hashlist_t *l) {
	qsort(l->hashbuf, l->entries, l->length, hashlist_comp);
}

static isc_boolean_t
hashlist_hasdup(hashlist_t *l) {
	unsigned char *current;
	unsigned char *next = l->hashbuf;
	size_t entries = l->entries;

	/*
	 * Skip initial speculative wild card hashs.
	 */
	while (entries > 0U && next[l->length-1] != 0U) {
		next += l->length;
		entries--;
	}

	current = next;
	while (entries-- > 1U) {
		next += l->length;
		if (next[l->length-1] != 0)
			continue;
		if (isc_safe_memequal(current, next, l->length - 1))
			return (ISC_TRUE);
		current = next;
	}
	return (ISC_FALSE);
}

static const unsigned char *
hashlist_findnext(const hashlist_t *l,
		  const unsigned char hash[NSEC3_MAX_HASH_LENGTH])
{
	size_t entries = l->entries;
	const unsigned char *next = bsearch(hash, l->hashbuf, l->entries,
					    l->length, hashlist_comp);
	INSIST(next != NULL);

	do {
		if (next < l->hashbuf + (l->entries - 1) * l->length)
			next += l->length;
		else
			next = l->hashbuf;
		if (next[l->length - 1] == 0)
			break;
	} while (entries-- > 1U);
	INSIST(entries != 0U);
	return (next);
}

static isc_boolean_t
hashlist_exists(const hashlist_t *l,
		const unsigned char hash[NSEC3_MAX_HASH_LENGTH])
{
	if (bsearch(hash, l->hashbuf, l->entries, l->length, hashlist_comp))
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static void
addnowildcardhash(hashlist_t *l, /*const*/ dns_name_t *name,
		  unsigned int hashalg, unsigned int iterations,
		  const unsigned char *salt, size_t salt_len)
{
	dns_fixedname_t fixed;
	dns_name_t *wild;
	dns_dbnode_t *node = NULL;
	isc_result_t result;
	char namestr[DNS_NAME_FORMATSIZE];

	dns_fixedname_init(&fixed);
	wild = dns_fixedname_name(&fixed);

	result = dns_name_concatenate(dns_wildcardname, name, wild, NULL);
	if (result == ISC_R_NOSPACE)
		return;
	check_result(result,"addnowildcardhash: dns_name_concatenate()");

	result = dns_db_findnode(gdb, wild, ISC_FALSE, &node);
	if (result == ISC_R_SUCCESS) {
		dns_db_detachnode(gdb, &node);
		return;
	}

	if (verbose) {
		dns_name_format(wild, namestr, sizeof(namestr));
		fprintf(stderr, "adding no-wildcardhash for %s\n", namestr);
	}

	hashlist_add_dns_name(l, wild, hashalg, iterations, salt, salt_len,
			      ISC_TRUE);
}

static void
opendb(const char *prefix, dns_name_t *name, dns_rdataclass_t rdclass,
       dns_db_t **dbp)
{
	char filename[PATH_MAX];
	isc_buffer_t b;
	isc_result_t result;

	isc_buffer_init(&b, filename, sizeof(filename));
	if (dsdir != NULL) {
		/* allow room for a trailing slash */
		if (strlen(dsdir) >= isc_buffer_availablelength(&b))
			fatal("path '%s' is too long", dsdir);
		isc_buffer_putstr(&b, dsdir);
		if (dsdir[strlen(dsdir) - 1] != '/')
			isc_buffer_putstr(&b, "/");
	}
	if (strlen(prefix) > isc_buffer_availablelength(&b))
		fatal("path '%s' is too long", dsdir);
	isc_buffer_putstr(&b, prefix);
	result = dns_name_tofilenametext(name, ISC_FALSE, &b);
	check_result(result, "dns_name_tofilenametext()");
	if (isc_buffer_availablelength(&b) == 0) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(name, namestr, sizeof(namestr));
		fatal("name '%s' is too long", namestr);
	}
	isc_buffer_putuint8(&b, 0);

	result = dns_db_create(mctx, "rbt", dns_rootname, dns_dbtype_zone,
			       rdclass, 0, NULL, dbp);
	check_result(result, "dns_db_create()");

	result = dns_db_load3(*dbp, filename, inputformat, DNS_MASTER_HINT);
	if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE)
		dns_db_detach(dbp);
}

/*%
 * Load the DS set for a child zone, if a dsset-* file can be found.
 * If not, try to find a keyset-* file from an earlier version of
 * dnssec-signzone, and build DS records from that.
 */
static isc_result_t
loadds(dns_name_t *name, isc_uint32_t ttl, dns_rdataset_t *dsset) {
	dns_db_t *db = NULL;
	dns_dbversion_t *ver = NULL;
	dns_dbnode_t *node = NULL;
	isc_result_t result;
	dns_rdataset_t keyset;
	dns_rdata_t key, ds;
	unsigned char dsbuf[DNS_DS_BUFFERSIZE];
	dns_diff_t diff;
	dns_difftuple_t *tuple = NULL;

	opendb("dsset-", name, gclass, &db);
	if (db != NULL) {
		result = dns_db_findnode(db, name, ISC_FALSE, &node);
		if (result == ISC_R_SUCCESS) {
			dns_rdataset_init(dsset);
			result = dns_db_findrdataset(db, node, NULL,
						     dns_rdatatype_ds, 0, 0,
						     dsset, NULL);
			dns_db_detachnode(db, &node);
			if (result == ISC_R_SUCCESS) {
				vbprintf(2, "found DS records\n");
				dsset->ttl = ttl;
				dns_db_detach(&db);
				return (result);
			}
		}
		dns_db_detach(&db);
	}

	/* No DS records found; try again, looking for DNSKEY records */
	opendb("keyset-", name, gclass, &db);
	if (db == NULL) {
		return (ISC_R_NOTFOUND);
	}

	result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS) {
		dns_db_detach(&db);
		return (result);
	}

	dns_rdataset_init(&keyset);
	result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_dnskey, 0, 0,
				     &keyset, NULL);
	if (result != ISC_R_SUCCESS) {
		dns_db_detachnode(db, &node);
		dns_db_detach(&db);
		return (result);
	}
	vbprintf(2, "found DNSKEY records\n");

	result = dns_db_newversion(db, &ver);
	check_result(result, "dns_db_newversion");
	dns_diff_init(mctx, &diff);

	for (result = dns_rdataset_first(&keyset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&keyset))
	{
		dns_rdata_init(&key);
		dns_rdata_init(&ds);
		dns_rdataset_current(&keyset, &key);
		result = dns_ds_buildrdata(name, &key, DNS_DSDIGEST_SHA1,
					   dsbuf, &ds);
		check_result(result, "dns_ds_buildrdata");

		result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD, name,
					      ttl, &ds, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(&diff, &tuple);

		dns_rdata_reset(&ds);
		result = dns_ds_buildrdata(name, &key, DNS_DSDIGEST_SHA256,
					   dsbuf, &ds);
		check_result(result, "dns_ds_buildrdata");

		result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD, name,
					      ttl, &ds, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(&diff, &tuple);
	}

	result = dns_diff_apply(&diff, db, ver);
	check_result(result, "dns_diff_apply");
	dns_diff_clear(&diff);

	dns_db_closeversion(db, &ver, ISC_TRUE);

	result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_ds, 0, 0,
				     dsset, NULL);
	check_result(result, "dns_db_findrdataset");

	dns_rdataset_disassociate(&keyset);
	dns_db_detachnode(db, &node);
	dns_db_detach(&db);
	return (result);
}

static isc_boolean_t
secure(dns_name_t *name, dns_dbnode_t *node) {
	dns_rdataset_t dsset;
	isc_result_t result;

	if (dns_name_equal(name, gorigin))
		return (ISC_FALSE);

	dns_rdataset_init(&dsset);
	result = dns_db_findrdataset(gdb, node, gversion, dns_rdatatype_ds,
				     0, 0, &dsset, NULL);
	if (dns_rdataset_isassociated(&dsset))
		dns_rdataset_disassociate(&dsset);

	return (ISC_TF(result == ISC_R_SUCCESS));
}

/*%
 * Signs all records at a name.
 */
static void
signname(dns_dbnode_t *node, dns_name_t *name) {
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_rdatasetiter_t *rdsiter;
	isc_boolean_t isdelegation = ISC_FALSE;
	dns_diff_t del, add;
	char namestr[DNS_NAME_FORMATSIZE];

	dns_rdataset_init(&rdataset);
	dns_name_format(name, namestr, sizeof(namestr));

	/*
	 * Determine if this is a delegation point.
	 */
	if (is_delegation(gdb, gversion, gorigin, name, node, NULL))
		isdelegation = ISC_TRUE;

	/*
	 * Now iterate through the rdatasets.
	 */
	dns_diff_init(mctx, &del);
	dns_diff_init(mctx, &add);
	rdsiter = NULL;
	result = dns_db_allrdatasets(gdb, node, gversion, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	result = dns_rdatasetiter_first(rdsiter);
	while (result == ISC_R_SUCCESS) {
		dns_rdatasetiter_current(rdsiter, &rdataset);

		/* If this is a RRSIG set, skip it. */
		if (rdataset.type == dns_rdatatype_rrsig)
			goto skip;

		/*
		 * If this name is a delegation point, skip all records
		 * except NSEC and DS sets.  Otherwise check that there
		 * isn't a DS record.
		 */
		if (isdelegation) {
			if (rdataset.type != nsec_datatype &&
			    rdataset.type != dns_rdatatype_ds)
				goto skip;
		} else if (rdataset.type == dns_rdatatype_ds) {
			char namebuf[DNS_NAME_FORMATSIZE];
			dns_name_format(name, namebuf, sizeof(namebuf));
			fatal("'%s': found DS RRset without NS RRset\n",
			      namebuf);
		}

		signset(&del, &add, node, name, &rdataset);

 skip:
		dns_rdataset_disassociate(&rdataset);
		result = dns_rdatasetiter_next(rdsiter);
	}
	if (result != ISC_R_NOMORE)
		fatal("rdataset iteration for name '%s' failed: %s",
		      namestr, isc_result_totext(result));

	dns_rdatasetiter_destroy(&rdsiter);

	result = dns_diff_applysilently(&del, gdb, gversion);
	if (result != ISC_R_SUCCESS)
		fatal("failed to delete SIGs at node '%s': %s",
		      namestr, isc_result_totext(result));

	result = dns_diff_applysilently(&add, gdb, gversion);
	if (result != ISC_R_SUCCESS)
		fatal("failed to add SIGs at node '%s': %s",
		      namestr, isc_result_totext(result));

	dns_diff_clear(&del);
	dns_diff_clear(&add);
}

static inline isc_boolean_t
active_node(dns_dbnode_t *node) {
	dns_rdatasetiter_t *rdsiter = NULL;
	dns_rdatasetiter_t *rdsiter2 = NULL;
	isc_boolean_t active = ISC_FALSE;
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_rdatatype_t type;
	dns_rdatatype_t covers;
	isc_boolean_t found;

	dns_rdataset_init(&rdataset);
	result = dns_db_allrdatasets(gdb, node, gversion, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	result = dns_rdatasetiter_first(rdsiter);
	while (result == ISC_R_SUCCESS) {
		dns_rdatasetiter_current(rdsiter, &rdataset);
		if (rdataset.type != dns_rdatatype_nsec &&
		    rdataset.type != dns_rdatatype_nsec3 &&
		    rdataset.type != dns_rdatatype_rrsig)
			active = ISC_TRUE;
		dns_rdataset_disassociate(&rdataset);
		if (!active)
			result = dns_rdatasetiter_next(rdsiter);
		else
			result = ISC_R_NOMORE;
	}
	if (result != ISC_R_NOMORE)
		fatal("rdataset iteration failed: %s",
		      isc_result_totext(result));

	if (!active && nsec_datatype == dns_rdatatype_nsec) {
		/*%
		 * The node is empty of everything but NSEC / RRSIG records.
		 */
		for (result = dns_rdatasetiter_first(rdsiter);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(rdsiter)) {
			dns_rdatasetiter_current(rdsiter, &rdataset);
			result = dns_db_deleterdataset(gdb, node, gversion,
						       rdataset.type,
						       rdataset.covers);
			check_result(result, "dns_db_deleterdataset()");
			dns_rdataset_disassociate(&rdataset);
		}
		if (result != ISC_R_NOMORE)
			fatal("rdataset iteration failed: %s",
			      isc_result_totext(result));
	} else {
		/*
		 * Delete RRSIGs for types that no longer exist.
		 */
		result = dns_db_allrdatasets(gdb, node, gversion, 0, &rdsiter2);
		check_result(result, "dns_db_allrdatasets()");
		for (result = dns_rdatasetiter_first(rdsiter);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(rdsiter)) {
			dns_rdatasetiter_current(rdsiter, &rdataset);
			type = rdataset.type;
			covers = rdataset.covers;
			dns_rdataset_disassociate(&rdataset);
			/*
			 * Delete the NSEC chain if we are signing with
			 * NSEC3.
			 */
			if (nsec_datatype == dns_rdatatype_nsec3 &&
			    (type == dns_rdatatype_nsec ||
			     covers == dns_rdatatype_nsec)) {
				result = dns_db_deleterdataset(gdb, node,
							       gversion, type,
							       covers);
				check_result(result,
					   "dns_db_deleterdataset(nsec/rrsig)");
				continue;
			}
			if (type != dns_rdatatype_rrsig)
				continue;
			found = ISC_FALSE;
			for (result = dns_rdatasetiter_first(rdsiter2);
			     !found && result == ISC_R_SUCCESS;
			     result = dns_rdatasetiter_next(rdsiter2)) {
				dns_rdatasetiter_current(rdsiter2, &rdataset);
				if (rdataset.type == covers)
					found = ISC_TRUE;
				dns_rdataset_disassociate(&rdataset);
			}
			if (!found) {
				if (result != ISC_R_NOMORE)
					fatal("rdataset iteration failed: %s",
					      isc_result_totext(result));
				result = dns_db_deleterdataset(gdb, node,
							       gversion, type,
							       covers);
				check_result(result,
					     "dns_db_deleterdataset(rrsig)");
			} else if (result != ISC_R_NOMORE &&
				   result != ISC_R_SUCCESS)
				fatal("rdataset iteration failed: %s",
				      isc_result_totext(result));
		}
		if (result != ISC_R_NOMORE)
			fatal("rdataset iteration failed: %s",
			      isc_result_totext(result));
		dns_rdatasetiter_destroy(&rdsiter2);
	}
	dns_rdatasetiter_destroy(&rdsiter);

	return (active);
}

/*%
 * Extracts the minimum TTL from the SOA record, and the SOA record's TTL.
 */
static void
get_soa_ttls(void) {
	dns_rdataset_t soaset;
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_rdataset_init(&soaset);
	result = dns_db_find(gdb, gorigin, gversion, dns_rdatatype_soa,
			     0, 0, NULL, name, &soaset, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find an SOA at the zone apex: %s",
		      isc_result_totext(result));

	result = dns_rdataset_first(&soaset);
	check_result(result, "dns_rdataset_first");
	dns_rdataset_current(&soaset, &rdata);
	zone_soa_min_ttl = dns_soa_getminimum(&rdata);
	soa_ttl = soaset.ttl;
	dns_rdataset_disassociate(&soaset);
}

/*%
 * Increment (or set if nonzero) the SOA serial
 */
static isc_result_t
setsoaserial(isc_uint32_t serial) {
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_uint32_t old_serial, new_serial;

	result = dns_db_getoriginnode(gdb, &node);
	if (result != ISC_R_SUCCESS)
		return result;

	dns_rdataset_init(&rdataset);

	result = dns_db_findrdataset(gdb, node, gversion,
				     dns_rdatatype_soa, 0,
				     0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = dns_rdataset_first(&rdataset);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	dns_rdataset_current(&rdataset, &rdata);

	old_serial = dns_soa_getserial(&rdata);

	if (serial) {
		/* Set SOA serial to the value provided. */
		new_serial = serial;
	} else {
		/* Increment SOA serial using RFC 1982 arithmetics */
		new_serial = (old_serial + 1) & 0xFFFFFFFF;
		if (new_serial == 0)
			new_serial = 1;
	}

	/* If the new serial is not likely to cause a zone transfer
	 * (a/ixfr) from servers having the old serial, warn the user.
	 *
	 * RFC1982 section 7 defines the maximum increment to be
	 * (2^(32-1))-1.  Using u_int32_t arithmetic, we can do a single
	 * comparison.  (5 - 6 == (2^32)-1, not negative-one)
	 */
	if (new_serial == old_serial ||
	    (new_serial - old_serial) > 0x7fffffffU)
		fprintf(stderr, "%s: warning: Serial number not advanced, "
			"zone may not transfer\n", program);

	dns_soa_setserial(new_serial, &rdata);

	result = dns_db_deleterdataset(gdb, node, gversion,
				       dns_rdatatype_soa, 0);
	check_result(result, "dns_db_deleterdataset");
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = dns_db_addrdataset(gdb, node, gversion,
				    0, &rdataset, 0, NULL);
	check_result(result, "dns_db_addrdataset");
	if (result != ISC_R_SUCCESS)
		goto cleanup;

cleanup:
	dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(gdb, &node);
	dns_rdata_reset(&rdata);

	return (result);
}

/*%
 * Delete any RRSIG records at a node.
 */
static void
cleannode(dns_db_t *db, dns_dbversion_t *dbversion, dns_dbnode_t *node) {
	dns_rdatasetiter_t *rdsiter = NULL;
	dns_rdataset_t set;
	isc_result_t result, dresult;

	if (outputformat != dns_masterformat_text || !disable_zone_check)
		return;

	dns_rdataset_init(&set);
	result = dns_db_allrdatasets(db, node, dbversion, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets");
	result = dns_rdatasetiter_first(rdsiter);
	while (result == ISC_R_SUCCESS) {
		isc_boolean_t destroy = ISC_FALSE;
		dns_rdatatype_t covers = 0;
		dns_rdatasetiter_current(rdsiter, &set);
		if (set.type == dns_rdatatype_rrsig) {
			covers = set.covers;
			destroy = ISC_TRUE;
		}
		dns_rdataset_disassociate(&set);
		result = dns_rdatasetiter_next(rdsiter);
		if (destroy) {
			dresult = dns_db_deleterdataset(db, node, dbversion,
							dns_rdatatype_rrsig,
							covers);
			check_result(dresult, "dns_db_deleterdataset");
		}
	}
	if (result != ISC_R_NOMORE)
		fatal("rdataset iteration failed: %s",
		      isc_result_totext(result));
	dns_rdatasetiter_destroy(&rdsiter);
}

/*%
 * Set up the iterator and global state before starting the tasks.
 */
static void
presign(void) {
	isc_result_t result;

	gdbiter = NULL;
	result = dns_db_createiterator(gdb, 0, &gdbiter);
	check_result(result, "dns_db_createiterator()");
}

/*%
 * Clean up the iterator and global state after the tasks complete.
 */
static void
postsign(void) {
	dns_dbiterator_destroy(&gdbiter);
}

/*%
 * Sign the apex of the zone.
 * Note the origin may not be the first node if there are out of zone
 * records.
 */
static void
signapex(void) {
	dns_dbnode_t *node = NULL;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_result_t result;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	result = dns_dbiterator_seek(gdbiter, gorigin);
	check_result(result, "dns_dbiterator_seek()");
	result = dns_dbiterator_current(gdbiter, &node, name);
	check_dns_dbiterator_current(result);
	signname(node, name);
	dumpnode(name, node);
	cleannode(gdb, gversion, node);
	dns_db_detachnode(gdb, &node);
	result = dns_dbiterator_first(gdbiter);
	if (result == ISC_R_NOMORE)
		finished = ISC_TRUE;
	else if (result != ISC_R_SUCCESS)
		fatal("failure iterating database: %s",
		      isc_result_totext(result));
}

/*%
 * Assigns a node to a worker thread.  This is protected by the master task's
 * lock.
 */
static void
assignwork(isc_task_t *task, isc_task_t *worker) {
	dns_fixedname_t *fname;
	dns_name_t *name;
	dns_dbnode_t *node;
	sevent_t *sevent;
	dns_rdataset_t nsec;
	isc_boolean_t found;
	isc_result_t result;
	static dns_name_t *zonecut = NULL;	/* Protected by namelock. */
	static dns_fixedname_t fzonecut;	/* Protected by namelock. */
	static unsigned int ended = 0;		/* Protected by namelock. */

	if (shuttingdown)
		return;

	LOCK(&namelock);
	if (finished) {
		ended++;
		if (ended == ntasks) {
			isc_task_detach(&task);
			isc_app_shutdown();
		}
		goto unlock;
	}

	fname = isc_mem_get(mctx, sizeof(dns_fixedname_t));
	if (fname == NULL)
		fatal("out of memory");
	dns_fixedname_init(fname);
	name = dns_fixedname_name(fname);
	node = NULL;
	found = ISC_FALSE;
	while (!found) {
		result = dns_dbiterator_current(gdbiter, &node, name);
		check_dns_dbiterator_current(result);
		/*
		 * The origin was handled by signapex().
		 */
		if (dns_name_equal(name, gorigin)) {
			dns_db_detachnode(gdb, &node);
			goto next;
		}
		/*
		 * Sort the zone data from the glue and out-of-zone data.
		 * For NSEC zones nodes with zone data have NSEC records.
		 * For NSEC3 zones the NSEC3 nodes are zone data but
		 * outside of the zone name space.  For the rest we need
		 * to track the bottom of zone cuts.
		 * Nodes which don't need to be signed are dumped here.
		 */
		dns_rdataset_init(&nsec);
		result = dns_db_findrdataset(gdb, node, gversion,
					     nsec_datatype, 0, 0,
					     &nsec, NULL);
		if (dns_rdataset_isassociated(&nsec))
			dns_rdataset_disassociate(&nsec);
		if (result == ISC_R_SUCCESS) {
			found = ISC_TRUE;
		} else if (nsec_datatype == dns_rdatatype_nsec3) {
			if (dns_name_issubdomain(name, gorigin) &&
			    (zonecut == NULL ||
			     !dns_name_issubdomain(name, zonecut))) {
				if (is_delegation(gdb, gversion, gorigin, name, node, NULL)) {
					dns_fixedname_init(&fzonecut);
					zonecut = dns_fixedname_name(&fzonecut);
					dns_name_copy(name, zonecut, NULL);
					if (!OPTOUT(nsec3flags) ||
					    secure(name, node))
						found = ISC_TRUE;
				} else
					found = ISC_TRUE;
			}
		}

		if (!found) {
			dumpnode(name, node);
			dns_db_detachnode(gdb, &node);
		}

 next:
		result = dns_dbiterator_next(gdbiter);
		if (result == ISC_R_NOMORE) {
			finished = ISC_TRUE;
			break;
		} else if (result != ISC_R_SUCCESS)
			fatal("failure iterating database: %s",
			      isc_result_totext(result));
	}
	if (!found) {
		ended++;
		if (ended == ntasks) {
			isc_task_detach(&task);
			isc_app_shutdown();
		}
		isc_mem_put(mctx, fname, sizeof(dns_fixedname_t));
		goto unlock;
	}
	sevent = (sevent_t *)
		 isc_event_allocate(mctx, task, SIGNER_EVENT_WORK,
				    sign, NULL, sizeof(sevent_t));
	if (sevent == NULL)
		fatal("failed to allocate event\n");

	sevent->node = node;
	sevent->fname = fname;
	isc_task_send(worker, ISC_EVENT_PTR(&sevent));
 unlock:
	UNLOCK(&namelock);
}

/*%
 * Start a worker task
 */
static void
startworker(isc_task_t *task, isc_event_t *event) {
	isc_task_t *worker;

	worker = (isc_task_t *)event->ev_arg;
	assignwork(task, worker);
	isc_event_free(&event);
}

/*%
 * Write a node to the output file, and restart the worker task.
 */
static void
writenode(isc_task_t *task, isc_event_t *event) {
	isc_task_t *worker;
	sevent_t *sevent = (sevent_t *)event;

	worker = (isc_task_t *)event->ev_sender;
	dumpnode(dns_fixedname_name(sevent->fname), sevent->node);
	cleannode(gdb, gversion, sevent->node);
	dns_db_detachnode(gdb, &sevent->node);
	isc_mem_put(mctx, sevent->fname, sizeof(dns_fixedname_t));
	assignwork(task, worker);
	isc_event_free(&event);
}

/*%
 *  Sign a database node.
 */
static void
sign(isc_task_t *task, isc_event_t *event) {
	dns_fixedname_t *fname;
	dns_dbnode_t *node;
	sevent_t *sevent, *wevent;

	sevent = (sevent_t *)event;
	node = sevent->node;
	fname = sevent->fname;
	isc_event_free(&event);

	signname(node, dns_fixedname_name(fname));
	wevent = (sevent_t *)
		 isc_event_allocate(mctx, task, SIGNER_EVENT_WRITE,
				    writenode, NULL, sizeof(sevent_t));
	if (wevent == NULL)
		fatal("failed to allocate event\n");
	wevent->node = node;
	wevent->fname = fname;
	isc_task_send(master, ISC_EVENT_PTR(&wevent));
}

/*%
 * Update / remove the DS RRset.  Preserve RRSIG(DS) if possible.
 */
static void
add_ds(dns_name_t *name, dns_dbnode_t *node, isc_uint32_t nsttl) {
	dns_rdataset_t dsset;
	dns_rdataset_t sigdsset;
	isc_result_t result;

	dns_rdataset_init(&dsset);
	dns_rdataset_init(&sigdsset);
	result = dns_db_findrdataset(gdb, node, gversion,
				     dns_rdatatype_ds,
				     0, 0, &dsset, &sigdsset);
	if (result == ISC_R_SUCCESS) {
		dns_rdataset_disassociate(&dsset);
		result = dns_db_deleterdataset(gdb, node, gversion,
					       dns_rdatatype_ds, 0);
		check_result(result, "dns_db_deleterdataset");
	}

	result = loadds(name, nsttl, &dsset);
	if (result == ISC_R_SUCCESS) {
		result = dns_db_addrdataset(gdb, node, gversion, 0,
					    &dsset, 0, NULL);
		check_result(result, "dns_db_addrdataset");
		dns_rdataset_disassociate(&dsset);
		if (dns_rdataset_isassociated(&sigdsset))
			dns_rdataset_disassociate(&sigdsset);
	} else if (dns_rdataset_isassociated(&sigdsset)) {
		result = dns_db_deleterdataset(gdb, node, gversion,
					       dns_rdatatype_rrsig,
					       dns_rdatatype_ds);
		check_result(result, "dns_db_deleterdataset");
		dns_rdataset_disassociate(&sigdsset);
	}
}

/*
 * Remove records of the given type and their signatures.
 */
static void
remove_records(dns_dbnode_t *node, dns_rdatatype_t which,
	       isc_boolean_t checknsec)
{
	isc_result_t result;
	dns_rdatatype_t type, covers;
	dns_rdatasetiter_t *rdsiter = NULL;
	dns_rdataset_t rdataset;

	dns_rdataset_init(&rdataset);

	/*
	 * Delete any records of the given type at the apex.
	 */
	result = dns_db_allrdatasets(gdb, node, gversion, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	for (result = dns_rdatasetiter_first(rdsiter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(rdsiter)) {
		dns_rdatasetiter_current(rdsiter, &rdataset);
		type = rdataset.type;
		covers = rdataset.covers;
		dns_rdataset_disassociate(&rdataset);
		if (type == which || covers == which) {
			if (which == dns_rdatatype_nsec &&
			    checknsec && !update_chain)
				fatal("Zone contains NSEC records.  Use -u "
				      "to update to NSEC3.");
			if (which == dns_rdatatype_nsec3param &&
			    checknsec && !update_chain)
				fatal("Zone contains NSEC3 chains.  Use -u "
				      "to update to NSEC.");
			result = dns_db_deleterdataset(gdb, node, gversion,
						       type, covers);
			check_result(result, "dns_db_deleterdataset()");
			continue;
		}
	}
	dns_rdatasetiter_destroy(&rdsiter);
}

/*
 * Remove signatures covering the given type.  If type == 0,
 * then remove all signatures, unless this is a delegation, in
 * which case remove all signatures except for DS or nsec_datatype
 */
static void
remove_sigs(dns_dbnode_t *node, isc_boolean_t delegation,
	    dns_rdatatype_t which)
{
	isc_result_t result;
	dns_rdatatype_t type, covers;
	dns_rdatasetiter_t *rdsiter = NULL;
	dns_rdataset_t rdataset;

	dns_rdataset_init(&rdataset);
	result = dns_db_allrdatasets(gdb, node, gversion, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	for (result = dns_rdatasetiter_first(rdsiter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(rdsiter)) {
		dns_rdatasetiter_current(rdsiter, &rdataset);
		type = rdataset.type;
		covers = rdataset.covers;
		dns_rdataset_disassociate(&rdataset);

		if (type != dns_rdatatype_rrsig)
			continue;

		if (which == 0 && delegation &&
		    (dns_rdatatype_atparent(covers) ||
		     (nsec_datatype == dns_rdatatype_nsec &&
		      covers == nsec_datatype)))
			continue;

		if (which != 0 && covers != which)
			continue;

		result = dns_db_deleterdataset(gdb, node, gversion,
					       type, covers);
		check_result(result, "dns_db_deleterdataset()");
	}
	dns_rdatasetiter_destroy(&rdsiter);
}

/*%
 * Generate NSEC records for the zone and remove NSEC3/NSEC3PARAM records.
 */
static void
nsecify(void) {
	dns_dbiterator_t *dbiter = NULL;
	dns_dbnode_t *node = NULL, *nextnode = NULL;
	dns_fixedname_t fname, fnextname, fzonecut;
	dns_name_t *name, *nextname, *zonecut;
	dns_rdataset_t rdataset;
	dns_rdatasetiter_t *rdsiter = NULL;
	dns_rdatatype_t type, covers;
	isc_boolean_t done = ISC_FALSE;
	isc_result_t result;
	isc_uint32_t nsttl = 0;

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_fixedname_init(&fnextname);
	nextname = dns_fixedname_name(&fnextname);
	dns_fixedname_init(&fzonecut);
	zonecut = NULL;

	/*
	 * Remove any NSEC3 chains.
	 */
	result = dns_db_createiterator(gdb, DNS_DB_NSEC3ONLY, &dbiter);
	check_result(result, "dns_db_createiterator()");
	for (result = dns_dbiterator_first(dbiter);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(dbiter)) {
		result = dns_dbiterator_current(dbiter, &node, name);
		check_dns_dbiterator_current(result);
		result = dns_db_allrdatasets(gdb, node, gversion, 0, &rdsiter);
		check_result(result, "dns_db_allrdatasets()");
		for (result = dns_rdatasetiter_first(rdsiter);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(rdsiter)) {
			dns_rdatasetiter_current(rdsiter, &rdataset);
			type = rdataset.type;
			covers = rdataset.covers;
			dns_rdataset_disassociate(&rdataset);
			result = dns_db_deleterdataset(gdb, node, gversion,
						       type, covers);
			check_result(result,
				     "dns_db_deleterdataset(nsec3param/rrsig)");
		}
		dns_rdatasetiter_destroy(&rdsiter);
		dns_db_detachnode(gdb, &node);
	}
	dns_dbiterator_destroy(&dbiter);

	result = dns_db_createiterator(gdb, DNS_DB_NONSEC3, &dbiter);
	check_result(result, "dns_db_createiterator()");

	result = dns_dbiterator_first(dbiter);
	check_result(result, "dns_dbiterator_first()");

	while (!done) {
		result = dns_dbiterator_current(dbiter, &node, name);
		check_dns_dbiterator_current(result);
		/*
		 * Skip out-of-zone records.
		 */
		if (!dns_name_issubdomain(name, gorigin)) {
			result = dns_dbiterator_next(dbiter);
			if (result == ISC_R_NOMORE)
				done = ISC_TRUE;
			else
				check_result(result, "dns_dbiterator_next()");
			dns_db_detachnode(gdb, &node);
			continue;
		}

		if (dns_name_equal(name, gorigin))
			remove_records(node, dns_rdatatype_nsec3param,
				       ISC_TRUE);

		if (is_delegation(gdb, gversion, gorigin, name, node, &nsttl)) {
			zonecut = dns_fixedname_name(&fzonecut);
			dns_name_copy(name, zonecut, NULL);
			remove_sigs(node, ISC_TRUE, 0);
			if (generateds)
				add_ds(name, node, nsttl);
		}

		result = dns_dbiterator_next(dbiter);
		nextnode = NULL;
		while (result == ISC_R_SUCCESS) {
			isc_boolean_t active = ISC_FALSE;
			result = dns_dbiterator_current(dbiter, &nextnode,
							nextname);
			check_dns_dbiterator_current(result);
			active = active_node(nextnode);
			if (!active) {
				dns_db_detachnode(gdb, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			if (!dns_name_issubdomain(nextname, gorigin) ||
			    (zonecut != NULL &&
			     dns_name_issubdomain(nextname, zonecut)))
			{
				remove_sigs(nextnode, ISC_FALSE, 0);
				remove_records(nextnode, dns_rdatatype_nsec,
					       ISC_FALSE);
				dns_db_detachnode(gdb, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			dns_db_detachnode(gdb, &nextnode);
			break;
		}
		if (result == ISC_R_NOMORE) {
			dns_name_clone(gorigin, nextname);
			done = ISC_TRUE;
		} else if (result != ISC_R_SUCCESS)
			fatal("iterating through the database failed: %s",
			      isc_result_totext(result));
		dns_dbiterator_pause(dbiter);
		result = dns_nsec_build(gdb, gversion, node, nextname,
					zone_soa_min_ttl);
		check_result(result, "dns_nsec_build()");
		dns_db_detachnode(gdb, &node);
	}

	dns_dbiterator_destroy(&dbiter);
}

static void
addnsec3param(const unsigned char *salt, size_t salt_len,
	      dns_iterations_t iterations)
{
	dns_dbnode_t *node = NULL;
	dns_rdata_nsec3param_t nsec3param;
	unsigned char nsec3parambuf[5 + 255];
	dns_rdatalist_t rdatalist;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_buffer_t b;
	isc_result_t result;

	dns_rdataset_init(&rdataset);

	nsec3param.common.rdclass = gclass;
	nsec3param.common.rdtype = dns_rdatatype_nsec3param;
	ISC_LINK_INIT(&nsec3param.common, link);
	nsec3param.mctx = NULL;
	nsec3param.flags = 0;
	nsec3param.hash = unknownalg ? DNS_NSEC3_UNKNOWNALG : dns_hash_sha1;
	nsec3param.iterations = iterations;
	nsec3param.salt_length = (unsigned char)salt_len;
	DE_CONST(salt, nsec3param.salt);

	isc_buffer_init(&b, nsec3parambuf, sizeof(nsec3parambuf));
	result = dns_rdata_fromstruct(&rdata, gclass,
				      dns_rdatatype_nsec3param,
				      &nsec3param, &b);
	check_result(result, "dns_rdata_fromstruct()");
	dns_rdatalist_init(&rdatalist);
	rdatalist.rdclass = rdata.rdclass;
	rdatalist.type = rdata.type;
	ISC_LIST_APPEND(rdatalist.rdata, &rdata, link);
	result = dns_rdatalist_tordataset(&rdatalist, &rdataset);
	check_result(result, "dns_rdatalist_tordataset()");

	result = dns_db_findnode(gdb, gorigin, ISC_TRUE, &node);
	check_result(result, "dns_db_find(gorigin)");

	/*
	 * Delete any current NSEC3PARAM records.
	 */
	result = dns_db_deleterdataset(gdb, node, gversion,
				       dns_rdatatype_nsec3param, 0);
	if (result == DNS_R_UNCHANGED)
		result = ISC_R_SUCCESS;
	check_result(result, "dddnsec3param: dns_db_deleterdataset()");

	result = dns_db_addrdataset(gdb, node, gversion, 0, &rdataset,
				    DNS_DBADD_MERGE, NULL);
	if (result == DNS_R_UNCHANGED)
		result = ISC_R_SUCCESS;
	check_result(result, "addnsec3param: dns_db_addrdataset()");
	dns_db_detachnode(gdb, &node);
}

static void
addnsec3(dns_name_t *name, dns_dbnode_t *node,
	 const unsigned char *salt, size_t salt_len,
	 unsigned int iterations, hashlist_t *hashlist,
	 dns_ttl_t ttl)
{
	unsigned char hash[NSEC3_MAX_HASH_LENGTH];
	const unsigned char *nexthash;
	unsigned char nsec3buffer[DNS_NSEC3_BUFFERSIZE];
	dns_fixedname_t hashname;
	dns_rdatalist_t rdatalist;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;
	dns_dbnode_t *nsec3node = NULL;
	char namebuf[DNS_NAME_FORMATSIZE];
	size_t hash_len;

	dns_name_format(name, namebuf, sizeof(namebuf));

	dns_fixedname_init(&hashname);
	dns_rdataset_init(&rdataset);

	dns_name_downcase(name, name, NULL);
	result = dns_nsec3_hashname(&hashname, hash, &hash_len,
				    name, gorigin, dns_hash_sha1, iterations,
				    salt, salt_len);
	check_result(result, "addnsec3: dns_nsec3_hashname()");
	nexthash = hashlist_findnext(hashlist, hash);
	result = dns_nsec3_buildrdata(gdb, gversion, node,
				      unknownalg ?
					  DNS_NSEC3_UNKNOWNALG : dns_hash_sha1,
				      nsec3flags, iterations,
				      salt, salt_len,
				      nexthash, ISC_SHA1_DIGESTLENGTH,
				      nsec3buffer, &rdata);
	check_result(result, "addnsec3: dns_nsec3_buildrdata()");
	dns_rdatalist_init(&rdatalist);
	rdatalist.rdclass = rdata.rdclass;
	rdatalist.type = rdata.type;
	rdatalist.ttl = ttl;
	ISC_LIST_APPEND(rdatalist.rdata, &rdata, link);
	result = dns_rdatalist_tordataset(&rdatalist, &rdataset);
	check_result(result, "dns_rdatalist_tordataset()");
	result = dns_db_findnsec3node(gdb, dns_fixedname_name(&hashname),
				      ISC_TRUE, &nsec3node);
	check_result(result, "addnsec3: dns_db_findnode()");
	result = dns_db_addrdataset(gdb, nsec3node, gversion, 0, &rdataset,
				    0, NULL);
	if (result == DNS_R_UNCHANGED)
		result = ISC_R_SUCCESS;
	check_result(result, "addnsec3: dns_db_addrdataset()");
	dns_db_detachnode(gdb, &nsec3node);
}

/*%
 * Clean out NSEC3 record and RRSIG(NSEC3) that are not in the hash list.
 *
 * Extract the hash from the first label of 'name' then see if it
 * is in hashlist.  If 'name' is not in the hashlist then delete the
 * any NSEC3 records which have the same parameters as the chain we
 * are building.
 *
 * XXXMPA Should we also check that it of the form <hash>.<origin>?
 */
static void
nsec3clean(dns_name_t *name, dns_dbnode_t *node,
	   unsigned int hashalg, unsigned int iterations,
	   const unsigned char *salt, size_t salt_len, hashlist_t *hashlist)
{
	dns_label_t label;
	dns_rdata_nsec3_t nsec3;
	dns_rdata_t rdata, delrdata;
	dns_rdatalist_t rdatalist;
	dns_rdataset_t rdataset, delrdataset;
	isc_boolean_t delete_rrsigs = ISC_FALSE;
	isc_buffer_t target;
	isc_result_t result;
	unsigned char hash[NSEC3_MAX_HASH_LENGTH + 1];
	isc_boolean_t exists;

	/*
	 * Get the first label.
	 */
	dns_name_getlabel(name, 0, &label);

	/*
	 * We want just the label contents.
	 */
	isc_region_consume(&label, 1);

	/*
	 * Decode base32hex string.
	 */
	isc_buffer_init(&target, hash, sizeof(hash) - 1);
	result = isc_base32hex_decoderegion(&label, &target);
	if (result != ISC_R_SUCCESS)
		return;

	hash[isc_buffer_usedlength(&target)] = 0;

	exists = hashlist_exists(hashlist, hash);

	/*
	 * Verify that the NSEC3 parameters match the current ones
	 * otherwise we are dealing with a different NSEC3 chain.
	 */
	dns_rdataset_init(&rdataset);
	dns_rdataset_init(&delrdataset);

	result = dns_db_findrdataset(gdb, node, gversion, dns_rdatatype_nsec3,
				     0, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS)
		return;

	/*
	 * Delete any NSEC3 records which are not part of the current
	 * NSEC3 chain.
	 */
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_init(&rdata);
		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &nsec3, NULL);
		check_result(result, "dns_rdata_tostruct");
		if (exists && nsec3.hash == hashalg &&
		    nsec3.iterations == iterations &&
		    nsec3.salt_length == salt_len &&
		    isc_safe_memequal(nsec3.salt, salt, salt_len))
			continue;
		dns_rdatalist_init(&rdatalist);
		rdatalist.rdclass = rdata.rdclass;
		rdatalist.type = rdata.type;
		rdatalist.ttl = rdataset.ttl;
		dns_rdata_init(&delrdata);
		dns_rdata_clone(&rdata, &delrdata);
		ISC_LIST_APPEND(rdatalist.rdata, &delrdata, link);
		result = dns_rdatalist_tordataset(&rdatalist, &delrdataset);
		check_result(result, "dns_rdatalist_tordataset()");
		result = dns_db_subtractrdataset(gdb, node, gversion,
						 &delrdataset, 0, NULL);
		dns_rdataset_disassociate(&delrdataset);
		if (result != ISC_R_SUCCESS && result != DNS_R_NXRRSET)
			check_result(result, "dns_db_subtractrdataset(NSEC3)");
		delete_rrsigs = ISC_TRUE;
	}
	dns_rdataset_disassociate(&rdataset);
	if (result != ISC_R_NOMORE)
		check_result(result, "dns_rdataset_first/next");

	if (!delete_rrsigs)
		return;
	/*
	 * Delete the NSEC3 RRSIGs
	 */
	result = dns_db_deleterdataset(gdb, node, gversion,
				       dns_rdatatype_rrsig,
				       dns_rdatatype_nsec3);
	if (result != ISC_R_SUCCESS && result != DNS_R_UNCHANGED)
		check_result(result, "dns_db_deleterdataset(RRSIG(NSEC3))");
}

static void
rrset_remove_duplicates(dns_name_t *name, dns_rdataset_t *rdataset,
			dns_diff_t *diff)
{
	dns_difftuple_t *tuple = NULL;
	isc_result_t result;
	unsigned int count1 = 0;
	dns_rdataset_t tmprdataset;

	dns_rdataset_init(&tmprdataset);
	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset)) {
		dns_rdata_t rdata1 = DNS_RDATA_INIT;
		unsigned int count2 = 0;

		count1++;
		dns_rdataset_current(rdataset, &rdata1);
		dns_rdataset_clone(rdataset, &tmprdataset);
		for (result = dns_rdataset_first(&tmprdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&tmprdataset)) {
			dns_rdata_t rdata2 = DNS_RDATA_INIT;
			count2++;
			if (count1 >= count2)
				continue;
			dns_rdataset_current(&tmprdataset, &rdata2);
			if (dns_rdata_casecompare(&rdata1, &rdata2) == 0) {
				result = dns_difftuple_create(mctx,
							      DNS_DIFFOP_DEL,
							      name,
							      rdataset->ttl,
							      &rdata2, &tuple);
				check_result(result, "dns_difftuple_create");
				dns_diff_append(diff, &tuple);
			}
		}
		dns_rdataset_disassociate(&tmprdataset);
	}
}

static void
remove_duplicates(void) {
	isc_result_t result;
	dns_dbiterator_t *dbiter = NULL;
	dns_rdatasetiter_t *rdsiter = NULL;
	dns_diff_t diff;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_fixedname_t fname;
	dns_name_t *name;

	dns_diff_init(mctx, &diff);
	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_rdataset_init(&rdataset);

	result = dns_db_createiterator(gdb, 0, &dbiter);
	check_result(result, "dns_db_createiterator()");

	for (result = dns_dbiterator_first(dbiter);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(dbiter)) {

		result = dns_dbiterator_current(dbiter, &node, name);
		check_dns_dbiterator_current(result);
		result = dns_db_allrdatasets(gdb, node, gversion, 0, &rdsiter);
		check_result(result, "dns_db_allrdatasets()");
		for (result = dns_rdatasetiter_first(rdsiter);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(rdsiter)) {
			dns_rdatasetiter_current(rdsiter, &rdataset);
			rrset_remove_duplicates(name, &rdataset, &diff);
			dns_rdataset_disassociate(&rdataset);
		}
		if (result != ISC_R_NOMORE)
			fatal("rdatasets iteration failed.");
		dns_rdatasetiter_destroy(&rdsiter);
		dns_db_detachnode(gdb, &node);
	}
	if (result != ISC_R_NOMORE)
		fatal("zone iteration failed.");

	if (!ISC_LIST_EMPTY(diff.tuples)) {
		result = dns_diff_applysilently(&diff, gdb, gversion);
		check_result(result, "dns_diff_applysilently");
	}
	dns_diff_clear(&diff);
	dns_dbiterator_destroy(&dbiter);
}

/*
 * Generate NSEC3 records for the zone.
 */
static void
nsec3ify(unsigned int hashalg, dns_iterations_t iterations,
	 const unsigned char *salt, size_t salt_len, hashlist_t *hashlist)
{
	dns_dbiterator_t *dbiter = NULL;
	dns_dbnode_t *node = NULL, *nextnode = NULL;
	dns_fixedname_t fname, fnextname, fzonecut;
	dns_name_t *name, *nextname, *zonecut;
	dns_rdataset_t rdataset;
	int order;
	isc_boolean_t active;
	isc_boolean_t done = ISC_FALSE;
	isc_result_t result;
	isc_uint32_t nsttl = 0;
	unsigned int count, nlabels;

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_fixedname_init(&fnextname);
	nextname = dns_fixedname_name(&fnextname);
	dns_fixedname_init(&fzonecut);
	zonecut = NULL;

	/*
	 * Walk the zone generating the hash names.
	 */
	result = dns_db_createiterator(gdb, DNS_DB_NONSEC3, &dbiter);
	check_result(result, "dns_db_createiterator()");

	result = dns_dbiterator_first(dbiter);
	check_result(result, "dns_dbiterator_first()");

	while (!done) {
		result = dns_dbiterator_current(dbiter, &node, name);
		check_dns_dbiterator_current(result);
		/*
		 * Skip out-of-zone records.
		 */
		if (!dns_name_issubdomain(name, gorigin)) {
			result = dns_dbiterator_next(dbiter);
			if (result == ISC_R_NOMORE)
				done = ISC_TRUE;
			else
				check_result(result, "dns_dbiterator_next()");
			dns_db_detachnode(gdb, &node);
			continue;
		}

		if (dns_name_equal(name, gorigin))
			remove_records(node, dns_rdatatype_nsec, ISC_TRUE);

		result = dns_dbiterator_next(dbiter);
		nextnode = NULL;
		while (result == ISC_R_SUCCESS) {
			result = dns_dbiterator_current(dbiter, &nextnode,
							nextname);
			check_dns_dbiterator_current(result);
			active = active_node(nextnode);
			if (!active) {
				dns_db_detachnode(gdb, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			if (!dns_name_issubdomain(nextname, gorigin) ||
			    (zonecut != NULL &&
			     dns_name_issubdomain(nextname, zonecut))) {
				remove_sigs(nextnode, ISC_FALSE, 0);
				dns_db_detachnode(gdb, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			if (is_delegation(gdb, gversion, gorigin,
					  nextname, nextnode, &nsttl))
			{
				zonecut = dns_fixedname_name(&fzonecut);
				dns_name_copy(nextname, zonecut, NULL);
				remove_sigs(nextnode, ISC_TRUE, 0);
				if (generateds)
					add_ds(nextname, nextnode, nsttl);
				if (OPTOUT(nsec3flags) &&
				    !secure(nextname, nextnode)) {
					dns_db_detachnode(gdb, &nextnode);
					result = dns_dbiterator_next(dbiter);
					continue;
				}
			}
			dns_db_detachnode(gdb, &nextnode);
			break;
		}
		if (result == ISC_R_NOMORE) {
			dns_name_copy(gorigin, nextname, NULL);
			done = ISC_TRUE;
		} else if (result != ISC_R_SUCCESS)
			fatal("iterating through the database failed: %s",
			      isc_result_totext(result));
		dns_name_downcase(name, name, NULL);
		hashlist_add_dns_name(hashlist, name, hashalg, iterations,
				      salt, salt_len, ISC_FALSE);
		dns_db_detachnode(gdb, &node);
		/*
		 * Add hashs for empty nodes.  Use closest encloser logic.
		 * The closest encloser either has data or is a empty
		 * node for another <name,nextname> span so we don't add
		 * it here.  Empty labels on nextname are within the span.
		 */
		dns_name_downcase(nextname, nextname, NULL);
		dns_name_fullcompare(name, nextname, &order, &nlabels);
		addnowildcardhash(hashlist, name, hashalg, iterations,
				  salt, salt_len);
		count = dns_name_countlabels(nextname);
		while (count > nlabels + 1) {
			count--;
			dns_name_split(nextname, count, NULL, nextname);
			hashlist_add_dns_name(hashlist, nextname, hashalg,
					      iterations, salt, salt_len,
					      ISC_FALSE);
			addnowildcardhash(hashlist, nextname, hashalg,
					  iterations, salt, salt_len);
		}
	}
	dns_dbiterator_destroy(&dbiter);

	/*
	 * We have all the hashes now so we can sort them.
	 */
	hashlist_sort(hashlist);

	/*
	 * Check for duplicate hashes.  If found the salt needs to
	 * be changed.
	 */
	if (hashlist_hasdup(hashlist))
		fatal("Duplicate hash detected. Pick a different salt.");

	/*
	 * Generate the nsec3 records.
	 */
	zonecut = NULL;
	done = ISC_FALSE;

	addnsec3param(salt, salt_len, iterations);

	/*
	 * Clean out NSEC3 records which don't match this chain.
	 */
	result = dns_db_createiterator(gdb, DNS_DB_NSEC3ONLY, &dbiter);
	check_result(result, "dns_db_createiterator()");

	for (result = dns_dbiterator_first(dbiter);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(dbiter)) {
		result = dns_dbiterator_current(dbiter, &node, name);
		check_dns_dbiterator_current(result);
		nsec3clean(name, node, hashalg, iterations, salt, salt_len,
			   hashlist);
		dns_db_detachnode(gdb, &node);
	}
	dns_dbiterator_destroy(&dbiter);

	/*
	 * Generate / complete the new chain.
	 */
	result = dns_db_createiterator(gdb, DNS_DB_NONSEC3, &dbiter);
	check_result(result, "dns_db_createiterator()");

	result = dns_dbiterator_first(dbiter);
	check_result(result, "dns_dbiterator_first()");

	while (!done) {
		result = dns_dbiterator_current(dbiter, &node, name);
		check_dns_dbiterator_current(result);
		/*
		 * Skip out-of-zone records.
		 */
		if (!dns_name_issubdomain(name, gorigin)) {
			result = dns_dbiterator_next(dbiter);
			if (result == ISC_R_NOMORE)
				done = ISC_TRUE;
			else
				check_result(result, "dns_dbiterator_next()");
			dns_db_detachnode(gdb, &node);
			continue;
		}
		result = dns_dbiterator_next(dbiter);
		nextnode = NULL;
		while (result == ISC_R_SUCCESS) {
			result = dns_dbiterator_current(dbiter, &nextnode,
							nextname);
			check_dns_dbiterator_current(result);
			active = active_node(nextnode);
			if (!active) {
				dns_db_detachnode(gdb, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			if (!dns_name_issubdomain(nextname, gorigin) ||
			    (zonecut != NULL &&
			     dns_name_issubdomain(nextname, zonecut))) {
				dns_db_detachnode(gdb, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			if (is_delegation(gdb, gversion, gorigin,
					  nextname, nextnode, NULL))
			{
				zonecut = dns_fixedname_name(&fzonecut);
				dns_name_copy(nextname, zonecut, NULL);
				if (OPTOUT(nsec3flags) &&
				    !secure(nextname, nextnode)) {
					dns_db_detachnode(gdb, &nextnode);
					result = dns_dbiterator_next(dbiter);
					continue;
				}
			}
			dns_db_detachnode(gdb, &nextnode);
			break;
		}
		if (result == ISC_R_NOMORE) {
			dns_name_copy(gorigin, nextname, NULL);
			done = ISC_TRUE;
		} else if (result != ISC_R_SUCCESS)
			fatal("iterating through the database failed: %s",
			      isc_result_totext(result));
		/*
		 * We need to pause here to release the lock on the database.
		 */
		dns_dbiterator_pause(dbiter);
		addnsec3(name, node, salt, salt_len, iterations,
			 hashlist, zone_soa_min_ttl);
		dns_db_detachnode(gdb, &node);
		/*
		 * Add NSEC3's for empty nodes.  Use closest encloser logic.
		 */
		dns_name_fullcompare(name, nextname, &order, &nlabels);
		count = dns_name_countlabels(nextname);
		while (count > nlabels + 1) {
			count--;
			dns_name_split(nextname, count, NULL, nextname);
			addnsec3(nextname, NULL, salt, salt_len,
				 iterations, hashlist, zone_soa_min_ttl);
		}
	}
	dns_dbiterator_destroy(&dbiter);
}

/*%
 * Load the zone file from disk
 */
static void
loadzone(char *file, char *origin, dns_rdataclass_t rdclass, dns_db_t **db) {
	isc_buffer_t b;
	int len;
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_result_t result;

	len = strlen(origin);
	isc_buffer_init(&b, origin, len);
	isc_buffer_add(&b, len);

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	result = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("failed converting name '%s' to dns format: %s",
		      origin, isc_result_totext(result));

	result = dns_db_create(mctx, "rbt", name, dns_dbtype_zone,
			       rdclass, 0, NULL, db);
	check_result(result, "dns_db_create()");

	result = dns_db_load2(*db, file, inputformat);
	if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE)
		fatal("failed loading zone from '%s': %s",
		      file, isc_result_totext(result));
}

/*%
 * Finds all public zone keys in the zone, and attempts to load the
 * private keys from disk.
 */
static void
loadzonekeys(isc_boolean_t preserve_keys, isc_boolean_t load_public) {
	dns_dbnode_t *node;
	dns_dbversion_t *currentversion = NULL;
	isc_result_t result;
	dns_rdataset_t rdataset, keysigs, soasigs;

	node = NULL;
	result = dns_db_findnode(gdb, gorigin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find the zone's origin: %s",
		      isc_result_totext(result));

	dns_db_currentversion(gdb, &currentversion);

	dns_rdataset_init(&rdataset);
	dns_rdataset_init(&soasigs);
	dns_rdataset_init(&keysigs);

	/* Make note of the keys which signed the SOA, if any */
	result = dns_db_findrdataset(gdb, node, currentversion,
				     dns_rdatatype_soa, 0, 0,
				     &rdataset, &soasigs);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	/* Preserve the TTL of the DNSKEY RRset, if any */
	dns_rdataset_disassociate(&rdataset);
	result = dns_db_findrdataset(gdb, node, currentversion,
				     dns_rdatatype_dnskey, 0, 0,
				     &rdataset, &keysigs);

	if (result != ISC_R_SUCCESS)
		goto cleanup;

	if (set_keyttl && keyttl != rdataset.ttl) {
		fprintf(stderr, "User-specified TTL (%d) conflicts "
				"with existing DNSKEY RRset TTL.\n",
				keyttl);
		fprintf(stderr, "Imported keys will use the RRSet "
				"TTL (%d) instead.\n",
				rdataset.ttl);
	}
	keyttl = rdataset.ttl;

	/* Load keys corresponding to the existing DNSKEY RRset. */
	result = dns_dnssec_keylistfromrdataset(gorigin, directory, mctx,
						&rdataset, &keysigs, &soasigs,
						preserve_keys, load_public,
						&keylist);
	if (result != ISC_R_SUCCESS)
		fatal("failed to load the zone keys: %s",
		      isc_result_totext(result));

 cleanup:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (dns_rdataset_isassociated(&keysigs))
		dns_rdataset_disassociate(&keysigs);
	if (dns_rdataset_isassociated(&soasigs))
		dns_rdataset_disassociate(&soasigs);
	dns_db_detachnode(gdb, &node);
	dns_db_closeversion(gdb, &currentversion, ISC_FALSE);
}

static void
loadexplicitkeys(char *keyfiles[], int n, isc_boolean_t setksk) {
	isc_result_t result;
	int i;

	for (i = 0; i < n; i++) {
		dns_dnsseckey_t *key = NULL;
		dst_key_t *newkey = NULL;

		result = dst_key_fromnamedfile(keyfiles[i], directory,
					       DST_TYPE_PUBLIC |
					       DST_TYPE_PRIVATE,
					       mctx, &newkey);
		if (result != ISC_R_SUCCESS)
			fatal("cannot load dnskey %s: %s", keyfiles[i],
			      isc_result_totext(result));

		if (!dns_name_equal(gorigin, dst_key_name(newkey)))
			fatal("key %s not at origin\n", keyfiles[i]);

		if (!dst_key_isprivate(newkey))
			fatal("cannot sign zone with non-private dnskey %s",
			      keyfiles[i]);

		/* Skip any duplicates */
		for (key = ISC_LIST_HEAD(keylist);
		     key != NULL;
		     key = ISC_LIST_NEXT(key, link)) {
			if (dst_key_id(key->key) == dst_key_id(newkey) &&
			    dst_key_alg(key->key) == dst_key_alg(newkey))
				break;
		}

		if (key == NULL) {
			/* We haven't seen this key before */
			dns_dnsseckey_create(mctx, &newkey, &key);
			ISC_LIST_APPEND(keylist, key, link);
			key->source = dns_keysource_user;
		} else {
			dst_key_free(&key->key);
			key->key = newkey;
		}

		key->force_publish = ISC_TRUE;
		key->force_sign = ISC_TRUE;

		if (setksk)
			key->ksk = ISC_TRUE;
	}
}

static void
report(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	putc('\n', stderr);
}

static void
build_final_keylist(void) {
	isc_result_t result;
	dns_dbversion_t *ver = NULL;
	dns_diff_t diff;
	dns_dnsseckeylist_t matchkeys;
	char name[DNS_NAME_FORMATSIZE];

	/*
	 * Find keys that match this zone in the key repository.
	 */
	ISC_LIST_INIT(matchkeys);
	result = dns_dnssec_findmatchingkeys(gorigin, directory,
					     mctx, &matchkeys);
	if (result == ISC_R_NOTFOUND)
		result = ISC_R_SUCCESS;
	check_result(result, "dns_dnssec_findmatchingkeys");

	result = dns_db_newversion(gdb, &ver);
	check_result(result, "dns_db_newversion");

	dns_diff_init(mctx, &diff);

	/*
	 * Update keylist with information from from the key repository.
	 */
	dns_dnssec_updatekeys(&keylist, &matchkeys, NULL, gorigin, keyttl,
			      &diff, ignore_kskflag, mctx, report);

	dns_name_format(gorigin, name, sizeof(name));

	result = dns_diff_applysilently(&diff, gdb, ver);
	if (result != ISC_R_SUCCESS)
		fatal("failed to update DNSKEY RRset at node '%s': %s",
		      name, isc_result_totext(result));

	dns_db_closeversion(gdb, &ver, ISC_TRUE);

	dns_diff_clear(&diff);
}

static void
warnifallksk(dns_db_t *db) {
	dns_dbversion_t *currentversion = NULL;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;
	dns_rdata_dnskey_t dnskey;
	isc_boolean_t have_non_ksk = ISC_FALSE;

	dns_db_currentversion(db, &currentversion);

	result = dns_db_findnode(db, gorigin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find the zone's origin: %s",
		      isc_result_totext(result));

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, currentversion,
				     dns_rdatatype_dnskey, 0, 0, &rdataset,
				     NULL);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find keys at the zone apex: %s",
		      isc_result_totext(result));
	result = dns_rdataset_first(&rdataset);
	check_result(result, "dns_rdataset_first");
	while (result == ISC_R_SUCCESS) {
		dns_rdata_reset(&rdata);
		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &dnskey, NULL);
		check_result(result, "dns_rdata_tostruct");
		if ((dnskey.flags & DNS_KEYFLAG_KSK) == 0) {
			have_non_ksk = ISC_TRUE;
			result = ISC_R_NOMORE;
		} else
			result = dns_rdataset_next(&rdataset);
		dns_rdata_freestruct(&dnskey);
	}
	dns_rdataset_disassociate(&rdataset);
	dns_db_detachnode(db, &node);
	dns_db_closeversion(db, &currentversion, ISC_FALSE);
	if (!have_non_ksk && !ignore_kskflag) {
		if (disable_zone_check)
			fprintf(stderr, "%s: warning: No non-KSK DNSKEY found; "
				"supply a ZSK or use '-z'.\n",
				program);
		else
			fatal("No non-KSK DNSKEY found; "
			      "supply a ZSK or use '-z'.");
	}
}

static void
set_nsec3params(isc_boolean_t update, isc_boolean_t set_salt,
		isc_boolean_t set_optout, isc_boolean_t set_iter)
{
	isc_result_t result;
	dns_dbversion_t *ver = NULL;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_nsec3_t nsec3;
	dns_fixedname_t fname;
	dns_name_t *hashname;
	unsigned char orig_salt[255];
	size_t orig_saltlen;
	dns_hash_t orig_hash;
	isc_uint16_t orig_iter;

	dns_db_currentversion(gdb, &ver);
	dns_rdataset_init(&rdataset);

	orig_saltlen = sizeof(orig_salt);
	result = dns_db_getnsec3parameters(gdb, ver, &orig_hash, NULL,
					   &orig_iter, orig_salt,
					   &orig_saltlen);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	nsec_datatype = dns_rdatatype_nsec3;

	if (!update && set_salt) {
		if (salt_length != orig_saltlen ||
		    !isc_safe_memequal(saltbuf, orig_salt, salt_length))
			fatal("An NSEC3 chain exists with a different salt. "
			      "Use -u to update it.");
	} else if (!set_salt) {
		salt_length = orig_saltlen;
		memmove(saltbuf, orig_salt, orig_saltlen);
		gsalt = saltbuf;
	}

	if (!update && set_iter) {
		if (nsec3iter != orig_iter)
			fatal("An NSEC3 chain exists with different "
			      "iterations. Use -u to update it.");
	} else if (!set_iter)
		nsec3iter = orig_iter;

	/*
	 * Find an NSEC3 record to get the current OPTOUT value.
	 * (This assumes all NSEC3 records agree.)
	 */

	dns_fixedname_init(&fname);
	hashname = dns_fixedname_name(&fname);
	result = dns_nsec3_hashname(&fname, NULL, NULL,
				    gorigin, gorigin, dns_hash_sha1,
				    orig_iter, orig_salt, orig_saltlen);
	check_result(result, "dns_nsec3_hashname");

	result = dns_db_findnsec3node(gdb, hashname, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = dns_db_findrdataset(gdb, node, ver, dns_rdatatype_nsec3,
				     0, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = dns_rdataset_first(&rdataset);
	check_result(result, "dns_rdataset_first");
	dns_rdataset_current(&rdataset, &rdata);
	result = dns_rdata_tostruct(&rdata, &nsec3, NULL);
	check_result(result, "dns_rdata_tostruct");

	if (!update && set_optout) {
		if (nsec3flags != nsec3.flags)
			fatal("An NSEC3 chain exists with%s OPTOUT. "
			      "Use -u -%s to %s it.",
			      OPTOUT(nsec3.flags) ? "" : "out",
			      OPTOUT(nsec3.flags) ? "AA" : "A",
			      OPTOUT(nsec3.flags) ? "clear" : "set");
	} else if (!set_optout)
		nsec3flags = nsec3.flags;

	dns_rdata_freestruct(&nsec3);

 cleanup:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(gdb, &node);
	dns_db_closeversion(gdb, &ver, ISC_FALSE);
}

static void
writeset(const char *prefix, dns_rdatatype_t type) {
	char *filename;
	char namestr[DNS_NAME_FORMATSIZE];
	dns_db_t *db = NULL;
	dns_dbversion_t *dbversion = NULL;
	dns_diff_t diff;
	dns_difftuple_t *tuple = NULL;
	dns_fixedname_t fixed;
	dns_name_t *name;
	dns_rdata_t rdata, ds;
	isc_boolean_t have_ksk = ISC_FALSE;
	isc_boolean_t have_non_ksk = ISC_FALSE;
	isc_buffer_t b;
	isc_buffer_t namebuf;
	isc_region_t r;
	isc_result_t result;
	dns_dnsseckey_t *key, *tmpkey;
	unsigned char dsbuf[DNS_DS_BUFFERSIZE];
	unsigned char keybuf[DST_KEY_MAXSIZE];
	unsigned int filenamelen;
	const dns_master_style_t *style =
		(type == dns_rdatatype_dnskey) ? masterstyle : dsstyle;

	isc_buffer_init(&namebuf, namestr, sizeof(namestr));
	result = dns_name_tofilenametext(gorigin, ISC_FALSE, &namebuf);
	check_result(result, "dns_name_tofilenametext");
	isc_buffer_putuint8(&namebuf, 0);
	filenamelen = strlen(prefix) + strlen(namestr);
	if (dsdir != NULL)
		filenamelen += strlen(dsdir) + 1;
	filename = isc_mem_get(mctx, filenamelen + 1);
	if (filename == NULL)
		fatal("out of memory");
	if (dsdir != NULL)
		sprintf(filename, "%s/", dsdir);
	else
		filename[0] = 0;
	strcat(filename, prefix);
	strcat(filename, namestr);

	dns_diff_init(mctx, &diff);

	if (type == dns_rdatatype_dlv) {
		dns_name_t tname;
		unsigned int labels;

		dns_name_init(&tname, NULL);
		dns_fixedname_init(&fixed);
		name = dns_fixedname_name(&fixed);
		labels = dns_name_countlabels(gorigin);
		dns_name_getlabelsequence(gorigin, 0, labels - 1, &tname);
		result = dns_name_concatenate(&tname, dlv, name, NULL);
		check_result(result, "dns_name_concatenate");
	} else
		name = gorigin;

	for (key = ISC_LIST_HEAD(keylist);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link))
	{
		if (REVOKE(key->key))
			continue;
		if (isksk(key)) {
			have_ksk = ISC_TRUE;
			have_non_ksk = ISC_FALSE;
		} else {
			have_ksk = ISC_FALSE;
			have_non_ksk = ISC_TRUE;
		}
		for (tmpkey = ISC_LIST_HEAD(keylist);
		     tmpkey != NULL;
		     tmpkey = ISC_LIST_NEXT(tmpkey, link)) {
			if (dst_key_alg(key->key) != dst_key_alg(tmpkey->key))
				continue;
			if (REVOKE(tmpkey->key))
				continue;
			if (isksk(tmpkey))
				have_ksk = ISC_TRUE;
			else
				have_non_ksk = ISC_TRUE;
		}
		if (have_ksk && have_non_ksk && !isksk(key))
			continue;
		dns_rdata_init(&rdata);
		dns_rdata_init(&ds);
		isc_buffer_init(&b, keybuf, sizeof(keybuf));
		result = dst_key_todns(key->key, &b);
		check_result(result, "dst_key_todns");
		isc_buffer_usedregion(&b, &r);
		dns_rdata_fromregion(&rdata, gclass, dns_rdatatype_dnskey, &r);
		if (type != dns_rdatatype_dnskey) {
			result = dns_ds_buildrdata(gorigin, &rdata,
						   DNS_DSDIGEST_SHA1,
						   dsbuf, &ds);
			check_result(result, "dns_ds_buildrdata");
			if (type == dns_rdatatype_dlv)
				ds.type = dns_rdatatype_dlv;
			result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD,
						      name, 0, &ds, &tuple);
			check_result(result, "dns_difftuple_create");
			dns_diff_append(&diff, &tuple);

			dns_rdata_reset(&ds);
			result = dns_ds_buildrdata(gorigin, &rdata,
						   DNS_DSDIGEST_SHA256,
						   dsbuf, &ds);
			check_result(result, "dns_ds_buildrdata");
			if (type == dns_rdatatype_dlv)
				ds.type = dns_rdatatype_dlv;
			result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD,
						      name, 0, &ds, &tuple);

		} else
			result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD,
						      gorigin, zone_soa_min_ttl,
						      &rdata, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(&diff, &tuple);
	}

	result = dns_db_create(mctx, "rbt", dns_rootname, dns_dbtype_zone,
			       gclass, 0, NULL, &db);
	check_result(result, "dns_db_create");

	result = dns_db_newversion(db, &dbversion);
	check_result(result, "dns_db_newversion");

	result = dns_diff_apply(&diff, db, dbversion);
	check_result(result, "dns_diff_apply");
	dns_diff_clear(&diff);

	result = dns_master_dump(mctx, db, dbversion, style, filename);
	check_result(result, "dns_master_dump");

	isc_mem_put(mctx, filename, filenamelen + 1);

	dns_db_closeversion(db, &dbversion, ISC_FALSE);
	dns_db_detach(&db);
}

static void
print_time(FILE *fp) {
	time_t currenttime;

	if (outputformat != dns_masterformat_text)
		return;

	currenttime = time(NULL);
	fprintf(fp, "; File written on %s", ctime(&currenttime));
}

static void
print_version(FILE *fp) {
	if (outputformat != dns_masterformat_text)
		return;

	fprintf(fp, "; dnssec_signzone version " VERSION "\n");
}

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s [options] zonefile [keys]\n", program);

	fprintf(stderr, "\n");

	fprintf(stderr, "Version: %s\n", VERSION);

	fprintf(stderr, "Options: (default value in parenthesis) \n");
	fprintf(stderr, "\t-S:\tsmart signing: automatically finds key files\n"
			"\t\tfor the zone and determines how they are to "
			"be used\n");
	fprintf(stderr, "\t-K directory:\n");
	fprintf(stderr, "\t\tdirectory to find key files (.)\n");
	fprintf(stderr, "\t-d directory:\n");
	fprintf(stderr, "\t\tdirectory to find dsset-* files (.)\n");
	fprintf(stderr, "\t-g:\t");
	fprintf(stderr, "update DS records based on child zones' "
			"dsset-* files\n");
	fprintf(stderr, "\t-s [YYYYMMDDHHMMSS|+offset]:\n");
	fprintf(stderr, "\t\tRRSIG start time "
				"- absolute|offset (now - 1 hour)\n");
	fprintf(stderr, "\t-e [YYYYMMDDHHMMSS|+offset|\"now\"+offset]:\n");
	fprintf(stderr, "\t\tRRSIG end time "
				"- absolute|from start|from now "
				"(now + 30 days)\n");
	fprintf(stderr, "\t-X [YYYYMMDDHHMMSS|+offset|\"now\"+offset]:\n");
	fprintf(stderr, "\t\tDNSKEY RRSIG end "
				"- absolute|from start|from now "
				"(matches -e)\n");
	fprintf(stderr, "\t-i interval:\n");
	fprintf(stderr, "\t\tcycle interval - resign "
				"if < interval from end ( (end-start)/4 )\n");
	fprintf(stderr, "\t-j jitter:\n");
	fprintf(stderr, "\t\trandomize signature end time up to jitter seconds\n");
	fprintf(stderr, "\t-v debuglevel (0)\n");
	fprintf(stderr, "\t-V:\tprint version information\n");
	fprintf(stderr, "\t-o origin:\n");
	fprintf(stderr, "\t\tzone origin (name of zonefile)\n");
	fprintf(stderr, "\t-f outfile:\n");
	fprintf(stderr, "\t\tfile the signed zone is written in "
				"(zonefile + .signed)\n");
	fprintf(stderr, "\t-I format:\n");
	fprintf(stderr, "\t\tfile format of input zonefile (text)\n");
	fprintf(stderr, "\t-O format:\n");
	fprintf(stderr, "\t\tfile format of signed zone file (text)\n");
	fprintf(stderr, "\t-N format:\n");
	fprintf(stderr, "\t\tsoa serial format of signed zone file (keep)\n");
	fprintf(stderr, "\t-D:\n");
	fprintf(stderr, "\t\toutput only DNSSEC-related records\n");
	fprintf(stderr, "\t-r randomdev:\n");
	fprintf(stderr,	"\t\ta file containing random data\n");
	fprintf(stderr, "\t-a:\t");
	fprintf(stderr, "verify generated signatures\n");
	fprintf(stderr, "\t-c class (IN)\n");
	fprintf(stderr, "\t-E engine:\n");
#ifdef USE_PKCS11
	fprintf(stderr, "\t\tname of an OpenSSL engine to use "
				"(default is \"pkcs11\")\n");
#else
	fprintf(stderr, "\t\tname of an OpenSSL engine to use\n");
#endif
	fprintf(stderr, "\t-p:\t");
	fprintf(stderr, "use pseudorandom data (faster but less secure)\n");
	fprintf(stderr, "\t-P:\t");
	fprintf(stderr, "disable post-sign verification\n");
	fprintf(stderr, "\t-Q:\t");
	fprintf(stderr, "remove signatures from keys that are no "
				"longer active\n");
	fprintf(stderr, "\t-R:\t");
	fprintf(stderr, "remove signatures from keys that no longer exist\n");
	fprintf(stderr, "\t-T TTL:\tTTL for newly added DNSKEYs\n");
	fprintf(stderr, "\t-t:\t");
	fprintf(stderr, "print statistics\n");
	fprintf(stderr, "\t-u:\t");
	fprintf(stderr, "update or replace an existing NSEC/NSEC3 chain\n");
	fprintf(stderr, "\t-x:\tsign DNSKEY record with KSKs only, not ZSKs\n");
	fprintf(stderr, "\t-z:\tsign all records with KSKs\n");
	fprintf(stderr, "\t-C:\tgenerate a keyset file, for compatibility\n"
			"\t\twith older versions of dnssec-signzone -g\n");
	fprintf(stderr, "\t-n ncpus (number of cpus present)\n");
	fprintf(stderr, "\t-k key_signing_key\n");
	fprintf(stderr, "\t-l lookasidezone\n");
	fprintf(stderr, "\t-3 NSEC3 salt\n");
	fprintf(stderr, "\t-H NSEC3 iterations (10)\n");
	fprintf(stderr, "\t-A NSEC3 optout\n");

	fprintf(stderr, "\n");

	fprintf(stderr, "Signing Keys: ");
	fprintf(stderr, "(default: all zone keys that have private keys)\n");
	fprintf(stderr, "\tkeyfile (Kname+alg+tag)\n");

	exit(0);
}

static void
removetempfile(void) {
	if (removefile)
		isc_file_remove(tempfile);
}

static void
print_stats(isc_time_t *timer_start, isc_time_t *timer_finish,
	    isc_time_t *sign_start, isc_time_t *sign_finish)
{
	isc_uint64_t time_us;      /* Time in microseconds */
	isc_uint64_t time_ms;      /* Time in milliseconds */
	isc_uint64_t sig_ms;	   /* Signatures per millisecond */
	FILE *out = output_stdout ? stderr : stdout;

	fprintf(out, "Signatures generated:               %10d\n", nsigned);
	fprintf(out, "Signatures retained:                %10d\n", nretained);
	fprintf(out, "Signatures dropped:                 %10d\n", ndropped);
	fprintf(out, "Signatures successfully verified:   %10d\n", nverified);
	fprintf(out, "Signatures unsuccessfully "
		     "verified: %10d\n", nverifyfailed);

	time_us = isc_time_microdiff(sign_finish, sign_start);
	time_ms = time_us / 1000;
	fprintf(out, "Signing time in seconds:           %7u.%03u\n",
		(unsigned int) (time_ms / 1000),
		(unsigned int) (time_ms % 1000));
	if (time_us > 0) {
		sig_ms = ((isc_uint64_t)nsigned * 1000000000) / time_us;
		fprintf(out, "Signatures per second:             %7u.%03u\n",
			(unsigned int) sig_ms / 1000,
			(unsigned int) sig_ms % 1000);
	}

	time_us = isc_time_microdiff(timer_finish, timer_start);
	time_ms = time_us / 1000;
	fprintf(out, "Runtime in seconds:                %7u.%03u\n",
		(unsigned int) (time_ms / 1000),
		(unsigned int) (time_ms % 1000));
}

int
main(int argc, char *argv[]) {
	int i, ch;
	char *startstr = NULL, *endstr = NULL, *classname = NULL;
	char *dnskey_endstr = NULL;
	char *origin = NULL, *file = NULL, *output = NULL;
	char *inputformatstr = NULL, *outputformatstr = NULL;
	char *serialformatstr = NULL;
	char *dskeyfile[MAXDSKEYS];
	int ndskeys = 0;
	char *endp;
	isc_time_t timer_start, timer_finish;
	isc_time_t sign_start, sign_finish;
	dns_dnsseckey_t *key;
	isc_result_t result;
	isc_log_t *log = NULL;
	isc_boolean_t pseudorandom = ISC_FALSE;
#ifdef USE_PKCS11
	const char *engine = "pkcs11";
#else
	const char *engine = NULL;
#endif
	unsigned int eflags;
	isc_boolean_t free_output = ISC_FALSE;
	int tempfilelen = 0;
	dns_rdataclass_t rdclass;
	isc_task_t **tasks = NULL;
	isc_buffer_t b;
	int len;
	hashlist_t hashlist;
	isc_boolean_t make_keyset = ISC_FALSE;
	isc_boolean_t set_salt = ISC_FALSE;
	isc_boolean_t set_optout = ISC_FALSE;
	isc_boolean_t set_iter = ISC_FALSE;
	isc_boolean_t nonsecify = ISC_FALSE;

	/* Unused letters: Bb G J M q Yy (and F is reserved). */
#define CMDLINE_FLAGS \
	"3:AaCc:Dd:E:e:f:FghH:i:I:j:K:k:L:l:m:n:N:o:O:PpQRr:s:ST:tuUv:VX:xzZ:"

	/*
	 * Process memory debugging argument first.
	 */
	while ((ch = isc_commandline_parse(argc, argv, CMDLINE_FLAGS)) != -1) {
		switch (ch) {
		case 'm':
			if (strcasecmp(isc_commandline_argument, "record") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGRECORD;
			if (strcasecmp(isc_commandline_argument, "trace") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGTRACE;
			if (strcasecmp(isc_commandline_argument, "usage") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGUSAGE;
			if (strcasecmp(isc_commandline_argument, "size") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGSIZE;
			if (strcasecmp(isc_commandline_argument, "mctx") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGCTX;
			break;
		default:
			break;
		}
	}
	isc_commandline_reset = ISC_TRUE;

	masterstyle = &dns_master_style_explicitttl;

	check_result(isc_app_start(), "isc_app_start");

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("out of memory");

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	while ((ch = isc_commandline_parse(argc, argv, CMDLINE_FLAGS)) != -1) {
		switch (ch) {
		case '3':
			set_salt = ISC_TRUE;
			nsec_datatype = dns_rdatatype_nsec3;
			if (strcmp(isc_commandline_argument, "-") != 0) {
				isc_buffer_t target;
				char *sarg;

				sarg = isc_commandline_argument;
				isc_buffer_init(&target, saltbuf,
						sizeof(saltbuf));
				result = isc_hex_decodestring(sarg, &target);
				check_result(result,
					     "isc_hex_decodestring(salt)");
				salt_length = isc_buffer_usedlength(&target);
			}
			break;

		case 'A':
			set_optout = ISC_TRUE;
			if (OPTOUT(nsec3flags))
				nsec3flags &= ~DNS_NSEC3FLAG_OPTOUT;
			else
				nsec3flags |= DNS_NSEC3FLAG_OPTOUT;
			break;

		case 'a':
			tryverify = ISC_TRUE;
			break;

		case 'C':
			make_keyset = ISC_TRUE;
			break;

		case 'c':
			classname = isc_commandline_argument;
			break;

		case 'd':
			dsdir = isc_commandline_argument;
			if (strlen(dsdir) == 0U)
				fatal("DS directory must be non-empty string");
			result = try_dir(dsdir);
			if (result != ISC_R_SUCCESS)
				fatal("cannot open directory %s: %s",
				      dsdir, isc_result_totext(result));
			break;

		case 'D':
			output_dnssec_only = ISC_TRUE;
			break;

		case 'E':
			engine = isc_commandline_argument;
			break;

		case 'e':
			endstr = isc_commandline_argument;
			break;

		case 'f':
			output = isc_commandline_argument;
			if (strcmp(output, "-") == 0)
				output_stdout = ISC_TRUE;
			break;

		case 'g':
			generateds = ISC_TRUE;
			break;

		case 'H':
			set_iter = ISC_TRUE;
			nsec3iter = strtoul(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("iterations must be numeric");
			if (nsec3iter  > 0xffffU)
				fatal("iterations too big");
			break;

		case 'I':
			inputformatstr = isc_commandline_argument;
			break;

		case 'i':
			endp = NULL;
			cycle = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0' || cycle < 0)
				fatal("cycle period must be numeric and "
				      "positive");
			break;

		case 'j':
			endp = NULL;
			jitter = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0' || jitter < 0)
				fatal("jitter must be numeric and positive");
			break;

		case 'K':
			directory = isc_commandline_argument;
			break;

		case 'k':
			if (ndskeys == MAXDSKEYS)
				fatal("too many key-signing keys specified");
			dskeyfile[ndskeys++] = isc_commandline_argument;
			break;

		case 'L':
			snset = ISC_TRUE;
			endp = NULL;
			serialnum = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0') {
				fprintf(stderr, "source serial number "
						"must be numeric");
				exit(1);
			}
			break;

		case 'l':
			len = strlen(isc_commandline_argument);
			isc_buffer_init(&b, isc_commandline_argument, len);
			isc_buffer_add(&b, len);

			dns_fixedname_init(&dlv_fixed);
			dlv = dns_fixedname_name(&dlv_fixed);
			result = dns_name_fromtext(dlv, &b, dns_rootname, 0,
						   NULL);
			check_result(result, "dns_name_fromtext(dlv)");
			break;

		case 'm':
			break;

		case 'N':
			serialformatstr = isc_commandline_argument;
			break;

		case 'n':
			endp = NULL;
			ntasks = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0' || ntasks > ISC_INT32_MAX)
				fatal("number of cpus must be numeric");
			break;

		case 'O':
			outputformatstr = isc_commandline_argument;
			break;

		case 'o':
			origin = isc_commandline_argument;
			break;

		case 'P':
			disable_zone_check = ISC_TRUE;
			break;

		case 'p':
			pseudorandom = ISC_TRUE;
			break;

		case 'Q':
			remove_inactkeysigs = ISC_TRUE;
			break;

		case 'R':
			remove_orphansigs = ISC_TRUE;
			break;

		case 'r':
			setup_entropy(mctx, isc_commandline_argument, &ectx);
			break;

		case 'S':
			smartsign = ISC_TRUE;
			break;

		case 's':
			startstr = isc_commandline_argument;
			break;

		case 'T':
			endp = NULL;
			set_keyttl = ISC_TRUE;
			keyttl = strtottl(isc_commandline_argument);
			break;

		case 't':
			printstats = ISC_TRUE;
			break;

		case 'U':	/* Undocumented for testing only. */
			unknownalg = ISC_TRUE;
			break;

		case 'u':
			update_chain = ISC_TRUE;
			break;

		case 'v':
			endp = NULL;
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("verbose level must be numeric");
			break;

		case 'X':
			dnskey_endstr = isc_commandline_argument;
			break;

		case 'x':
			keyset_kskonly = ISC_TRUE;
			break;

		case 'z':
			ignore_kskflag = ISC_TRUE;
			break;

		case 'F':
			/* Reserved for FIPS mode */
			/* FALLTHROUGH */
		case '?':
			if (isc_commandline_option != '?')
				fprintf(stderr, "%s: invalid argument -%c\n",
					program, isc_commandline_option);
			/* FALLTHROUGH */
		case 'h':
			/* Does not return. */
			usage();

		case 'V':
			/* Does not return. */
			version(program);

		case 'Z':	/* Undocumented test options */
			if (!strcmp(isc_commandline_argument, "nonsecify"))
				nonsecify = ISC_TRUE;
			break;

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);
	eflags = ISC_ENTROPY_BLOCKING;
	if (!pseudorandom)
		eflags |= ISC_ENTROPY_GOODONLY;

	result = isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE);
	if (result != ISC_R_SUCCESS)
		fatal("could not create hash context");

	result = dst_lib_init2(mctx, ectx, engine, eflags);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize dst: %s",
		      isc_result_totext(result));
	isc_stdtime_get(&now);

	if (startstr != NULL) {
		starttime = strtotime(startstr, now, now, NULL);
	} else
		starttime = now - 3600;  /* Allow for some clock skew. */

	if (endstr != NULL)
		endtime = strtotime(endstr, now, starttime, NULL);
	else
		endtime = starttime + (30 * 24 * 60 * 60);

	if (dnskey_endstr != NULL) {
		dnskey_endtime = strtotime(dnskey_endstr, now, starttime,
					   NULL);
		if (endstr != NULL && dnskey_endtime == endtime)
			fprintf(stderr, "WARNING: -e and -X were both set, "
					"but have identical values.\n");
	} else
		dnskey_endtime = endtime;

	if (cycle == -1)
		cycle = (endtime - starttime) / 4;

	if (ntasks == 0)
		ntasks = isc_os_ncpus() * 2;
	vbprintf(4, "using %d cpus\n", ntasks);

	rdclass = strtoclass(classname);

	if (directory == NULL)
		directory = ".";

	setup_logging(mctx, &log);

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	if (argc < 1)
		usage();

	file = argv[0];

	argc -= 1;
	argv += 1;

	if (origin == NULL)
		origin = file;

	if (output == NULL) {
		free_output = ISC_TRUE;
		output = isc_mem_allocate(mctx,
					  strlen(file) + strlen(".signed") + 1);
		if (output == NULL)
			fatal("out of memory");
		sprintf(output, "%s.signed", file);
	}

	if (inputformatstr != NULL) {
		if (strcasecmp(inputformatstr, "text") == 0)
			inputformat = dns_masterformat_text;
		else if (strcasecmp(inputformatstr, "raw") == 0)
			inputformat = dns_masterformat_raw;
		else if (strncasecmp(inputformatstr, "raw=", 4) == 0) {
			inputformat = dns_masterformat_raw;
			fprintf(stderr,
				"WARNING: input format version ignored\n");
		} else
			fatal("unknown file format: %s", inputformatstr);

	}

	if (outputformatstr != NULL) {
		if (strcasecmp(outputformatstr, "text") == 0) {
			outputformat = dns_masterformat_text;
		} else if (strcasecmp(outputformatstr, "full") == 0) {
			outputformat = dns_masterformat_text;
			masterstyle = &dns_master_style_full;
		} else if (strcasecmp(outputformatstr, "raw") == 0) {
			outputformat = dns_masterformat_raw;
		} else if (strncasecmp(outputformatstr, "raw=", 4) == 0) {
			char *end;
			outputformat = dns_masterformat_raw;

			outputformat = dns_masterformat_raw;
			rawversion = strtol(outputformatstr + 4, &end, 10);
			if (end == outputformatstr + 4 || *end != '\0' ||
			    rawversion > 1U) {
				fprintf(stderr,
					"unknown raw format version\n");
				exit(1);
			}
		} else
			fatal("unknown file format: %s\n", outputformatstr);
	}

	if (serialformatstr != NULL) {
		if (strcasecmp(serialformatstr, "keep") == 0)
			serialformat = SOA_SERIAL_KEEP;
		else if (strcasecmp(serialformatstr, "increment") == 0 ||
			 strcasecmp(serialformatstr, "incr") == 0)
			serialformat = SOA_SERIAL_INCREMENT;
		else if (strcasecmp(serialformatstr, "unixtime") == 0)
			serialformat = SOA_SERIAL_UNIXTIME;
		else
			fatal("unknown soa serial format: %s\n",
			      serialformatstr);
	}

	if (output_dnssec_only && outputformat != dns_masterformat_text)
		fatal("option -D can only be used with \"-O text\"\n");

	if (output_dnssec_only && serialformat != SOA_SERIAL_KEEP)
		fatal("option -D can only be used with \"-N keep\"\n");

	result = dns_master_stylecreate(&dsstyle,  DNS_STYLEFLAG_NO_TTL,
					0, 24, 0, 0, 0, 8, mctx);
	check_result(result, "dns_master_stylecreate");

	gdb = NULL;
	TIME_NOW(&timer_start);
	loadzone(file, origin, rdclass, &gdb);
	gorigin = dns_db_origin(gdb);
	gclass = dns_db_class(gdb);
	get_soa_ttls();

	if (!set_keyttl)
		keyttl = soa_ttl;

	/*
	 * Check for any existing NSEC3 parameters in the zone,
	 * and use them as defaults if -u was not specified.
	 */
	if (update_chain && !set_optout && !set_iter && !set_salt)
		nsec_datatype = dns_rdatatype_nsec;
	else
		set_nsec3params(update_chain, set_salt, set_optout, set_iter);

	/*
	 * We need to do this early on, as we start messing with the list
	 * of keys rather early.
	 */
	ISC_LIST_INIT(keylist);
	result = isc_rwlock_init(&keylist_lock, 0, 0);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize keylist_lock: %s",
		      isc_result_totext(result));

	/*
	 * Fill keylist with:
	 * 1) Keys listed in the DNSKEY set that have
	 *    private keys associated, *if* no keys were
	 *    set on the command line.
	 * 2) ZSKs set on the command line
	 * 3) KSKs set on the command line
	 * 4) Any keys remaining in the DNSKEY set which
	 *    do not have private keys associated and were
	 *    not specified on the command line.
	 */
	if (argc == 0 || smartsign)
		loadzonekeys(!smartsign, ISC_FALSE);
	loadexplicitkeys(argv, argc, ISC_FALSE);
	loadexplicitkeys(dskeyfile, ndskeys, ISC_TRUE);
	loadzonekeys(!smartsign, ISC_TRUE);

	/*
	 * If we're doing smart signing, look in the key repository for
	 * key files with metadata, and merge them with the keylist
	 * we have now.
	 */
	if (smartsign)
		build_final_keylist();

	/* Now enumerate the key list */
	for (key = ISC_LIST_HEAD(keylist);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link)) {
		key->index = keycount++;
	}

	if (keycount == 0) {
		if (disable_zone_check)
			fprintf(stderr, "%s: warning: No keys specified "
					"or found\n", program);
		else
			fatal("No signing keys specified or found.");
		nokeys = ISC_TRUE;
	}

	warnifallksk(gdb);

	if (IS_NSEC3) {
		unsigned int max;
		isc_boolean_t answer;

		hash_length = dns_nsec3_hashlength(dns_hash_sha1);
		hashlist_init(&hashlist, dns_db_nodecount(gdb) * 2,
			      hash_length);
		result = dns_nsec_nseconly(gdb, gversion, &answer);
		if (result == ISC_R_NOTFOUND)
			fprintf(stderr, "%s: warning: NSEC3 generation "
				"requested with no DNSKEY; ignoring\n",
				program);
		else if (result != ISC_R_SUCCESS)
			check_result(result, "dns_nsec_nseconly");
		else if (answer)
			fatal("NSEC3 generation requested with "
			      "NSEC-only DNSKEY");

		result = dns_nsec3_maxiterations(gdb, NULL, mctx, &max);
		check_result(result, "dns_nsec3_maxiterations()");
		if (nsec3iter > max)
			fatal("NSEC3 iterations too big for weakest DNSKEY "
			      "strength. Maximum iterations allowed %u.", max);
	}

	gversion = NULL;
	result = dns_db_newversion(gdb, &gversion);
	check_result(result, "dns_db_newversion()");

	switch (serialformat) {
		case SOA_SERIAL_INCREMENT:
			setsoaserial(0);
			break;
		case SOA_SERIAL_UNIXTIME:
			setsoaserial(now);
			break;
		case SOA_SERIAL_KEEP:
		default:
			/* do nothing */
			break;
	}

	remove_duplicates();

	if (!nonsecify) {
		if (IS_NSEC3)
			nsec3ify(dns_hash_sha1, nsec3iter, gsalt, salt_length,
				 &hashlist);
		else
			nsecify();
	}

	if (!nokeys) {
		writeset("dsset-", dns_rdatatype_ds);
		if (make_keyset)
			writeset("keyset-", dns_rdatatype_dnskey);
		if (dlv != NULL) {
			writeset("dlvset-", dns_rdatatype_dlv);
		}
	}

	if (output_stdout) {
		outfp = stdout;
		if (outputformatstr == NULL)
			masterstyle = &dns_master_style_full;
	} else {
		tempfilelen = strlen(output) + 20;
		tempfile = isc_mem_get(mctx, tempfilelen);
		if (tempfile == NULL)
			fatal("out of memory");

		result = isc_file_mktemplate(output, tempfile, tempfilelen);
		check_result(result, "isc_file_mktemplate");

		if (outputformat == dns_masterformat_text)
			result = isc_file_openunique(tempfile, &outfp);
		else
			result = isc_file_bopenunique(tempfile, &outfp);
		if (result != ISC_R_SUCCESS)
			fatal("failed to open temporary output file: %s",
			      isc_result_totext(result));
		removefile = ISC_TRUE;
		setfatalcallback(&removetempfile);
	}

	print_time(outfp);
	print_version(outfp);

	result = isc_taskmgr_create(mctx, ntasks, 0, &taskmgr);
	if (result != ISC_R_SUCCESS)
		fatal("failed to create task manager: %s",
		      isc_result_totext(result));

	master = NULL;
	result = isc_task_create(taskmgr, 0, &master);
	if (result != ISC_R_SUCCESS)
		fatal("failed to create task: %s", isc_result_totext(result));

	tasks = isc_mem_get(mctx, ntasks * sizeof(isc_task_t *));
	if (tasks == NULL)
		fatal("out of memory");
	for (i = 0; i < (int)ntasks; i++) {
		tasks[i] = NULL;
		result = isc_task_create(taskmgr, 0, &tasks[i]);
		if (result != ISC_R_SUCCESS)
			fatal("failed to create task: %s",
			      isc_result_totext(result));
	}

	RUNTIME_CHECK(isc_mutex_init(&namelock) == ISC_R_SUCCESS);
	if (printstats)
		RUNTIME_CHECK(isc_mutex_init(&statslock) == ISC_R_SUCCESS);

	presign();
	TIME_NOW(&sign_start);
	signapex();
	if (!finished) {
		/*
		 * There is more work to do.  Spread it out over multiple
		 * processors if possible.
		 */
		for (i = 0; i < (int)ntasks; i++) {
			result = isc_app_onrun(mctx, master, startworker,
					       tasks[i]);
			if (result != ISC_R_SUCCESS)
				fatal("failed to start task: %s",
				      isc_result_totext(result));
		}
		(void)isc_app_run();
		if (!finished)
			fatal("process aborted by user");
	} else
		isc_task_detach(&master);
	shuttingdown = ISC_TRUE;
	for (i = 0; i < (int)ntasks; i++)
		isc_task_detach(&tasks[i]);
	isc_taskmgr_destroy(&taskmgr);
	isc_mem_put(mctx, tasks, ntasks * sizeof(isc_task_t *));
	postsign();
	TIME_NOW(&sign_finish);

	if (!disable_zone_check)
		verifyzone(gdb, gversion, gorigin, mctx,
			   ignore_kskflag, keyset_kskonly);

	if (outputformat != dns_masterformat_text) {
		dns_masterrawheader_t header;
		dns_master_initrawheader(&header);
		if (rawversion == 0U)
			header.flags = DNS_MASTERRAW_COMPAT;
		else if (snset) {
			header.flags = DNS_MASTERRAW_SOURCESERIALSET;
			header.sourceserial = serialnum;
		}
		result = dns_master_dumptostream3(mctx, gdb, gversion,
						  masterstyle, outputformat,
						  &header, outfp);
		check_result(result, "dns_master_dumptostream3");
	}

	DESTROYLOCK(&namelock);
	if (printstats)
		DESTROYLOCK(&statslock);

	if (!output_stdout) {
		result = isc_stdio_close(outfp);
		check_result(result, "isc_stdio_close");
		removefile = ISC_FALSE;

		result = isc_file_rename(tempfile, output);
		if (result != ISC_R_SUCCESS)
			fatal("failed to rename temp file to %s: %s\n",
			      output, isc_result_totext(result));

		printf("%s\n", output);
	}

	dns_db_closeversion(gdb, &gversion, ISC_FALSE);
	dns_db_detach(&gdb);

	while (!ISC_LIST_EMPTY(keylist)) {
		key = ISC_LIST_HEAD(keylist);
		ISC_LIST_UNLINK(keylist, key, link);
		dns_dnsseckey_destroy(mctx, &key);
	}

	if (tempfilelen != 0)
		isc_mem_put(mctx, tempfile, tempfilelen);

	if (free_output)
		isc_mem_free(mctx, output);

	dns_master_styledestroy(&dsstyle, mctx);

	cleanup_logging(&log);
	dst_lib_destroy();
	isc_hash_destroy();
	cleanup_entropy(&ectx);
	dns_name_destroy();
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	(void) isc_app_finish();

	if (printstats) {
		TIME_NOW(&timer_finish);
		print_stats(&timer_start, &timer_finish,
			    &sign_start, &sign_finish);
	}

	return (0);
}
