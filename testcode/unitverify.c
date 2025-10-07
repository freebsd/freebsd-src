/*
 * testcode/unitverify.c - unit test for signature verification routines.
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/**
 * \file
 * Calls verification unit tests. Exits with code 1 on a failure. 
 */

#include "config.h"
#include "util/log.h"
#include "testcode/unitmain.h"
#include "validator/val_sigcrypt.h"
#include "validator/val_secalgo.h"
#include "validator/val_nsec.h"
#include "validator/val_nsec3.h"
#include "validator/validator.h"
#include "testcode/testpkts.h"
#include "util/data/msgreply.h"
#include "util/data/msgparse.h"
#include "util/data/dname.h"
#include "util/regional.h"
#include "util/alloc.h"
#include "util/rbtree.h"
#include "util/net_help.h"
#include "util/module.h"
#include "util/config_file.h"
#include "sldns/sbuffer.h"
#include "sldns/keyraw.h"
#include "sldns/str2wire.h"
#include "sldns/wire2str.h"

#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL_ERR_H
#include <openssl/err.h>
#endif
#endif

/** verbose signature test */
static int vsig = 0;

/** entry to packet buffer with wireformat */
static void
entry_to_buf(struct entry* e, sldns_buffer* pkt)
{
	unit_assert(e->reply_list);
	if(e->reply_list->reply_from_hex) {
		sldns_buffer_copy(pkt, e->reply_list->reply_from_hex);
	} else {
		sldns_buffer_clear(pkt);
		sldns_buffer_write(pkt, e->reply_list->reply_pkt,
			e->reply_list->reply_len);
		sldns_buffer_flip(pkt);
	}
}

/** entry to reply info conversion */
static void
entry_to_repinfo(struct entry* e, struct alloc_cache* alloc, 
	struct regional* region, sldns_buffer* pkt, struct query_info* qi, 
	struct reply_info** rep)
{
	int ret;
	struct edns_data edns;
	entry_to_buf(e, pkt);
	/* lock alloc lock to please lock checking software. 
	 * alloc_special_obtain assumes it is talking to a ub-alloc,
	 * and does not need to perform locking. Here the alloc is
	 * the only one, so we lock it here */
	lock_quick_lock(&alloc->lock);
	ret = reply_info_parse(pkt, alloc, qi, rep, region, &edns);
	lock_quick_unlock(&alloc->lock);
	if(ret != 0) {
		char rcode[16];
		sldns_wire2str_rcode_buf(ret, rcode, sizeof(rcode));
		printf("parse code %d: %s\n", ret, rcode);
		unit_assert(ret != 0);
	}
}

/** extract DNSKEY rrset from answer and convert it */
static struct ub_packed_rrset_key* 
extract_keys(struct entry* e, struct alloc_cache* alloc, 
	struct regional* region, sldns_buffer* pkt)
{
	struct ub_packed_rrset_key* dnskey = NULL;
	struct query_info qinfo;
	struct reply_info* rep = NULL;
	size_t i;

	entry_to_repinfo(e, alloc, region, pkt, &qinfo, &rep);
	for(i=0; i<rep->an_numrrsets; i++) {
		if(ntohs(rep->rrsets[i]->rk.type) == LDNS_RR_TYPE_DNSKEY) {
			dnskey = rep->rrsets[i];
			rep->rrsets[i] = NULL;
			break;
		}
	}
	unit_assert(dnskey);

	reply_info_parsedelete(rep, alloc);
	query_info_clear(&qinfo);
	return dnskey;
}

/** return true if answer should be bogus */
static int
should_be_bogus(struct ub_packed_rrset_key* rrset, struct query_info* qinfo)
{
	struct packed_rrset_data* d = (struct packed_rrset_data*)rrset->
		entry.data;
	if(d->rrsig_count == 0)
		return 1;
	/* name 'bogus' as first label signals bogus */
	if(rrset->rk.dname_len > 6 && memcmp(rrset->rk.dname+1, "bogus", 5)==0)
		return 1;
	if(qinfo->qname_len > 6 && memcmp(qinfo->qname+1, "bogus", 5)==0)
		return 1;
	return 0;
}

