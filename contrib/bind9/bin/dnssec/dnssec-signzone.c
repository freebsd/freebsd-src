/*
 * Portions Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 1999-2003  Internet Software Consortium.
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: dnssec-signzone.c,v 1.139.2.2.4.17 2004/10/25 01:36:06 marka Exp $ */

#include <config.h>

#include <stdlib.h>
#include <time.h>

#include <isc/app.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/event.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/os.h>
#include <isc/print.h>
#include <isc/serial.h>
#include <isc/stdio.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/util.h>
#include <isc/time.h>

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
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdataclass.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/time.h>

#include <dst/dst.h>

#include "dnssectool.h"

const char *program = "dnssec-signzone";
int verbose;

#define BUFSIZE 2048
#define MAXDSKEYS 8

typedef struct signer_key_struct signer_key_t;

struct signer_key_struct {
	dst_key_t *key;
	isc_boolean_t issigningkey;
	isc_boolean_t isdsk;
	isc_boolean_t isksk;
	unsigned int position;
	ISC_LINK(signer_key_t) link;
};

#define SIGNER_EVENTCLASS	ISC_EVENTCLASS(0x4453)
#define SIGNER_EVENT_WRITE	(SIGNER_EVENTCLASS + 0)
#define SIGNER_EVENT_WORK	(SIGNER_EVENTCLASS + 1)

typedef struct signer_event sevent_t;
struct signer_event {
	ISC_EVENT_COMMON(sevent_t);
	dns_fixedname_t *fname;
	dns_dbnode_t *node;
};

static ISC_LIST(signer_key_t) keylist;
static unsigned int keycount = 0;
static isc_stdtime_t starttime = 0, endtime = 0, now;
static int cycle = -1;
static isc_boolean_t tryverify = ISC_FALSE;
static isc_boolean_t printstats = ISC_FALSE;
static isc_mem_t *mctx = NULL;
static isc_entropy_t *ectx = NULL;
static dns_ttl_t zonettl;
static FILE *fp;
static char *tempfile = NULL;
static const dns_master_style_t *masterstyle;
static unsigned int nsigned = 0, nretained = 0, ndropped = 0;
static unsigned int nverified = 0, nverifyfailed = 0;
static const char *directory;
static isc_mutex_t namelock, statslock;
static isc_taskmgr_t *taskmgr = NULL;
static dns_db_t *gdb;			/* The database */
static dns_dbversion_t *gversion;	/* The database version */
static dns_dbiterator_t *gdbiter;	/* The database iterator */
static dns_rdataclass_t gclass;		/* The class */
static dns_name_t *gorigin;		/* The database origin */
static isc_task_t *master = NULL;
static unsigned int ntasks = 0;
static isc_boolean_t shuttingdown = ISC_FALSE, finished = ISC_FALSE;
static unsigned int assigned = 0, completed = 0;
static isc_boolean_t nokeys = ISC_FALSE;
static isc_boolean_t removefile = ISC_FALSE;
static isc_boolean_t generateds = ISC_FALSE;
static isc_boolean_t ignoreksk = ISC_FALSE;
static dns_name_t *dlv = NULL;
static dns_fixedname_t dlv_fixed;
static dns_master_style_t *dsstyle = NULL;

#define INCSTAT(counter)		\
	if (printstats) {		\
		LOCK(&statslock);	\
		counter++;		\
		UNLOCK(&statslock);	\
	}

static void
sign(isc_task_t *task, isc_event_t *event);


static inline void
set_bit(unsigned char *array, unsigned int index, unsigned int bit) {
	unsigned int shift, mask;

	shift = 7 - (index % 8);
	mask = 1 << shift;

	if (bit != 0)
		array[index / 8] |= mask;
	else
		array[index / 8] &= (~mask & 0xFF);
}

static void
dumpnode(dns_name_t *name, dns_dbnode_t *node) {
	isc_result_t result;

	result = dns_master_dumpnodetostream(mctx, gdb, gversion, node, name,
					     masterstyle, fp);
	check_result(result, "dns_master_dumpnodetostream");
}

static void
dumpdb(dns_db_t *db) {
	dns_dbiterator_t *dbiter = NULL;
	dns_dbnode_t *node;
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_result_t result;

	dbiter = NULL;
	result = dns_db_createiterator(db, ISC_FALSE, &dbiter);
	check_result(result, "dns_db_createiterator()");

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	node = NULL;

	for (result = dns_dbiterator_first(dbiter);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(dbiter))
	{
		result = dns_dbiterator_current(dbiter, &node, name);
		check_result(result, "dns_dbiterator_current()");
		dumpnode(name, node);
		dns_db_detachnode(db, &node);
	}
	if (result != ISC_R_NOMORE)
		fatal("iterating database: %s", isc_result_totext(result));

	dns_dbiterator_destroy(&dbiter);
}

static signer_key_t *
newkeystruct(dst_key_t *dstkey, isc_boolean_t signwithkey) {
	signer_key_t *key;

	key = isc_mem_get(mctx, sizeof(signer_key_t));
	if (key == NULL)
		fatal("out of memory");
	key->key = dstkey;
	if ((dst_key_flags(dstkey) & DNS_KEYFLAG_KSK) != 0) {
		key->issigningkey = signwithkey;
		key->isksk = ISC_TRUE;
		key->isdsk = ISC_FALSE;
	} else {
		key->issigningkey = signwithkey;
		key->isksk = ISC_FALSE;
		key->isdsk = ISC_TRUE;
	}
	key->position = keycount++;
	ISC_LINK_INIT(key, link);
	return (key);
}

static void
signwithkey(dns_name_t *name, dns_rdataset_t *rdataset, dns_rdata_t *rdata,
	    dst_key_t *key, isc_buffer_t *b)
{
	isc_result_t result;

	result = dns_dnssec_sign(name, rdataset, key, &starttime, &endtime,
				 mctx, b, rdata);
	isc_entropy_stopcallbacksources(ectx);
	if (result != ISC_R_SUCCESS) {
		char keystr[KEY_FORMATSIZE];
		key_format(key, keystr, sizeof(keystr));
		fatal("dnskey '%s' failed to sign data: %s",
		      keystr, isc_result_totext(result));
	}
	INCSTAT(nsigned);

	if (tryverify) {
		result = dns_dnssec_verify(name, rdataset, key,
					   ISC_TRUE, mctx, rdata);
		if (result == ISC_R_SUCCESS) {
			vbprintf(3, "\tsignature verified\n");
			INCSTAT(nverified);
		} else {
			vbprintf(3, "\tsignature failed to verify\n");
			INCSTAT(nverifyfailed);
		}
	}
}

static inline isc_boolean_t
issigningkey(signer_key_t *key) {
	return (key->issigningkey);
}

static inline isc_boolean_t
iszonekey(signer_key_t *key) {
	return (ISC_TF(dns_name_equal(dst_key_name(key->key), gorigin) &&
		       dst_key_iszonekey(key->key)));
}

/*
 * Finds the key that generated a RRSIG, if possible.  First look at the keys
 * that we've loaded already, and then see if there's a key on disk.
 */
static signer_key_t *
keythatsigned(dns_rdata_rrsig_t *rrsig) {
	isc_result_t result;
	dst_key_t *pubkey = NULL, *privkey = NULL;
	signer_key_t *key;

	key = ISC_LIST_HEAD(keylist);
	while (key != NULL) {
		if (rrsig->keyid == dst_key_id(key->key) &&
		    rrsig->algorithm == dst_key_alg(key->key) &&
		    dns_name_equal(&rrsig->signer, dst_key_name(key->key)))
			return key;
		key = ISC_LIST_NEXT(key, link);
	}

	result = dst_key_fromfile(&rrsig->signer, rrsig->keyid,
				  rrsig->algorithm, DST_TYPE_PUBLIC,
				  NULL, mctx, &pubkey);
	if (result != ISC_R_SUCCESS)
		return (NULL);

	result = dst_key_fromfile(&rrsig->signer, rrsig->keyid,
				  rrsig->algorithm,
				  DST_TYPE_PUBLIC | DST_TYPE_PRIVATE,
				  NULL, mctx, &privkey);
	if (result == ISC_R_SUCCESS) {
		dst_key_free(&pubkey);
		key = newkeystruct(privkey, ISC_FALSE);
	} else
		key = newkeystruct(pubkey, ISC_FALSE);
	ISC_LIST_APPEND(keylist, key, link);
	return (key);
}

/*
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
	return (ISC_FALSE); /* removes a warning */
}

static inline isc_boolean_t
setverifies(dns_name_t *name, dns_rdataset_t *set, signer_key_t *key,
	    dns_rdata_t *rrsig)
{
	isc_result_t result;
	result = dns_dnssec_verify(name, set, key->key, ISC_FALSE, mctx, rrsig);
	if (result == ISC_R_SUCCESS) {
		INCSTAT(nverified);
		return (ISC_TRUE);
	} else {
		INCSTAT(nverifyfailed);
		return (ISC_FALSE);
	}
}

/*
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
	signer_key_t *key;
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
			 expecttofindkey(&rrsig.signer))
		{
			/* rrsig is dropped and not replaced */
			vbprintf(2, "\trrsig by %s dropped - "
				 "private dnskey not found\n",
				 sigstr);
		} else if (key == NULL || future) {
			vbprintf(2, "\trrsig by %s %s - dnskey not found\n",
				 expired ? "retained" : "dropped", sigstr);
			if (!expired)
				keep = ISC_TRUE;
		} else if (issigningkey(key)) {
			if (!expired && setverifies(name, set, key, &sigrdata))
			{
				vbprintf(2, "\trrsig by %s retained\n", sigstr);
				keep = ISC_TRUE;
				wassignedby[key->position] = ISC_TRUE;
				nowsignedby[key->position] = ISC_TRUE;
			} else {
				vbprintf(2, "\trrsig by %s dropped - %s\n",
					 sigstr,
					 expired ? "expired" :
						   "failed to verify");
				wassignedby[key->position] = ISC_TRUE;
				resign = ISC_TRUE;
			}
		} else if (iszonekey(key)) {
			if (!expired && setverifies(name, set, key, &sigrdata))
			{
				vbprintf(2, "\trrsig by %s retained\n", sigstr);
				keep = ISC_TRUE;
				wassignedby[key->position] = ISC_TRUE;
				nowsignedby[key->position] = ISC_TRUE;
			} else {
				vbprintf(2, "\trrsig by %s dropped - %s\n",
					 sigstr,
					 expired ? "expired" :
						   "failed to verify");
				wassignedby[key->position] = ISC_TRUE;
			}
		} else if (!expired) {
			vbprintf(2, "\trrsig by %s retained\n", sigstr);
			keep = ISC_TRUE;
		} else {
			vbprintf(2, "\trrsig by %s expired\n", sigstr);
		}

		if (keep) {
			nowsignedby[key->position] = ISC_TRUE;
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
			result = dns_difftuple_create(mctx, DNS_DIFFOP_DEL,
						      name, sigset.ttl,
						      &sigrdata, &tuple);
			check_result(result, "dns_difftuple_create");
			dns_diff_append(del, &tuple);
			INCSTAT(ndropped);
		}

		if (resign) {
			isc_buffer_t b;
			dns_rdata_t trdata = DNS_RDATA_INIT;
			unsigned char array[BUFSIZE];
			char keystr[KEY_FORMATSIZE];

			INSIST(!keep);

			key_format(key->key, keystr, sizeof(keystr));
			vbprintf(1, "\tresigning with dnskey %s\n", keystr);
			isc_buffer_init(&b, array, sizeof(array));
			signwithkey(name, set, &trdata, key->key, &b);
			nowsignedby[key->position] = ISC_TRUE;
			tuple = NULL;
			result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD,
						      name, ttl, &trdata,
						      &tuple);
			check_result(result, "dns_difftuple_create");
			dns_diff_append(add, &tuple);
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
		isc_buffer_t b;
		dns_rdata_t trdata;
		unsigned char array[BUFSIZE];
		char keystr[KEY_FORMATSIZE];

		if (nowsignedby[key->position])
			continue;

		if (!key->issigningkey)
			continue;
		if (!(ignoreksk || key->isdsk ||
		      (key->isksk &&
		       set->type == dns_rdatatype_dnskey &&
		       dns_name_equal(name, gorigin))))
			continue;

		key_format(key->key, keystr, sizeof(keystr));
		vbprintf(1, "\tsigning with dnskey %s\n", keystr);
		dns_rdata_init(&trdata);
		isc_buffer_init(&b, array, sizeof(array));
		signwithkey(name, set, &trdata, key->key, &b);
		tuple = NULL;
		result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD, name,
					      ttl, &trdata, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(add, &tuple);
	}

	isc_mem_put(mctx, wassignedby, arraysize * sizeof(isc_boolean_t));
	isc_mem_put(mctx, nowsignedby, arraysize * sizeof(isc_boolean_t));
}