/** return number of rrs in an rrset */
static size_t
rrset_get_count(struct ub_packed_rrset_key* rrset)
{
	struct packed_rrset_data* d = (struct packed_rrset_data*)
	rrset->entry.data;
	if(!d) return 0;
	return d->count;
}

/** setup sig alg list from dnskey */
static void
setup_sigalg(struct ub_packed_rrset_key* dnskey, uint8_t* sigalg)
{
	uint8_t a[ALGO_NEEDS_MAX];
	size_t i, n = 0;
	memset(a, 0, sizeof(a));
	for(i=0; i<rrset_get_count(dnskey); i++) {
		uint8_t algo = (uint8_t)dnskey_get_algo(dnskey, i);
		if(a[algo] == 0) {
			a[algo] = 1;
			sigalg[n++] = algo;
		}
	}
	sigalg[n] = 0;
}

/** verify and test one rrset against the key rrset */
static void
verifytest_rrset(struct module_env* env, struct val_env* ve, 
	struct ub_packed_rrset_key* rrset, struct ub_packed_rrset_key* dnskey,
	struct query_info* qinfo)
{
	enum sec_status sec;
	char reasonbuf[256];
	char* reason = NULL;
	uint8_t sigalg[ALGO_NEEDS_MAX+1];
	int verified = 0;
	if(vsig) {
		log_nametypeclass(VERB_QUERY, "verify of rrset",
			rrset->rk.dname, ntohs(rrset->rk.type),
			ntohs(rrset->rk.rrset_class));
	}
	setup_sigalg(dnskey, sigalg); /* check all algorithms in the dnskey */
	/* ok to give null as qstate here, won't be used for answer section. */
	sec = dnskeyset_verify_rrset(env, ve, rrset, dnskey, sigalg, &reason,
		NULL, LDNS_SECTION_ANSWER, NULL, &verified, reasonbuf,
		sizeof(reasonbuf));
	if(vsig) {
		printf("verify outcome is: %s %s\n", sec_status_to_string(sec),
			reason?reason:"");
	}
	if(should_be_bogus(rrset, qinfo)) {
		unit_assert(sec == sec_status_bogus);
	} else {
		unit_assert(sec == sec_status_secure);
	}
}

/** verify and test an entry - every rr in the message */
static void
verifytest_entry(struct entry* e, struct alloc_cache* alloc, 
	struct regional* region, sldns_buffer* pkt, 
	struct ub_packed_rrset_key* dnskey, struct module_env* env, 
	struct val_env* ve)
{
	struct query_info qinfo;
	struct reply_info* rep = NULL;
	size_t i;

	regional_free_all(region);
	if(vsig) {
		char* s = sldns_wire2str_pkt(e->reply_list->reply_pkt,
			e->reply_list->reply_len);
		printf("verifying pkt:\n%s\n", s?s:"outofmemory");
		free(s);
	}
	entry_to_repinfo(e, alloc, region, pkt, &qinfo, &rep);

	for(i=0; i<rep->rrset_count; i++) {
		verifytest_rrset(env, ve, rep->rrsets[i], dnskey, &qinfo);
	}

	reply_info_parsedelete(rep, alloc);
	query_info_clear(&qinfo);
}

/** find RRset in reply by type */
static struct ub_packed_rrset_key*
find_rrset_type(struct reply_info* rep, uint16_t type)
{
	size_t i;
	for(i=0; i<rep->rrset_count; i++) {
		if(ntohs(rep->rrsets[i]->rk.type) == type)
			return rep->rrsets[i];
	}
	return NULL;
}

/** DS sig test an entry - get DNSKEY and DS in entry and verify */
static void
dstest_entry(struct entry* e, struct alloc_cache* alloc, 
	struct regional* region, sldns_buffer* pkt, struct module_env* env)
{
	struct query_info qinfo;
	struct reply_info* rep = NULL;
	struct ub_packed_rrset_key* ds, *dnskey;
	int ret;

	regional_free_all(region);
	if(vsig) {
		char* s = sldns_wire2str_pkt(e->reply_list->reply_pkt,
			e->reply_list->reply_len);
		printf("verifying DS-DNSKEY match:\n%s\n", s?s:"outofmemory");
		free(s);
	}
	entry_to_repinfo(e, alloc, region, pkt, &qinfo, &rep);
	ds = find_rrset_type(rep, LDNS_RR_TYPE_DS);
	dnskey = find_rrset_type(rep, LDNS_RR_TYPE_DNSKEY);
	/* check test is OK */
	unit_assert(ds && dnskey);

	ret = ds_digest_match_dnskey(env, dnskey, 0, ds, 0);
	if(strncmp((char*)qinfo.qname, "\003yes", 4) == 0) {
		if(vsig) {
			printf("result(yes)= %s\n", ret?"yes":"no");
		}
		unit_assert(ret);
	} else if (strncmp((char*)qinfo.qname, "\002no", 3) == 0) {
		if(vsig) {
			printf("result(no)= %s\n", ret?"yes":"no");
		}
		unit_assert(!ret);
		verbose(VERB_QUERY, "DS fail: OK; matched unit test");
	} else {
		fatal_exit("Bad qname in DS unit test, yes or no");
	}

	reply_info_parsedelete(rep, alloc);
	query_info_clear(&qinfo);
}

/** verify from a file */
static void
verifytest_file(const char* fname, const char* at_date)
{
	/* 
	 * The file contains a list of ldns-testpkts entries.
	 * The first entry must be a query for DNSKEY.
	 * The answer rrset is the keyset that will be used for verification
	 */
	struct ub_packed_rrset_key* dnskey;
	struct regional* region = regional_create();
	struct alloc_cache alloc;
	sldns_buffer* buf = sldns_buffer_new(65535);
	struct entry* e;
	struct entry* list = read_datafile(fname, 1);
	struct module_env env;
	struct val_env ve;
	time_t now = time(NULL);
	unit_show_func("signature verify", fname);

	if(!list)
		fatal_exit("could not read %s: %s", fname, strerror(errno));
	alloc_init(&alloc, NULL, 1);
	memset(&env, 0, sizeof(env));
	memset(&ve, 0, sizeof(ve));
	env.scratch = region;
	env.scratch_buffer = buf;
	env.now = &now;
	ve.date_override = cfg_convert_timeval(at_date);
	unit_assert(region && buf);
	dnskey = extract_keys(list, &alloc, region, buf);
	if(vsig) log_nametypeclass(VERB_QUERY, "test dnskey",
			dnskey->rk.dname, ntohs(dnskey->rk.type), 
			ntohs(dnskey->rk.rrset_class));
	/* ready to go! */
	for(e = list->next; e; e = e->next) {
		verifytest_entry(e, &alloc, region, buf, dnskey, &env, &ve);
	}

	ub_packed_rrset_parsedelete(dnskey, &alloc);
	delete_entry(list);
	regional_destroy(region);
	alloc_clear(&alloc);
	sldns_buffer_free(buf);
}

/** verify DS matches DNSKEY from a file */
static void
dstest_file(const char* fname)
{
	/* 
	 * The file contains a list of ldns-testpkts entries.
	 * The first entry must be a query for DNSKEY.
	 * The answer rrset is the keyset that will be used for verification
	 */
	struct regional* region = regional_create();
	struct alloc_cache alloc;
	sldns_buffer* buf = sldns_buffer_new(65535);
	struct entry* e;
	struct entry* list = read_datafile(fname, 1);
	struct module_env env;
	unit_show_func("DS verify", fname);

	if(!list)
		fatal_exit("could not read %s: %s", fname, strerror(errno));
	alloc_init(&alloc, NULL, 1);
	memset(&env, 0, sizeof(env));
	env.scratch = region;
	env.scratch_buffer = buf;
	unit_assert(region && buf);

	/* ready to go! */
	for(e = list; e; e = e->next) {
		dstest_entry(e, &alloc, region, buf, &env);
	}

	delete_entry(list);
	regional_destroy(region);
	alloc_clear(&alloc);
	sldns_buffer_free(buf);
}