static void
opendb(const char *prefix, dns_name_t *name, dns_rdataclass_t rdclass,
       dns_db_t **dbp)
{
	char filename[256];
	isc_buffer_t b;
	isc_result_t result;

	isc_buffer_init(&b, filename, sizeof(filename));
	if (directory != NULL) {
		isc_buffer_putstr(&b, directory);
		if (directory[strlen(directory) - 1] != '/')
			isc_buffer_putstr(&b, "/");
	}
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

	result = dns_db_load(*dbp, filename);
	if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE)
		dns_db_detach(dbp);
}

/*
 * Loads the key set for a child zone, if there is one, and builds DS records.
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

	opendb("keyset-", name, gclass, &db);
	if (db == NULL)
		return (ISC_R_NOTFOUND);

	result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS) {
		dns_db_detach(&db);
		return (DNS_R_BADDB);
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
nsec_setbit(dns_name_t *name, dns_rdataset_t *rdataset, dns_rdatatype_t type,
	   unsigned int val)
{
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_nsec_t nsec;
	unsigned int newlen;
	unsigned char bitmap[8192 + 512];
	unsigned char nsecdata[8192 + 512 + DNS_NAME_MAXWIRE];
	isc_boolean_t answer = ISC_FALSE;
	unsigned int i, len, window;
	int octet;

	result = dns_rdataset_first(rdataset);
	check_result(result, "dns_rdataset_first()");
	dns_rdataset_current(rdataset, &rdata);
	result = dns_rdata_tostruct(&rdata, &nsec, NULL);
	check_result(result, "dns_rdata_tostruct");

	INSIST(nsec.len <= sizeof(bitmap));

	newlen = 0;

	memset(bitmap, 0, sizeof(bitmap));
	for (i = 0; i < nsec.len; i += len) {
		INSIST(i + 2 <= nsec.len);
		window = nsec.typebits[i];
		len = nsec.typebits[i+1];
		i += 2;
		INSIST(len > 0 && len <= 32);
		INSIST(i + len <= nsec.len);
		memmove(&bitmap[window * 32 + 512], &nsec.typebits[i], len);
	}
	set_bit(bitmap + 512, type, val);
	for (window = 0; window < 256; window++) {
		for (octet = 31; octet >= 0; octet--)
			if (bitmap[window * 32 + 512 + octet] != 0)
				break;
		if (octet < 0)
			continue;
		bitmap[newlen] = window;
		bitmap[newlen + 1] = octet + 1;
		newlen += 2;
		/*
		 * Overlapping move.
		 */
		memmove(&bitmap[newlen], &bitmap[window * 32 + 512], octet + 1);
		newlen += octet + 1;
	}
	if (newlen != nsec.len ||
	    memcmp(nsec.typebits, bitmap, newlen) != 0) {
		dns_rdata_t newrdata = DNS_RDATA_INIT;
		isc_buffer_t b;
		dns_diff_t diff;
		dns_difftuple_t *tuple = NULL;

		dns_diff_init(mctx, &diff);
		result = dns_difftuple_create(mctx, DNS_DIFFOP_DEL, name,
					      rdataset->ttl, &rdata, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(&diff, &tuple);

		nsec.typebits = bitmap;
		nsec.len = newlen;
		isc_buffer_init(&b, nsecdata, sizeof(nsecdata));
		result = dns_rdata_fromstruct(&newrdata, rdata.rdclass,
					      dns_rdatatype_nsec, &nsec,
					      &b);
		check_result(result, "dns_rdata_fromstruct");

		result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD,
					      name, rdataset->ttl,
					      &newrdata, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(&diff, &tuple);
		result = dns_diff_apply(&diff, gdb, gversion);
		check_result(result, "dns_difftuple_apply");
		dns_diff_clear(&diff);
		answer = ISC_TRUE;
	}
	dns_rdata_freestruct(&nsec);
	return (answer);
}

static isc_boolean_t
delegation(dns_name_t *name, dns_dbnode_t *node, isc_uint32_t *ttlp) {
	dns_rdataset_t nsset;
	isc_result_t result;

	if (dns_name_equal(name, gorigin))
		return (ISC_FALSE);

	dns_rdataset_init(&nsset);
	result = dns_db_findrdataset(gdb, node, gversion, dns_rdatatype_ns,
				     0, 0, &nsset, NULL);
	if (dns_rdataset_isassociated(&nsset)) {
		if (ttlp != NULL)
			*ttlp = nsset.ttl;
		dns_rdataset_disassociate(&nsset);
	}

	return (ISC_TF(result == ISC_R_SUCCESS));
}

/*
 * Signs all records at a name.  This mostly just signs each set individually,
 * but also adds the RRSIG bit to any NSECs generated earlier, deals with
 * parent/child KEY signatures, and handles other exceptional cases.
 */