/** helper for unittest of NSEC routines */
static int
unitest_nsec_has_type_rdata(char* bitmap, size_t len, uint16_t type)
{
	return nsecbitmap_has_type_rdata((uint8_t*)bitmap, len, type);
}

/** Test NSEC type bitmap routine */
static void
nsectest(void)
{
	/* bitmap starts at type bitmap rdata field */
	/* from rfc 4034 example */
	char* bitmap = "\000\006\100\001\000\000\000\003"
		"\004\033\000\000\000\000\000\000"
		"\000\000\000\000\000\000\000\000"
		"\000\000\000\000\000\000\000\000"
		"\000\000\000\000\040";
	size_t len = 37;

	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 0));
	unit_assert(unitest_nsec_has_type_rdata(bitmap, len, LDNS_RR_TYPE_A));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 2));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 3));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 4));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 5));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 6));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 7));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 8));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 9));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 10));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 11));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 12));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 13));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 14));
	unit_assert(unitest_nsec_has_type_rdata(bitmap, len, LDNS_RR_TYPE_MX));
	unit_assert(unitest_nsec_has_type_rdata(bitmap, len, LDNS_RR_TYPE_RRSIG));
	unit_assert(unitest_nsec_has_type_rdata(bitmap, len, LDNS_RR_TYPE_NSEC));
	unit_assert(unitest_nsec_has_type_rdata(bitmap, len, 1234));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 1233));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 1235));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 1236));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 1237));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 1238));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 1239));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 1240));
	unit_assert(!unitest_nsec_has_type_rdata(bitmap, len, 2230));
}

/** Test hash algo - NSEC3 hash it and compare result */
static void
nsec3_hash_test_entry(struct entry* e, rbtree_type* ct,
	struct alloc_cache* alloc, struct regional* region, 
	sldns_buffer* buf)
{
	struct query_info qinfo;
	struct reply_info* rep = NULL;
	struct ub_packed_rrset_key* answer, *nsec3, *nsec3_region;
	struct nsec3_cached_hash* hash = NULL;
	int ret;
	uint8_t* qname;

	if(vsig) {
		char* s = sldns_wire2str_pkt(e->reply_list->reply_pkt,
			e->reply_list->reply_len);
		printf("verifying NSEC3 hash:\n%s\n", s?s:"outofmemory");
		free(s);
	}
	entry_to_repinfo(e, alloc, region, buf, &qinfo, &rep);
	nsec3 = find_rrset_type(rep, LDNS_RR_TYPE_NSEC3);
	answer = find_rrset_type(rep, LDNS_RR_TYPE_AAAA);
	qname = regional_alloc_init(region, qinfo.qname, qinfo.qname_len);
	/* check test is OK */
	unit_assert(nsec3 && answer && qname);

	/* Copy the nsec3 to the region, so it can stay referenced by the
	 * ct tree entry. The region is freed when the file is done. */
	nsec3_region = packed_rrset_copy_region(nsec3, region, 0);

	ret = nsec3_hash_name(ct, region, buf, nsec3_region, 0, qname,
		qinfo.qname_len, &hash);
	if(ret < 1) {
		printf("Bad nsec3_hash_name retcode %d\n", ret);
		unit_assert(ret == 1 || ret == 2);
	}
	unit_assert(hash->dname && hash->hash && hash->hash_len &&
		hash->b32 && hash->b32_len);
	unit_assert(hash->b32_len == (size_t)answer->rk.dname[0]);
	/* does not do lowercasing. */
	unit_assert(memcmp(hash->b32, answer->rk.dname+1, hash->b32_len) 
		== 0);

	reply_info_parsedelete(rep, alloc);
	query_info_clear(&qinfo);
}


/** Read file to test NSEC3 hash algo */
static void
nsec3_hash_test(const char* fname)
{
	/* 
	 * The list contains a list of ldns-testpkts entries.
	 * Every entry is a test.
	 * 	The qname is hashed.
	 * 	The answer section AAAA RR name is the required result.
	 * 	The auth section NSEC3 is used to get hash parameters.
	 * The hash cache is maintained per file.
	 *
	 * The test does not perform canonicalization during the compare.
	 */
	rbtree_type ct;
	struct regional* region = regional_create();
	struct alloc_cache alloc;
	sldns_buffer* buf = sldns_buffer_new(65535);
	struct entry* e;
	struct entry* list = read_datafile(fname, 1);
	unit_show_func("NSEC3 hash", fname);

	if(!list)
		fatal_exit("could not read %s: %s", fname, strerror(errno));
	rbtree_init(&ct, &nsec3_hash_cmp);
	alloc_init(&alloc, NULL, 1);
	unit_assert(region && buf);

	/* ready to go! */
	for(e = list; e; e = e->next) {
		nsec3_hash_test_entry(e, &ct, &alloc, region, buf);
	}

	delete_entry(list);
	regional_destroy(region);
	alloc_clear(&alloc);
	sldns_buffer_free(buf);
}

#define xstr(s) str(s)
#define str(s) #s

#define SRCDIRSTR xstr(SRCDIR)

#if defined(HAVE_SSL) && defined(USE_SHA1)
/* Detect if openssl is configured to disable RSASHA1 signatures,
 * with the rh-allow-sha1-signatures disabled. */