static void
signname(dns_dbnode_t *node, dns_name_t *name) {
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_rdatasetiter_t *rdsiter;
	isc_boolean_t isdelegation = ISC_FALSE;
	isc_boolean_t hasds = ISC_FALSE;
	isc_boolean_t atorigin;
	isc_boolean_t changed = ISC_FALSE;
	dns_diff_t del, add;
	char namestr[DNS_NAME_FORMATSIZE];
	isc_uint32_t nsttl = 0;

	dns_name_format(name, namestr, sizeof(namestr));

	atorigin = dns_name_equal(name, gorigin);

	/*
	 * Determine if this is a delegation point.
	 */
	if (delegation(name, node, &nsttl))
		isdelegation = ISC_TRUE;

	/*
	 * If this is a delegation point, look for a DS set.
	 */
	if (isdelegation) {
		dns_rdataset_t dsset;
		dns_rdataset_t sigdsset;

		dns_rdataset_init(&dsset);
		dns_rdataset_init(&sigdsset);
		result = dns_db_findrdataset(gdb, node, gversion,
					     dns_rdatatype_ds,
					     0, 0, &dsset, &sigdsset);
		if (result == ISC_R_SUCCESS) {
			dns_rdataset_disassociate(&dsset);
			if (generateds) {
				result = dns_db_deleterdataset(gdb, node,
							       gversion,
							       dns_rdatatype_ds,
							       0);
				check_result(result, "dns_db_deleterdataset");
			} else
				hasds = ISC_TRUE;
		}
		if (generateds) {
			result = loadds(name, nsttl, &dsset);
			if (result == ISC_R_SUCCESS) {
				result = dns_db_addrdataset(gdb, node,
							    gversion, 0,
							    &dsset, 0, NULL);
				check_result(result, "dns_db_addrdataset");
				hasds = ISC_TRUE;
				dns_rdataset_disassociate(&dsset);
				if (dns_rdataset_isassociated(&sigdsset))
					dns_rdataset_disassociate(&sigdsset);
			} else if (dns_rdataset_isassociated(&sigdsset)) {
				result = dns_db_deleterdataset(gdb, node,
							    gversion,
							    dns_rdatatype_rrsig,
							    dns_rdatatype_ds);
				check_result(result, "dns_db_deleterdataset");
				dns_rdataset_disassociate(&sigdsset);
			}
		} else if (dns_rdataset_isassociated(&sigdsset))
			dns_rdataset_disassociate(&sigdsset);
	}

	/*
	 * Make sure that NSEC bits are appropriately set.
	 */
	dns_rdataset_init(&rdataset);
	RUNTIME_CHECK(dns_db_findrdataset(gdb, node, gversion,
					  dns_rdatatype_nsec, 0, 0, &rdataset,
					  NULL) == ISC_R_SUCCESS);
	if (!nokeys)
		changed = nsec_setbit(name, &rdataset, dns_rdatatype_rrsig, 1);
	if (changed) {
		dns_rdataset_disassociate(&rdataset);
		RUNTIME_CHECK(dns_db_findrdataset(gdb, node, gversion,
						  dns_rdatatype_nsec, 0, 0,
						  &rdataset,
						  NULL) == ISC_R_SUCCESS);
	}
	if (hasds)
		(void)nsec_setbit(name, &rdataset, dns_rdatatype_ds, 1);
	else
		(void)nsec_setbit(name, &rdataset, dns_rdatatype_ds, 0);
	dns_rdataset_disassociate(&rdataset);

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
			if (rdataset.type != dns_rdatatype_nsec &&
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
	dns_rdatasetiter_t *rdsiter;
	isc_boolean_t active = ISC_FALSE;
	isc_result_t result;
	dns_rdataset_t rdataset;

	dns_rdataset_init(&rdataset);
	rdsiter = NULL;
	result = dns_db_allrdatasets(gdb, node, gversion, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	result = dns_rdatasetiter_first(rdsiter);
	while (result == ISC_R_SUCCESS) {
		dns_rdatasetiter_current(rdsiter, &rdataset);
		if (rdataset.type != dns_rdatatype_nsec &&
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

	if (!active) {
		/*
		 * Make sure there is no NSEC / RRSIG records for
		 * this node.
		 */
		result = dns_db_deleterdataset(gdb, node, gversion,
					       dns_rdatatype_nsec, 0);
		if (result == DNS_R_UNCHANGED)
			result = ISC_R_SUCCESS;
		check_result(result, "dns_db_deleterdataset(nsec)");
		
		result = dns_rdatasetiter_first(rdsiter);
		for (result = dns_rdatasetiter_first(rdsiter);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(rdsiter)) {
			dns_rdatasetiter_current(rdsiter, &rdataset);
			if (rdataset.type == dns_rdatatype_rrsig) {
				dns_rdatatype_t type = rdataset.type;
				dns_rdatatype_t covers = rdataset.covers;
				result = dns_db_deleterdataset(gdb, node,
							       gversion, type,
							       covers);
				if (result == DNS_R_UNCHANGED)
					result = ISC_R_SUCCESS;
				check_result(result,
					     "dns_db_deleterdataset(rrsig)");
			}
			dns_rdataset_disassociate(&rdataset);
		}
		if (result != ISC_R_NOMORE)
			fatal("rdataset iteration failed: %s",
			      isc_result_totext(result));
	}
	dns_rdatasetiter_destroy(&rdsiter);

	return (active);
}

/*
 * Extracts the TTL from the SOA.
 */
static dns_ttl_t
soattl(void) {
	dns_rdataset_t soaset;
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_result_t result;
	dns_ttl_t ttl;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_soa_t soa;

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
	result = dns_rdata_tostruct(&rdata, &soa, NULL);
	check_result(result, "dns_rdata_tostruct");
	ttl = soa.minimum;
	dns_rdataset_disassociate(&soaset);
	return (ttl);
}

/*
 * Delete any RRSIG records at a node.
 */
static void
cleannode(dns_db_t *db, dns_dbversion_t *version, dns_dbnode_t *node) {
	dns_rdatasetiter_t *rdsiter = NULL;
	dns_rdataset_t set;
	isc_result_t result, dresult;

	dns_rdataset_init(&set);
	result = dns_db_allrdatasets(db, node, version, 0, &rdsiter);
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
			dresult = dns_db_deleterdataset(db, node, version,
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

/*
 * Set up the iterator and global state before starting the tasks.
 */
static void
presign(void) {
	isc_result_t result;

	gdbiter = NULL;
	result = dns_db_createiterator(gdb, ISC_FALSE, &gdbiter);
	check_result(result, "dns_db_createiterator()");

	result = dns_dbiterator_first(gdbiter);
	check_result(result, "dns_dbiterator_first()");
}

/*
 * Clean up the iterator and global state after the tasks complete.
 */
static void
postsign(void) {
	dns_dbiterator_destroy(&gdbiter);
}

/*
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

	if (shuttingdown)
		return;

	if (finished) {
		if (assigned == completed) {
			isc_task_detach(&task);
			isc_app_shutdown();
		}
		return;
	}

	fname = isc_mem_get(mctx, sizeof(dns_fixedname_t));
	if (fname == NULL)
		fatal("out of memory");
	dns_fixedname_init(fname);
	name = dns_fixedname_name(fname);
	node = NULL;
	found = ISC_FALSE;
	LOCK(&namelock);
	while (!found) {
		result = dns_dbiterator_current(gdbiter, &node, name);
		if (result != ISC_R_SUCCESS)
			fatal("failure iterating database: %s",
			      isc_result_totext(result));
		dns_rdataset_init(&nsec);
		result = dns_db_findrdataset(gdb, node, gversion,
					     dns_rdatatype_nsec, 0, 0,
					     &nsec, NULL);
		if (result == ISC_R_SUCCESS)
			found = ISC_TRUE;
		else
			dumpnode(name, node);
		if (dns_rdataset_isassociated(&nsec))
			dns_rdataset_disassociate(&nsec);
		if (!found)
			dns_db_detachnode(gdb, &node);

		result = dns_dbiterator_next(gdbiter);
		if (result == ISC_R_NOMORE) {
			finished = ISC_TRUE;
			break;
		} else if (result != ISC_R_SUCCESS)
			fatal("failure iterating database: %s",
			      isc_result_totext(result));
	}
	UNLOCK(&namelock);
	if (!found) {
		if (assigned == completed) {
			isc_task_detach(&task);
			isc_app_shutdown();
		}
		isc_mem_put(mctx, fname, sizeof(dns_fixedname_t));
		return;
	}
	sevent = (sevent_t *)
		 isc_event_allocate(mctx, task, SIGNER_EVENT_WORK,
				    sign, NULL, sizeof(sevent_t));
	if (sevent == NULL)
		fatal("failed to allocate event\n");

	sevent->node = node;
	sevent->fname = fname;
	isc_task_send(worker, ISC_EVENT_PTR(&sevent));
	assigned++;
}

/*
 * Start a worker task
 */
static void
startworker(isc_task_t *task, isc_event_t *event) {
	isc_task_t *worker;

	worker = (isc_task_t *)event->ev_arg;
	assignwork(task, worker);
	isc_event_free(&event);
}

/*
 * Write a node to the output file, and restart the worker task.
 */
static void
writenode(isc_task_t *task, isc_event_t *event) {
	isc_task_t *worker;
	sevent_t *sevent = (sevent_t *)event;

	completed++;
	worker = (isc_task_t *)event->ev_sender;
	dumpnode(dns_fixedname_name(sevent->fname), sevent->node);
	cleannode(gdb, gversion, sevent->node);
	dns_db_detachnode(gdb, &sevent->node);
	isc_mem_put(mctx, sevent->fname, sizeof(dns_fixedname_t));
	assignwork(task, worker);
	isc_event_free(&event);
}

/*
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

/*
 * Generate NSEC records for the zone.
 */
static void
nsecify(void) {
	dns_dbiterator_t *dbiter = NULL;
	dns_dbnode_t *node = NULL, *nextnode = NULL;
	dns_fixedname_t fname, fnextname, fzonecut;
	dns_name_t *name, *nextname, *zonecut;
	isc_boolean_t done = ISC_FALSE;
	isc_result_t result;

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_fixedname_init(&fnextname);
	nextname = dns_fixedname_name(&fnextname);
	dns_fixedname_init(&fzonecut);
	zonecut = NULL;

	result = dns_db_createiterator(gdb, ISC_FALSE, &dbiter);
	check_result(result, "dns_db_createiterator()");

	result = dns_dbiterator_first(dbiter);
	check_result(result, "dns_dbiterator_first()");

	while (!done) {
		dns_dbiterator_current(dbiter, &node, name);
		if (delegation(name, node, NULL)) {
			zonecut = dns_fixedname_name(&fzonecut);
			dns_name_copy(name, zonecut, NULL);
		}
		result = dns_dbiterator_next(dbiter);
		nextnode = NULL;
		while (result == ISC_R_SUCCESS) {
			isc_boolean_t active = ISC_FALSE;
			result = dns_dbiterator_current(dbiter, &nextnode,
							nextname);
			if (result != ISC_R_SUCCESS)
				break;
			active = active_node(nextnode);
			if (!active) {
				dns_db_detachnode(gdb, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			if (result != ISC_R_SUCCESS) {
				dns_db_detachnode(gdb, &nextnode);
				break;
			}
			if (!dns_name_issubdomain(nextname, gorigin) ||
			    (zonecut != NULL &&
			     dns_name_issubdomain(nextname, zonecut)))
			{
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
		result = dns_nsec_build(gdb, gversion, node, nextname,
					zonettl);
		check_result(result, "dns_nsec_build()");
		dns_db_detachnode(gdb, &node);
	}

	dns_dbiterator_destroy(&dbiter);
}

/*
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
	result = dns_name_fromtext(name, &b, dns_rootname, ISC_FALSE, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("failed converting name '%s' to dns format: %s",
		      origin, isc_result_totext(result));

	result = dns_db_create(mctx, "rbt", name, dns_dbtype_zone,
			       rdclass, 0, NULL, db);
	check_result(result, "dns_db_create()");

	result = dns_db_load(*db, file);
	if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE)
		fatal("failed loading zone from '%s': %s",
		      file, isc_result_totext(result));
}

/*
 * Finds all public zone keys in the zone, and attempts to load the
 * private keys from disk.
 */
static void
loadzonekeys(dns_db_t *db) {
	dns_dbnode_t *node;
	dns_dbversion_t *currentversion;
	isc_result_t result;
	dst_key_t *keys[20];
	unsigned int nkeys, i;

	currentversion = NULL;
	dns_db_currentversion(db, &currentversion);

	node = NULL;
	result = dns_db_findnode(db, gorigin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find the zone's origin: %s",
		      isc_result_totext(result));

	result = dns_dnssec_findzonekeys(db, currentversion, node, gorigin,
					 mctx, 20, keys, &nkeys);
	if (result == ISC_R_NOTFOUND)
		result = ISC_R_SUCCESS;
	if (result != ISC_R_SUCCESS)
		fatal("failed to find the zone keys: %s",
		      isc_result_totext(result));

	for (i = 0; i < nkeys; i++) {
		signer_key_t *key;

		key = newkeystruct(keys[i], ISC_TRUE);
		ISC_LIST_APPEND(keylist, key, link);
	}
	dns_db_detachnode(db, &node);
	dns_db_closeversion(db, &currentversion, ISC_FALSE);
}

/*
 * Finds all public zone keys in the zone.
 */
static void
loadzonepubkeys(dns_db_t *db) {
	dns_dbversion_t *currentversion = NULL;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dst_key_t *pubkey;
	signer_key_t *key;
	isc_result_t result;

	dns_db_currentversion(db, &currentversion);

	result = dns_db_findnode(db, gorigin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find the zone's origin: %s",
		      isc_result_totext(result));

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, currentversion,
				     dns_rdatatype_dnskey, 0, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find keys at the zone apex: %s",
		      isc_result_totext(result));
	result = dns_rdataset_first(&rdataset);
	check_result(result, "dns_rdataset_first");
	while (result == ISC_R_SUCCESS) {
		pubkey = NULL;
		dns_rdata_reset(&rdata);
		dns_rdataset_current(&rdataset, &rdata);
		result = dns_dnssec_keyfromrdata(gorigin, &rdata, mctx,
						 &pubkey);
		if (result != ISC_R_SUCCESS)
			goto next;
		if (!dst_key_iszonekey(pubkey)) {
			dst_key_free(&pubkey);
			goto next;
		}

		key = newkeystruct(pubkey, ISC_FALSE);
		ISC_LIST_APPEND(keylist, key, link);
 next:
		result = dns_rdataset_next(&rdataset);
	}
	dns_rdataset_disassociate(&rdataset);
	dns_db_detachnode(db, &node);
	dns_db_closeversion(db, &currentversion, ISC_FALSE);
}

static void
warnifallksk(dns_db_t *db) {
	dns_dbversion_t *currentversion = NULL;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dst_key_t *pubkey;
	isc_result_t result;
	dns_rdata_key_t key;
	isc_boolean_t have_non_ksk = ISC_FALSE;

	dns_db_currentversion(db, &currentversion);

	result = dns_db_findnode(db, gorigin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find the zone's origin: %s",
		      isc_result_totext(result));

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, currentversion,
				     dns_rdatatype_dnskey, 0, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find keys at the zone apex: %s",
		      isc_result_totext(result));
	result = dns_rdataset_first(&rdataset);
	check_result(result, "dns_rdataset_first");
	while (result == ISC_R_SUCCESS) {
		pubkey = NULL;
		dns_rdata_reset(&rdata);
		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &key, NULL);
		check_result(result, "dns_rdata_tostruct");
		if ((key.flags & DNS_KEYFLAG_KSK) == 0) {
			have_non_ksk = ISC_TRUE;
			result = ISC_R_NOMORE;
		} else
			result = dns_rdataset_next(&rdataset);
	}
	dns_rdataset_disassociate(&rdataset);
	dns_db_detachnode(db, &node);
	dns_db_closeversion(db, &currentversion, ISC_FALSE);
	if (!have_non_ksk && !ignoreksk)
		fprintf(stderr, "%s: warning: No non-KSK dnskey found. "
			"Supply non-KSK dnskey or use '-z'.\n",
			program);
}

static void
writeset(const char *prefix, dns_rdatatype_t type) {
	char *filename;
	char namestr[DNS_NAME_FORMATSIZE];
	dns_db_t *db = NULL;
	dns_dbversion_t *version = NULL;
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
	signer_key_t *key;
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
	if (directory != NULL)
		filenamelen += strlen(directory) + 1;
	filename = isc_mem_get(mctx, filenamelen + 1);
	if (filename == NULL)
		fatal("out of memory");
	if (directory != NULL)
		sprintf(filename, "%s/", directory);
	else
		filename[0] = 0;
	strcat(filename, prefix);
	strcat(filename, namestr);

	dns_diff_init(mctx, &diff);

	for (key = ISC_LIST_HEAD(keylist);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link))
		if (!key->isksk) {
			have_non_ksk = ISC_TRUE;
			break;
		}

	for (key = ISC_LIST_HEAD(keylist);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link))
		if (key->isksk) {
			have_ksk = ISC_TRUE;
			break;
		}

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
		if (have_ksk && have_non_ksk && !key->isksk)
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
		} else
			result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD,
						      gorigin, zonettl,
						      &rdata, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(&diff, &tuple);
	}

	result = dns_db_create(mctx, "rbt", dns_rootname, dns_dbtype_zone,
			       gclass, 0, NULL, &db);
	check_result(result, "dns_db_create");

	result = dns_db_newversion(db, &version);
	check_result(result, "dns_db_newversion");

	result = dns_diff_apply(&diff, db, version);
	check_result(result, "dns_diff_apply");
	dns_diff_clear(&diff);

	result = dns_master_dump(mctx, db, version, style, filename);
	check_result(result, "dns_master_dump");

	isc_mem_put(mctx, filename, filenamelen + 1);

	dns_db_closeversion(db, &version, ISC_FALSE);
	dns_db_detach(&db);
}

static void
print_time(FILE *fp) {
	time_t currenttime;

	currenttime = time(NULL);
	fprintf(fp, "; File written on %s", ctime(&currenttime));
}

static void
print_version(FILE *fp) {
	fprintf(fp, "; dnssec_signzone version " VERSION "\n");
}

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s [options] zonefile [keys]\n", program);

	fprintf(stderr, "\n");

	fprintf(stderr, "Version: %s\n", VERSION);

	fprintf(stderr, "Options: (default value in parenthesis) \n");
	fprintf(stderr, "\t-c class (IN)\n");
	fprintf(stderr, "\t-d directory\n");
	fprintf(stderr, "\t\tdirectory to find keyset files (.)\n");
	fprintf(stderr, "\t-g:\t");
	fprintf(stderr, "generate DS records from keyset files\n");
	fprintf(stderr, "\t-s YYYYMMDDHHMMSS|+offset:\n");
	fprintf(stderr, "\t\tRRSIG start time - absolute|offset (now - 1 hour)\n");
	fprintf(stderr, "\t-e YYYYMMDDHHMMSS|+offset|\"now\"+offset]:\n");
	fprintf(stderr, "\t\tRRSIG end time  - absolute|from start|from now "
				"(now + 30 days)\n");
	fprintf(stderr, "\t-i interval:\n");
	fprintf(stderr, "\t\tcycle interval - resign "
				"if < interval from end ( (end-start)/4 )\n");
	fprintf(stderr, "\t-v debuglevel (0)\n");
	fprintf(stderr, "\t-o origin:\n");
	fprintf(stderr, "\t\tzone origin (name of zonefile)\n");
	fprintf(stderr, "\t-f outfile:\n");
	fprintf(stderr, "\t\tfile the signed zone is written in "
				"(zonefile + .signed)\n");
	fprintf(stderr, "\t-r randomdev:\n");
	fprintf(stderr,	"\t\ta file containing random data\n");
	fprintf(stderr, "\t-a:\t");
	fprintf(stderr, "verify generated signatures\n");
	fprintf(stderr, "\t-p:\t");
	fprintf(stderr, "use pseudorandom data (faster but less secure)\n");
	fprintf(stderr, "\t-t:\t");
	fprintf(stderr, "print statistics\n");
	fprintf(stderr, "\t-n ncpus (number of cpus present)\n");
	fprintf(stderr, "\t-k key_signing_key\n");
	fprintf(stderr, "\t-l lookasidezone\n");
	fprintf(stderr, "\t-z:\t");
	fprintf(stderr, "ignore KSK flag in DNSKEYs");

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
print_stats(isc_time_t *timer_start, isc_time_t *timer_finish) {
	isc_uint64_t runtime_us;   /* Runtime in microseconds */
	isc_uint64_t runtime_ms;   /* Runtime in milliseconds */
	isc_uint64_t sig_ms;	   /* Signatures per millisecond */

	runtime_us = isc_time_microdiff(timer_finish, timer_start);

	printf("Signatures generated:               %10d\n", nsigned);
	printf("Signatures retained:                %10d\n", nretained);
	printf("Signatures dropped:                 %10d\n", ndropped);
	printf("Signatures successfully verified:   %10d\n", nverified);
	printf("Signatures unsuccessfully verified: %10d\n", nverifyfailed);
	runtime_ms = runtime_us / 1000;
	printf("Runtime in seconds:                %7u.%03u\n", 
	       (unsigned int) (runtime_ms / 1000), 
	       (unsigned int) (runtime_ms % 1000));
	if (runtime_us > 0) {
		sig_ms = ((isc_uint64_t)nsigned * 1000000000) / runtime_us;
		printf("Signatures per second:             %7u.%03u\n",
		       (unsigned int) sig_ms / 1000, 
		       (unsigned int) sig_ms % 1000);
	}
}

int
main(int argc, char *argv[]) {
	int i, ch;
	char *startstr = NULL, *endstr = NULL, *classname = NULL;
	char *origin = NULL, *file = NULL, *output = NULL;
	char *dskeyfile[MAXDSKEYS];
	int ndskeys = 0;
	char *endp;
	isc_time_t timer_start, timer_finish;
	signer_key_t *key;
	isc_result_t result;
	isc_log_t *log = NULL;
	isc_boolean_t pseudorandom = ISC_FALSE;
	unsigned int eflags;
	isc_boolean_t free_output = ISC_FALSE;
	int tempfilelen;
	dns_rdataclass_t rdclass;
	dns_db_t *udb = NULL;
	isc_task_t **tasks = NULL;
	isc_buffer_t b;
	int len;

	masterstyle = &dns_master_style_explicitttl;

	check_result(isc_app_start(), "isc_app_start");

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("out of memory");

	dns_result_register();

	while ((ch = isc_commandline_parse(argc, argv,
					   "ac:d:e:f:ghi:k:l:n:o:pr:s:Stv:z"))
	       != -1) {
		switch (ch) {
		case 'a':
			tryverify = ISC_TRUE;
			break;

		case 'c':
			classname = isc_commandline_argument;
			break;

		case 'd':
			directory = isc_commandline_argument;
			break;

		case 'e':
			endstr = isc_commandline_argument;
			break;

		case 'f':
			output = isc_commandline_argument;
			break;

		case 'g':
			generateds = ISC_TRUE;
			break;

		case 'h':
		default:
			usage();
			break;

		case 'i':
			endp = NULL;
			cycle = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0' || cycle < 0)
				fatal("cycle period must be numeric and "
				      "positive");
			break;

		case 'l': 
			dns_fixedname_init(&dlv_fixed);
			len = strlen(isc_commandline_argument);
			isc_buffer_init(&b, isc_commandline_argument, len);
			isc_buffer_add(&b, len);

			dns_fixedname_init(&dlv_fixed);
			dlv = dns_fixedname_name(&dlv_fixed);
			result = dns_name_fromtext(dlv, &b, dns_rootname,
						   ISC_FALSE, NULL);
			check_result(result, "dns_name_fromtext(dlv)");
			break;

		case 'k':
			if (ndskeys == MAXDSKEYS)
				fatal("too many key-signing keys specified");
			dskeyfile[ndskeys++] = isc_commandline_argument;
			break;

		case 'n':
			endp = NULL;
			ntasks = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0' || ntasks > ISC_INT32_MAX)
				fatal("number of cpus must be numeric");
			break;

		case 'o':
			origin = isc_commandline_argument;
			break;

		case 'p':
			pseudorandom = ISC_TRUE;
			break;

		case 'r':
			setup_entropy(mctx, isc_commandline_argument, &ectx);
			break;

		case 's':
			startstr = isc_commandline_argument;
			break;

		case 'S':
			/* This is intentionally undocumented */
			/* -S: simple output style */
			masterstyle = &dns_master_style_simple;
			break;

		case 't':
			printstats = ISC_TRUE;
			break;

		case 'v':
			endp = NULL;
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("verbose level must be numeric");
			break;

		case 'z':
			ignoreksk = ISC_TRUE;
			break;
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

	result = dst_lib_init(mctx, ectx, eflags);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize dst");

	isc_stdtime_get(&now);

	if (startstr != NULL)
		starttime = strtotime(startstr, now, now);
	else
		starttime = now - 3600;  /* Allow for some clock skew. */

	if (endstr != NULL)
		endtime = strtotime(endstr, now, starttime);
	else
		endtime = starttime + (30 * 24 * 60 * 60);

	if (cycle == -1)
		cycle = (endtime - starttime) / 4;

	if (ntasks == 0)
		ntasks = isc_os_ncpus();
	vbprintf(4, "using %d cpus\n", ntasks);

	rdclass = strtoclass(classname);

	setup_logging(verbose, mctx, &log);

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

	result = dns_master_stylecreate(&dsstyle,  DNS_STYLEFLAG_NO_TTL,
					0, 24, 0, 0, 0, 8, mctx);
	check_result(result, "dns_master_stylecreate");
					

	gdb = NULL;
	TIME_NOW(&timer_start);
	loadzone(file, origin, rdclass, &gdb);
	gorigin = dns_db_origin(gdb);
	gclass = dns_db_class(gdb);
	zonettl = soattl();

	ISC_LIST_INIT(keylist);

	if (argc == 0) {
		loadzonekeys(gdb);
	} else {
		for (i = 0; i < argc; i++) {
			dst_key_t *newkey = NULL;

			result = dst_key_fromnamedfile(argv[i],
						       DST_TYPE_PUBLIC |
						       DST_TYPE_PRIVATE,
						       mctx, &newkey);
			if (result != ISC_R_SUCCESS)
				fatal("cannot load dnskey %s: %s", argv[i], 
				      isc_result_totext(result)); 

			key = ISC_LIST_HEAD(keylist);
			while (key != NULL) {
				dst_key_t *dkey = key->key;
				if (dst_key_id(dkey) == dst_key_id(newkey) &&
				    dst_key_alg(dkey) == dst_key_alg(newkey) &&
				    dns_name_equal(dst_key_name(dkey),
					    	   dst_key_name(newkey)))
				{
					if (!dst_key_isprivate(dkey))
						fatal("cannot sign zone with "
						      "non-private dnskey %s",
						      argv[i]);
					break;
				}
				key = ISC_LIST_NEXT(key, link);
			}
			if (key == NULL) {
				key = newkeystruct(newkey, ISC_TRUE);
				ISC_LIST_APPEND(keylist, key, link);
			} else
				dst_key_free(&newkey);
		}

		loadzonepubkeys(gdb);
	}

	for (i = 0; i < ndskeys; i++) {
		dst_key_t *newkey = NULL;

		result = dst_key_fromnamedfile(dskeyfile[i],
					       DST_TYPE_PUBLIC |
					       DST_TYPE_PRIVATE,
					       mctx, &newkey);
		if (result != ISC_R_SUCCESS)
			fatal("cannot load dnskey %s: %s", dskeyfile[i],
			      isc_result_totext(result)); 

		key = ISC_LIST_HEAD(keylist);
		while (key != NULL) {
			dst_key_t *dkey = key->key;
			if (dst_key_id(dkey) == dst_key_id(newkey) &&
			    dst_key_alg(dkey) == dst_key_alg(newkey) &&
			    dns_name_equal(dst_key_name(dkey),
				    	   dst_key_name(newkey)))
			{
				/* Override key flags. */
				key->issigningkey = ISC_TRUE;
				key->isksk = ISC_TRUE;
				key->isdsk = ISC_FALSE;
				dst_key_free(&dkey);
				key->key = newkey;
				break;
			}
			key = ISC_LIST_NEXT(key, link);
		}
		if (key == NULL) {
			/* Override dnskey flags. */
			key = newkeystruct(newkey, ISC_TRUE);
			key->isksk = ISC_TRUE;
			key->isdsk = ISC_FALSE;
			ISC_LIST_APPEND(keylist, key, link);
		}
	}

	if (ISC_LIST_EMPTY(keylist)) {
		fprintf(stderr, "%s: warning: No keys specified or found\n",
			program);
		nokeys = ISC_TRUE;
	}

	warnifallksk(gdb);

	gversion = NULL;
	result = dns_db_newversion(gdb, &gversion);
	check_result(result, "dns_db_newversion()");

	nsecify();

	if (!nokeys) {
		writeset("keyset-", dns_rdatatype_dnskey);
		writeset("dsset-", dns_rdatatype_ds);
		if (dlv != NULL) {
			writeset("dlvset-", dns_rdatatype_dlv);
		}
	}

	tempfilelen = strlen(output) + 20;
	tempfile = isc_mem_get(mctx, tempfilelen);
	if (tempfile == NULL)
		fatal("out of memory");

	result = isc_file_mktemplate(output, tempfile, tempfilelen);
	check_result(result, "isc_file_mktemplate");

	fp = NULL;
	result = isc_file_openunique(tempfile, &fp);
	if (result != ISC_R_SUCCESS)
		fatal("failed to open temporary output file: %s",
		      isc_result_totext(result));
	removefile = ISC_TRUE;
	setfatalcallback(&removetempfile);

	print_time(fp);
	print_version(fp);

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
		result = isc_app_onrun(mctx, master, startworker, tasks[i]);
		if (result != ISC_R_SUCCESS)
			fatal("failed to start task: %s",
			      isc_result_totext(result));
	}

	RUNTIME_CHECK(isc_mutex_init(&namelock) == ISC_R_SUCCESS);
	if (printstats)
		RUNTIME_CHECK(isc_mutex_init(&statslock) == ISC_R_SUCCESS);

	presign();
	(void)isc_app_run();
	if (!finished)
		fatal("process aborted by user");
	shuttingdown = ISC_TRUE;
	for (i = 0; i < (int)ntasks; i++)
		isc_task_detach(&tasks[i]);
	isc_taskmgr_destroy(&taskmgr);
	isc_mem_put(mctx, tasks, ntasks * sizeof(isc_task_t *));
	postsign();

	if (udb != NULL) {
		dumpdb(udb);
		dns_db_detach(&udb);
	}

	result = isc_stdio_close(fp);
	check_result(result, "isc_stdio_close");
	removefile = ISC_FALSE;

	result = isc_file_rename(tempfile, output);
	if (result != ISC_R_SUCCESS)
		fatal("failed to rename temp file to %s: %s\n",
		      output, isc_result_totext(result));

	DESTROYLOCK(&namelock);
	if (printstats)
		DESTROYLOCK(&statslock);

	printf("%s\n", output);

	dns_db_closeversion(gdb, &gversion, ISC_FALSE);
	dns_db_detach(&gdb);

	while (!ISC_LIST_EMPTY(keylist)) {
		key = ISC_LIST_HEAD(keylist);
		ISC_LIST_UNLINK(keylist, key, link);
		dst_key_free(&key->key);
		isc_mem_put(mctx, key, sizeof(signer_key_t));
	}

	isc_mem_put(mctx, tempfile, tempfilelen);

	if (free_output)
		isc_mem_free(mctx, output);

	dns_master_styledestroy(&dsstyle, mctx);

	cleanup_logging(&log);
	dst_lib_destroy();
	isc_hash_destroy();
	cleanup_entropy(&ectx);
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	(void) isc_app_finish();

	if (printstats) {
		TIME_NOW(&timer_finish);
		print_stats(&timer_start, &timer_finish);
	}

	return (0);
}