static int
rh_allow_sha1_signatures_disabled(void)
{
	EVP_MD_CTX* ctx;
	EVP_PKEY* evp_key;
	/* This key is rdata from nlnetlabs.nl DNSKEY from 20250424005001,
	 * with id=50602 (ksk), size=2048b.
	 * A 2048 bit key is taken to avoid key too small errors. */
	unsigned char key[] = {
		0x03, 0x01, 0x00, 0x01, 0xBC, 0x0B, 0xE8, 0xBB,
		0x97, 0x4C, 0xB5, 0xED, 0x6F, 0x6D, 0xC2, 0xB1,
		0x78, 0x69, 0x93, 0x1C, 0x72, 0x19, 0xB1, 0x05,
		0x51, 0x13, 0xA1, 0xFC, 0xBF, 0x01, 0x58, 0x0D,
		0x44, 0x10, 0x5F, 0x0B, 0x75, 0x0E, 0x11, 0x9A,
		0xC8, 0xF8, 0x0F, 0x90, 0xFC, 0xB8, 0x09, 0xD1,
		0x14, 0x39, 0x0D, 0x84, 0xCE, 0x97, 0x88, 0x82,
		0x3D, 0xC5, 0xCB, 0x1A, 0xBF, 0x00, 0x46, 0x37,
		0x01, 0xF1, 0xCD, 0x46, 0xA2, 0x8F, 0x83, 0x19,
		0x42, 0xED, 0x6F, 0xAF, 0x37, 0x1F, 0x18, 0x82,
		0x4B, 0x70, 0x2D, 0x50, 0xA5, 0xA6, 0x66, 0x48,
		0x7F, 0x56, 0xA8, 0x86, 0x05, 0x41, 0xC8, 0xBE,
		0x4F, 0x8B, 0x38, 0x51, 0xF0, 0xEB, 0xAD, 0x2F,
		0x7A, 0xC0, 0xEF, 0xC7, 0xD2, 0x72, 0x6F, 0x16,
		0x66, 0xAF, 0x59, 0x55, 0xFF, 0xEE, 0x9D, 0x50,
		0xE9, 0xDB, 0xF4, 0x02, 0xBC, 0x33, 0x5C, 0xC5,
		0xDA, 0x1C, 0x6A, 0xD1, 0x55, 0xD1, 0x20, 0x2B,
		0x63, 0x03, 0x4B, 0x77, 0x45, 0x46, 0x78, 0x31,
		0xE4, 0x90, 0xB9, 0x7F, 0x00, 0xFB, 0x62, 0x7C,
		0x07, 0xD3, 0xC1, 0x00, 0xA0, 0x54, 0x63, 0x74,
		0x0A, 0x17, 0x7B, 0xE7, 0xAD, 0x38, 0x07, 0x86,
		0x68, 0xE4, 0xFD, 0x20, 0x68, 0xD5, 0x33, 0x92,
		0xCA, 0x90, 0xDD, 0xA4, 0xE9, 0xF2, 0x11, 0xBD,
		0x9D, 0xA5, 0xF5, 0xEB, 0xB9, 0xFE, 0x8F, 0xA1,
		0xE4, 0xBF, 0xA4, 0xA4, 0x34, 0x5C, 0x6A, 0x95,
		0xB6, 0x42, 0x22, 0xF6, 0xD6, 0x10, 0x9C, 0x9B,
		0x0A, 0x56, 0xE7, 0x42, 0xE5, 0x7F, 0x1F, 0x4E,
		0xBE, 0x4F, 0x8C, 0xED, 0x30, 0x63, 0xA7, 0x88,
		0x93, 0xED, 0x37, 0x3C, 0x80, 0xBC, 0xD1, 0x66,
		0xBD, 0xB8, 0x2E, 0x65, 0xC4, 0xC8, 0x00, 0x5B,
		0xE7, 0x85, 0x96, 0xDD, 0xAA, 0x05, 0xE6, 0x4F,
		0x03, 0x64, 0xFA, 0x2D, 0xF6, 0x88, 0x14, 0x8F,
		0x15, 0x4D, 0xFD, 0xD3
	};
	size_t keylen = 260;

#ifdef HAVE_EVP_MD_CTX_NEW
	ctx = EVP_MD_CTX_new();
#else
	ctx = (EVP_MD_CTX*)malloc(sizeof(*ctx));
	if(ctx) EVP_MD_CTX_init(ctx);
#endif
	if(!ctx) return 0;

	evp_key = sldns_key_rsa2pkey_raw(key, keylen);
	if(!evp_key) {
#ifdef HAVE_EVP_MD_CTX_NEW
		EVP_MD_CTX_destroy(ctx);
#else
		EVP_MD_CTX_cleanup(ctx);
		free(ctx);
#endif
		return 0;
	}

#ifndef HAVE_EVP_DIGESTVERIFY
	(void)evp_key; /* not used */
	if(EVP_DigestInit(ctx, EVP_sha1()) == 0)
#else
	if(EVP_DigestVerifyInit(ctx, NULL, EVP_sha1(), NULL, evp_key) == 0)
#endif
	{
		unsigned long e = ERR_get_error();
#ifdef EVP_R_INVALID_DIGEST
		if (ERR_GET_LIB(e) == ERR_LIB_EVP &&
			ERR_GET_REASON(e) == EVP_R_INVALID_DIGEST) {
			/* rh-allow-sha1-signatures makes use of sha1 invalid. */
			if(vsig)
				printf("Detected that rh-allow-sha1-signatures is off, and disables SHA1 signatures\n");
#ifdef HAVE_EVP_MD_CTX_NEW
			EVP_MD_CTX_destroy(ctx);
#else
			EVP_MD_CTX_cleanup(ctx);
			free(ctx);
#endif
			EVP_PKEY_free(evp_key);
			return 1;
		}
#endif /* EVP_R_INVALID_DIGEST */
		/* The signature verify failed for another reason. */
		log_crypto_err_code("EVP_DigestVerifyInit", e);
#ifdef HAVE_EVP_MD_CTX_NEW
		EVP_MD_CTX_destroy(ctx);
#else
		EVP_MD_CTX_cleanup(ctx);
		free(ctx);
#endif
		EVP_PKEY_free(evp_key);
		return 0;
	}
#ifdef HAVE_EVP_MD_CTX_NEW
	EVP_MD_CTX_destroy(ctx);
#else
	EVP_MD_CTX_cleanup(ctx);
	free(ctx);
#endif
	EVP_PKEY_free(evp_key);
	return 0;
}
#endif /* HAVE_SSL && USE_SHA1 */

void 
verify_test(void)
{
	unit_show_feature("signature verify");

#if defined(HAVE_SSL) && defined(USE_SHA1)
	if(rh_allow_sha1_signatures_disabled()) {
		/* Allow the use of SHA1 signatures for the test,
		 * in case that OpenSSL disallows use of RSASHA1
		 * with rh-allow-sha1-signatures disabled. */
#ifndef UB_ON_WINDOWS
		setenv("OPENSSL_ENABLE_SHA1_SIGNATURES", "1", 0);
#else
		_putenv("OPENSSL_ENABLE_SHA1_SIGNATURES=1");
#endif
	}
#endif

#ifdef USE_SHA1
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.1", "20070818005004");
#endif
#if defined(USE_DSA) && defined(USE_SHA1)
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.2", "20080414005004");
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.3", "20080416005004");
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.4", "20080416005004");
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.5", "20080416005004");
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.6", "20080416005004");
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.7", "20070829144150");
#endif /* USE_DSA */
#ifdef USE_SHA1
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.8", "20070829144150");
#endif
#if (defined(HAVE_EVP_SHA256) || defined(HAVE_NSS) || defined(HAVE_NETTLE)) && defined(USE_SHA2)
	verifytest_file(SRCDIRSTR "/testdata/test_sigs.rsasha256", "20070829144150");
#  ifdef USE_SHA1
	verifytest_file(SRCDIRSTR "/testdata/test_sigs.sha1_and_256", "20070829144150");
#  endif
	verifytest_file(SRCDIRSTR "/testdata/test_sigs.rsasha256_draft", "20090101000000");
#endif
#if (defined(HAVE_EVP_SHA512) || defined(HAVE_NSS) || defined(HAVE_NETTLE)) && defined(USE_SHA2)
	verifytest_file(SRCDIRSTR "/testdata/test_sigs.rsasha512_draft", "20070829144150");
	verifytest_file(SRCDIRSTR "/testdata/test_signatures.9", "20171215000000");
#endif
#ifdef USE_SHA1
	verifytest_file(SRCDIRSTR "/testdata/test_sigs.hinfo", "20090107100022");
	verifytest_file(SRCDIRSTR "/testdata/test_sigs.revoked", "20080414005004");
#endif
#ifdef USE_GOST
	if(sldns_key_EVP_load_gost_id())
	  verifytest_file(SRCDIRSTR "/testdata/test_sigs.gost", "20090807060504");
	else printf("Warning: skipped GOST, openssl does not provide gost.\n");
#endif
#ifdef USE_ECDSA
	/* test for support in case we use libNSS and ECC is removed */
	if(dnskey_algo_id_is_supported(LDNS_ECDSAP256SHA256)) {
		verifytest_file(SRCDIRSTR "/testdata/test_sigs.ecdsa_p256", "20100908100439");
		verifytest_file(SRCDIRSTR "/testdata/test_sigs.ecdsa_p384", "20100908100439");
	}
	dstest_file(SRCDIRSTR "/testdata/test_ds.sha384");
#endif
#ifdef USE_ED25519
	if(dnskey_algo_id_is_supported(LDNS_ED25519)) {
		verifytest_file(SRCDIRSTR "/testdata/test_sigs.ed25519", "20170530140439");
	}
#endif
#ifdef USE_ED448
	if(dnskey_algo_id_is_supported(LDNS_ED448)) {
		verifytest_file(SRCDIRSTR "/testdata/test_sigs.ed448", "20180408143630");
	}
#endif
#ifdef USE_SHA1
	dstest_file(SRCDIRSTR "/testdata/test_ds.sha1");
#endif
	nsectest();
	nsec3_hash_test(SRCDIRSTR "/testdata/test_nsec3_hash.1");
}
