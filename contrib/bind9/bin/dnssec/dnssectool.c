/*
 * Copyright (C) 2004, 2005, 2007, 2009-2015  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001, 2003  Internet Software Consortium.
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

/*%
 * DNSSEC Support Routines.
 */

#include <config.h>

#include <stdlib.h>

#include <isc/base32.h>
#include <isc/buffer.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/heap.h>
#include <isc/list.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/time.h>
#include <isc/util.h>
#include <isc/print.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/dnssec.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/nsec.h>
#include <dns/nsec3.h>
#include <dns/rdatastruct.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/secalg.h>
#include <dns/time.h>

#include "dnssectool.h"

static isc_heap_t *expected_chains, *found_chains;

struct nsec3_chain_fixed {
	isc_uint8_t	hash;
	isc_uint8_t	salt_length;
	isc_uint8_t	next_length;
	isc_uint16_t	iterations;
	/* unsigned char salt[0]; */
	/* unsigned char owner[0]; */
	/* unsigned char next[0]; */
};

extern int verbose;
extern const char *program;

typedef struct entropysource entropysource_t;

struct entropysource {
	isc_entropysource_t *source;
	isc_mem_t *mctx;
	ISC_LINK(entropysource_t) link;
};

static ISC_LIST(entropysource_t) sources;
static fatalcallback_t *fatalcallback = NULL;

void
fatal(const char *format, ...) {
	va_list args;

	fprintf(stderr, "%s: fatal: ", program);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	if (fatalcallback != NULL)
		(*fatalcallback)();
	exit(1);
}

void
setfatalcallback(fatalcallback_t *callback) {
	fatalcallback = callback;
}

void
check_result(isc_result_t result, const char *message) {
	if (result != ISC_R_SUCCESS)
		fatal("%s: %s", message, isc_result_totext(result));
}

void
vbprintf(int level, const char *fmt, ...) {
	va_list ap;
	if (level > verbose)
		return;
	va_start(ap, fmt);
	fprintf(stderr, "%s: ", program);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
version(const char *name) {
	fprintf(stderr, "%s %s\n", name, VERSION);
	exit(0);
}

void
type_format(const dns_rdatatype_t type, char *cp, unsigned int size) {
	isc_buffer_t b;
	isc_region_t r;
	isc_result_t result;

	isc_buffer_init(&b, cp, size - 1);
	result = dns_rdatatype_totext(type, &b);
	check_result(result, "dns_rdatatype_totext()");
	isc_buffer_usedregion(&b, &r);
	r.base[r.length] = 0;
}

void
sig_format(dns_rdata_rrsig_t *sig, char *cp, unsigned int size) {
	char namestr[DNS_NAME_FORMATSIZE];
	char algstr[DNS_NAME_FORMATSIZE];

	dns_name_format(&sig->signer, namestr, sizeof(namestr));
	dns_secalg_format(sig->algorithm, algstr, sizeof(algstr));
	snprintf(cp, size, "%s/%s/%d", namestr, algstr, sig->keyid);
}

void
setup_logging(isc_mem_t *mctx, isc_log_t **logp) {
	isc_result_t result;
	isc_logdestination_t destination;
	isc_logconfig_t *logconfig = NULL;
	isc_log_t *log = NULL;
	int level;

	if (verbose < 0)
		verbose = 0;
	switch (verbose) {
	case 0:
		/*
		 * We want to see warnings about things like out-of-zone
		 * data in the master file even when not verbose.
		 */
		level = ISC_LOG_WARNING;
		break;
	case 1:
		level = ISC_LOG_INFO;
		break;
	default:
		level = ISC_LOG_DEBUG(verbose - 2 + 1);
		break;
	}

	RUNTIME_CHECK(isc_log_create(mctx, &log, &logconfig) == ISC_R_SUCCESS);
	isc_log_setcontext(log);
	dns_log_init(log);
	dns_log_setcontext(log);

	RUNTIME_CHECK(isc_log_settag(logconfig, program) == ISC_R_SUCCESS);

	/*
	 * Set up a channel similar to default_stderr except:
	 *  - the logging level is passed in
	 *  - the program name and logging level are printed
	 *  - no time stamp is printed
	 */
	destination.file.stream = stderr;
	destination.file.name = NULL;
	destination.file.versions = ISC_LOG_ROLLNEVER;
	destination.file.maximum_size = 0;
	result = isc_log_createchannel(logconfig, "stderr",
				       ISC_LOG_TOFILEDESC,
				       level,
				       &destination,
				       ISC_LOG_PRINTTAG|ISC_LOG_PRINTLEVEL);
	check_result(result, "isc_log_createchannel()");

	RUNTIME_CHECK(isc_log_usechannel(logconfig, "stderr",
					 NULL, NULL) == ISC_R_SUCCESS);

	*logp = log;
}

void
cleanup_logging(isc_log_t **logp) {
	isc_log_t *log;

	REQUIRE(logp != NULL);

	log = *logp;
	if (log == NULL)
		return;
	isc_log_destroy(&log);
	isc_log_setcontext(NULL);
	dns_log_setcontext(NULL);
	logp = NULL;
}

void
setup_entropy(isc_mem_t *mctx, const char *randomfile, isc_entropy_t **ectx) {
	isc_result_t result;
	isc_entropysource_t *source = NULL;
	entropysource_t *elt;
	int usekeyboard = ISC_ENTROPY_KEYBOARDMAYBE;

	REQUIRE(ectx != NULL);

	if (*ectx == NULL) {
		result = isc_entropy_create(mctx, ectx);
		if (result != ISC_R_SUCCESS)
			fatal("could not create entropy object");
		ISC_LIST_INIT(sources);
	}

	if (randomfile != NULL && strcmp(randomfile, "keyboard") == 0) {
		usekeyboard = ISC_ENTROPY_KEYBOARDYES;
		randomfile = NULL;
	}

	result = isc_entropy_usebestsource(*ectx, &source, randomfile,
					   usekeyboard);

	if (result != ISC_R_SUCCESS)
		fatal("could not initialize entropy source: %s",
		      isc_result_totext(result));

	if (source != NULL) {
		elt = isc_mem_get(mctx, sizeof(*elt));
		if (elt == NULL)
			fatal("out of memory");
		elt->source = source;
		elt->mctx = mctx;
		ISC_LINK_INIT(elt, link);
		ISC_LIST_APPEND(sources, elt, link);
	}
}

void
cleanup_entropy(isc_entropy_t **ectx) {
	entropysource_t *source;
	while (!ISC_LIST_EMPTY(sources)) {
		source = ISC_LIST_HEAD(sources);
		ISC_LIST_UNLINK(sources, source, link);
		isc_entropy_destroysource(&source->source);
		isc_mem_put(source->mctx, source, sizeof(*source));
	}
	isc_entropy_detach(ectx);
}

static isc_stdtime_t
time_units(isc_stdtime_t offset, char *suffix, const char *str) {
	switch (suffix[0]) {
	    case 'Y': case 'y':
		return (offset * (365 * 24 * 3600));
	    case 'M': case 'm':
		switch (suffix[1]) {
		    case 'O': case 'o':
			return (offset * (30 * 24 * 3600));
		    case 'I': case 'i':
			return (offset * 60);
		    case '\0':
			fatal("'%s' ambiguous: use 'mi' for minutes "
			      "or 'mo' for months", str);
		    default:
			fatal("time value %s is invalid", str);
		}
		/* NOTREACHED */
		break;
	    case 'W': case 'w':
		return (offset * (7 * 24 * 3600));
	    case 'D': case 'd':
		return (offset * (24 * 3600));
	    case 'H': case 'h':
		return (offset * 3600);
	    case 'S': case 's': case '\0':
		return (offset);
	    default:
		fatal("time value %s is invalid", str);
	}
	/* NOTREACHED */
	return(0); /* silence compiler warning */
}

static inline isc_boolean_t
isnone(const char *str) {
	return (ISC_TF((strcasecmp(str, "none") == 0) ||
		       (strcasecmp(str, "never") == 0)));
}

dns_ttl_t
strtottl(const char *str) {
	const char *orig = str;
	dns_ttl_t ttl;
	char *endp;

	if (isnone(str))
		return ((dns_ttl_t) 0);

	ttl = strtol(str, &endp, 0);
	if (ttl == 0 && endp == str)
		fatal("TTL must be numeric");
	ttl = time_units(ttl, endp, orig);
	return (ttl);
}

isc_stdtime_t
strtotime(const char *str, isc_int64_t now, isc_int64_t base,
	  isc_boolean_t *setp)
{
	isc_int64_t val, offset;
	isc_result_t result;
	const char *orig = str;
	char *endp;
	size_t n;

	if (isnone(str)) {
		if (setp != NULL)
			*setp = ISC_FALSE;
		return ((isc_stdtime_t) 0);
	}

	if (setp != NULL)
		*setp = ISC_TRUE;

	if ((str[0] == '0' || str[0] == '-') && str[1] == '\0')
		return ((isc_stdtime_t) 0);

	/*
	 * We accept times in the following formats:
	 *   now([+-]offset)
	 *   YYYYMMDD([+-]offset)
	 *   YYYYMMDDhhmmss([+-]offset)
	 *   [+-]offset
	 */
	n = strspn(str, "0123456789");
	if ((n == 8u || n == 14u) &&
	    (str[n] == '\0' || str[n] == '-' || str[n] == '+'))
	{
		char timestr[15];

		strlcpy(timestr, str, sizeof(timestr));
		timestr[n] = 0;
		if (n == 8u)
			strlcat(timestr, "000000", sizeof(timestr));
		result = dns_time64_fromtext(timestr, &val);
		if (result != ISC_R_SUCCESS)
			fatal("time value %s is invalid: %s", orig,
			      isc_result_totext(result));
		base = val;
		str += n;
	} else if (strncmp(str, "now", 3) == 0) {
		base = now;
		str += 3;
	}

	if (str[0] == '\0')
		return ((isc_stdtime_t) base);
	else if (str[0] == '+') {
		offset = strtol(str + 1, &endp, 0);
		offset = time_units((isc_stdtime_t) offset, endp, orig);
		val = base + offset;
	} else if (str[0] == '-') {
		offset = strtol(str + 1, &endp, 0);
		offset = time_units((isc_stdtime_t) offset, endp, orig);
		val = base - offset;
	} else
		fatal("time value %s is invalid", orig);

	return ((isc_stdtime_t) val);
}

dns_rdataclass_t
strtoclass(const char *str) {
	isc_textregion_t r;
	dns_rdataclass_t rdclass;
	isc_result_t ret;

	if (str == NULL)
		return dns_rdataclass_in;
	DE_CONST(str, r.base);
	r.length = strlen(str);
	ret = dns_rdataclass_fromtext(&rdclass, &r);
	if (ret != ISC_R_SUCCESS)
		fatal("unknown class %s", str);
	return (rdclass);
}

isc_result_t
try_dir(const char *dirname) {
	isc_result_t result;
	isc_dir_t d;

	isc_dir_init(&d);
	result = isc_dir_open(&d, dirname);
	if (result == ISC_R_SUCCESS) {
		isc_dir_close(&d);
	}
	return (result);
}

/*
 * Check private key version compatibility.
 */
void
check_keyversion(dst_key_t *key, char *keystr) {
	int major, minor;
	dst_key_getprivateformat(key, &major, &minor);
	INSIST(major <= DST_MAJOR_VERSION); /* invalid private key */

	if (major < DST_MAJOR_VERSION || minor < DST_MINOR_VERSION)
		fatal("Key %s has incompatible format version %d.%d, "
		      "use -f to force upgrade to new version.",
		      keystr, major, minor);
	if (minor > DST_MINOR_VERSION)
		fatal("Key %s has incompatible format version %d.%d, "
		      "use -f to force downgrade to current version.",
		      keystr, major, minor);
}

void
set_keyversion(dst_key_t *key) {
	int major, minor;
	dst_key_getprivateformat(key, &major, &minor);
	INSIST(major <= DST_MAJOR_VERSION);

	if (major != DST_MAJOR_VERSION || minor != DST_MINOR_VERSION)
		dst_key_setprivateformat(key, DST_MAJOR_VERSION,
					 DST_MINOR_VERSION);

	/*
	 * If the key is from a version older than 1.3, set
	 * set the creation date
	 */
	if (major < 1 || (major == 1 && minor <= 2)) {
		isc_stdtime_t now;
		isc_stdtime_get(&now);
		dst_key_settime(key, DST_TIME_CREATED, now);
	}
}

isc_boolean_t
key_collision(dst_key_t *dstkey, dns_name_t *name, const char *dir,
	      isc_mem_t *mctx, isc_boolean_t *exact)
{
	isc_result_t result;
	isc_boolean_t conflict = ISC_FALSE;
	dns_dnsseckeylist_t matchkeys;
	dns_dnsseckey_t *key = NULL;
	isc_uint16_t id, oldid;
	isc_uint32_t rid, roldid;
	dns_secalg_t alg;

	if (exact != NULL)
		*exact = ISC_FALSE;

	id = dst_key_id(dstkey);
	rid = dst_key_rid(dstkey);
	alg = dst_key_alg(dstkey);

	ISC_LIST_INIT(matchkeys);
	result = dns_dnssec_findmatchingkeys(name, dir, mctx, &matchkeys);
	if (result == ISC_R_NOTFOUND)
		return (ISC_FALSE);

	while (!ISC_LIST_EMPTY(matchkeys) && !conflict) {
		key = ISC_LIST_HEAD(matchkeys);
		if (dst_key_alg(key->key) != alg)
			goto next;

		oldid = dst_key_id(key->key);
		roldid = dst_key_rid(key->key);

		if (oldid == rid || roldid == id || id == oldid) {
			conflict = ISC_TRUE;
			if (id != oldid) {
				if (verbose > 1)
					fprintf(stderr, "Key ID %d could "
						"collide with %d\n",
						id, oldid);
			} else {
				if (exact != NULL)
					*exact = ISC_TRUE;
				if (verbose > 1)
					fprintf(stderr, "Key ID %d exists\n",
						id);
			}
		}

 next:
		ISC_LIST_UNLINK(matchkeys, key, link);
		dns_dnsseckey_destroy(mctx, &key);
	}

	/* Finish freeing the list */
	while (!ISC_LIST_EMPTY(matchkeys)) {
		key = ISC_LIST_HEAD(matchkeys);
		ISC_LIST_UNLINK(matchkeys, key, link);
		dns_dnsseckey_destroy(mctx, &key);
	}

	return (conflict);
}

isc_boolean_t
is_delegation(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *origin,
	      dns_name_t *name, dns_dbnode_t *node, isc_uint32_t *ttlp)
{
	dns_rdataset_t nsset;
	isc_result_t result;

	if (dns_name_equal(name, origin))
		return (ISC_FALSE);

	dns_rdataset_init(&nsset);
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_ns,
				     0, 0, &nsset, NULL);
	if (dns_rdataset_isassociated(&nsset)) {
		if (ttlp != NULL)
			*ttlp = nsset.ttl;
		dns_rdataset_disassociate(&nsset);
	}

	return (ISC_TF(result == ISC_R_SUCCESS));
}

static isc_boolean_t
goodsig(dns_name_t *origin, dns_rdata_t *sigrdata, dns_name_t *name,
	dns_rdataset_t *keyrdataset, dns_rdataset_t *rdataset, isc_mem_t *mctx)
{
	dns_rdata_dnskey_t key;
	dns_rdata_rrsig_t sig;
	dst_key_t *dstkey = NULL;
	isc_result_t result;

	result = dns_rdata_tostruct(sigrdata, &sig, NULL);
	check_result(result, "dns_rdata_tostruct()");

	for (result = dns_rdataset_first(keyrdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(keyrdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdataset_current(keyrdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &key, NULL);
		check_result(result, "dns_rdata_tostruct()");
		result = dns_dnssec_keyfromrdata(origin, &rdata, mctx,
						 &dstkey);
		if (result != ISC_R_SUCCESS)
			return (ISC_FALSE);
		if (sig.algorithm != key.algorithm ||
		    sig.keyid != dst_key_id(dstkey) ||
		    !dns_name_equal(&sig.signer, origin)) {
			dst_key_free(&dstkey);
			continue;
		}
		result = dns_dnssec_verify(name, rdataset, dstkey, ISC_FALSE,
					   mctx, sigrdata);
		dst_key_free(&dstkey);
		if (result == ISC_R_SUCCESS)
			return(ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_result_t
verifynsec(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	   dns_dbnode_t *node, dns_name_t *nextname)
{
	unsigned char buffer[DNS_NSEC_BUFFERSIZE];
	char namebuf[DNS_NAME_FORMATSIZE];
	char nextbuf[DNS_NAME_FORMATSIZE];
	char found[DNS_NAME_FORMATSIZE];
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_t tmprdata = DNS_RDATA_INIT;
	dns_rdata_nsec_t nsec;
	isc_result_t result;

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec,
				     0, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		fprintf(stderr, "Missing NSEC record for %s\n", namebuf);
		goto failure;
	}

	result = dns_rdataset_first(&rdataset);
	check_result(result, "dns_rdataset_first()");

	dns_rdataset_current(&rdataset, &rdata);
	result = dns_rdata_tostruct(&rdata, &nsec, NULL);
	check_result(result, "dns_rdata_tostruct()");
	/* Check bit next name is consistent */
	if (!dns_name_equal(&nsec.next, nextname)) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		dns_name_format(nextname, nextbuf, sizeof(nextbuf));
		dns_name_format(&nsec.next, found, sizeof(found));
		fprintf(stderr, "Bad NSEC record for %s, next name "
				"mismatch (expected:%s, found:%s)\n", namebuf,
				nextbuf, found);
		goto failure;
	}
	/* Check bit map is consistent */
	result = dns_nsec_buildrdata(db, ver, node, nextname, buffer,
				     &tmprdata);
	check_result(result, "dns_nsec_buildrdata()");
	if (dns_rdata_compare(&rdata, &tmprdata) != 0) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		fprintf(stderr, "Bad NSEC record for %s, bit map "
				"mismatch\n", namebuf);
		goto failure;
	}
	result = dns_rdataset_next(&rdataset);
	if (result != ISC_R_NOMORE) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		fprintf(stderr, "Multipe NSEC records for %s\n", namebuf);
		goto failure;

	}
	dns_rdataset_disassociate(&rdataset);
	return (ISC_R_SUCCESS);
 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	return (ISC_R_FAILURE);
}

static void
check_no_rrsig(dns_db_t *db, dns_dbversion_t *ver, dns_rdataset_t *rdataset,
	       dns_name_t *name, dns_dbnode_t *node)
{
	char namebuf[DNS_NAME_FORMATSIZE];
	char typebuf[80];
	dns_rdataset_t sigrdataset;
	dns_rdatasetiter_t *rdsiter = NULL;
	isc_result_t result;

	dns_rdataset_init(&sigrdataset);
	result = dns_db_allrdatasets(db, node, ver, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	for (result = dns_rdatasetiter_first(rdsiter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(rdsiter)) {
		dns_rdatasetiter_current(rdsiter, &sigrdataset);
		if (sigrdataset.type == dns_rdatatype_rrsig &&
		    sigrdataset.covers == rdataset->type)
			break;
		dns_rdataset_disassociate(&sigrdataset);
	}
	if (result == ISC_R_SUCCESS) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		type_format(rdataset->type, typebuf, sizeof(typebuf));
		fprintf(stderr, "Warning: Found unexpected signatures for "
			"%s/%s\n", namebuf, typebuf);
	}
	if (dns_rdataset_isassociated(&sigrdataset))
		dns_rdataset_disassociate(&sigrdataset);
	dns_rdatasetiter_destroy(&rdsiter);
}

static isc_boolean_t
chain_compare(void *arg1, void *arg2) {
	struct nsec3_chain_fixed *e1 = arg1, *e2 = arg2;
	size_t len;

	/*
	 * Do each element in turn to get a stable sort.
	 */
	if (e1->hash < e2->hash)
		return (ISC_TRUE);
	if (e1->hash > e2->hash)
		return (ISC_FALSE);
	if (e1->iterations < e2->iterations)
		return (ISC_TRUE);
	if (e1->iterations > e2->iterations)
		return (ISC_FALSE);
	if (e1->salt_length < e2->salt_length)
		return (ISC_TRUE);
	if (e1->salt_length > e2->salt_length)
		return (ISC_FALSE);
	if (e1->next_length < e2->next_length)
		return (ISC_TRUE);
	if (e1->next_length > e2->next_length)
		return (ISC_FALSE);
	len = e1->salt_length + 2 * e1->next_length;
	if (memcmp(e1 + 1, e2 + 1, len) < 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static isc_boolean_t
chain_equal(struct nsec3_chain_fixed *e1, struct nsec3_chain_fixed *e2) {
	size_t len;

	if (e1->hash != e2->hash)
		return (ISC_FALSE);
	if (e1->iterations != e2->iterations)
		return (ISC_FALSE);
	if (e1->salt_length != e2->salt_length)
		return (ISC_FALSE);
	if (e1->next_length != e2->next_length)
		return (ISC_FALSE);
	len = e1->salt_length + 2 * e1->next_length;
	if (memcmp(e1 + 1, e2 + 1, len) != 0)
		return (ISC_FALSE);
	return (ISC_TRUE);
}

static isc_result_t
record_nsec3(const unsigned char *rawhash, const dns_rdata_nsec3_t *nsec3,
	     isc_mem_t *mctx, isc_heap_t *chains)
{
	struct nsec3_chain_fixed *element;
	size_t len;
	unsigned char *cp;
	isc_result_t result;

	len = sizeof(*element) + nsec3->next_length * 2 + nsec3->salt_length;

	element = isc_mem_get(mctx, len);
	if (element == NULL)
		return (ISC_R_NOMEMORY);
	memset(element, 0, len);
	element->hash = nsec3->hash;
	element->salt_length = nsec3->salt_length;
	element->next_length = nsec3->next_length;
	element->iterations = nsec3->iterations;
	cp = (unsigned char *)(element + 1);
	memmove(cp, nsec3->salt, nsec3->salt_length);
	cp += nsec3->salt_length;
	memmove(cp, rawhash, nsec3->next_length);
	cp += nsec3->next_length;
	memmove(cp, nsec3->next, nsec3->next_length);
	result = isc_heap_insert(chains, element);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "isc_heap_insert failed: %s\n",
			isc_result_totext(result));
		isc_mem_put(mctx, element, len);
	}
	return (result);
}

static isc_result_t
match_nsec3(dns_name_t *name, isc_mem_t *mctx,
	    dns_rdata_nsec3param_t *nsec3param, dns_rdataset_t *rdataset,
	    unsigned char types[8192], unsigned int maxtype,
	    unsigned char *rawhash, size_t rhsize)
{
	unsigned char cbm[8244];
	char namebuf[DNS_NAME_FORMATSIZE];
	dns_rdata_nsec3_t nsec3;
	isc_result_t result;
	unsigned int len;

	/*
	 * Find matching NSEC3 record.
	 */
	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdataset_current(rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &nsec3, NULL);
		check_result(result, "dns_rdata_tostruct()");
		if (nsec3.hash == nsec3param->hash &&
		    nsec3.next_length == rhsize &&
		    nsec3.iterations == nsec3param->iterations &&
		    nsec3.salt_length == nsec3param->salt_length &&
		    memcmp(nsec3.salt, nsec3param->salt,
			   nsec3param->salt_length) == 0)
			break;
	}
	if (result != ISC_R_SUCCESS) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		fprintf(stderr, "Missing NSEC3 record for %s\n", namebuf);
		return (result);
	}

	/*
	 * Check the type list.
	 */
	len = dns_nsec_compressbitmap(cbm, types, maxtype);
	if (nsec3.len != len || memcmp(cbm, nsec3.typebits, len) != 0) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		fprintf(stderr, "Bad NSEC3 record for %s, bit map "
				"mismatch\n", namebuf);
		return (ISC_R_FAILURE);
	}

	/*
	 * Record chain.
	 */
	result = record_nsec3(rawhash, &nsec3, mctx, expected_chains);
	check_result(result, "record_nsec3()");

	/*
	 * Make sure there is only one NSEC3 record with this set of
	 * parameters.
	 */
	for (result = dns_rdataset_next(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdataset_current(rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &nsec3, NULL);
		check_result(result, "dns_rdata_tostruct()");
		if (nsec3.hash == nsec3param->hash &&
		    nsec3.iterations == nsec3param->iterations &&
		    nsec3.salt_length == nsec3param->salt_length &&
		    memcmp(nsec3.salt, nsec3param->salt,
			   nsec3.salt_length) == 0) {
			dns_name_format(name, namebuf, sizeof(namebuf));
			fprintf(stderr, "Multiple NSEC3 records with the "
				"same parameter set for %s", namebuf);
			result = DNS_R_DUPLICATE;
			break;
		}
	}
	if (result != ISC_R_NOMORE)
		return (result);

	result = ISC_R_SUCCESS;
	return (result);
}

static isc_boolean_t
innsec3params(dns_rdata_nsec3_t *nsec3, dns_rdataset_t *nsec3paramset) {
	dns_rdata_nsec3param_t nsec3param;
	isc_result_t result;

	for (result = dns_rdataset_first(nsec3paramset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(nsec3paramset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(nsec3paramset, &rdata);
		result = dns_rdata_tostruct(&rdata, &nsec3param, NULL);
		check_result(result, "dns_rdata_tostruct()");
		if (nsec3param.flags == 0 &&
		    nsec3param.hash == nsec3->hash &&
		    nsec3param.iterations == nsec3->iterations &&
		    nsec3param.salt_length == nsec3->salt_length &&
		    memcmp(nsec3param.salt, nsec3->salt,
			   nsec3->salt_length) == 0)
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_result_t
record_found(dns_db_t *db, dns_dbversion_t *ver, isc_mem_t *mctx,
	     dns_name_t *name, dns_dbnode_t *node,
	     dns_rdataset_t *nsec3paramset)
{
	unsigned char owner[NSEC3_MAX_HASH_LENGTH];
	dns_rdata_nsec3_t nsec3;
	dns_rdataset_t rdataset;
	dns_label_t hashlabel;
	isc_buffer_t b;
	isc_result_t result;

	if (nsec3paramset == NULL || !dns_rdataset_isassociated(nsec3paramset))
		return (ISC_R_SUCCESS);

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3,
				     0, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_SUCCESS);

	dns_name_getlabel(name, 0, &hashlabel);
	isc_region_consume(&hashlabel, 1);
	isc_buffer_init(&b, owner, sizeof(owner));
	result = isc_base32hex_decoderegion(&hashlabel, &b);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &nsec3, NULL);
		check_result(result, "dns_rdata_tostruct()");
		if (nsec3.next_length != isc_buffer_usedlength(&b))
			continue;
		/*
		 * We only care about NSEC3 records that match a NSEC3PARAM
		 * record.
		 */
		if (!innsec3params(&nsec3, nsec3paramset))
			continue;

		/*
		 * Record chain.
		 */
		result = record_nsec3(owner, &nsec3, mctx, found_chains);
		check_result(result, "record_nsec3()");
	}

 cleanup:
	dns_rdataset_disassociate(&rdataset);
	return (ISC_R_SUCCESS);
}

static isc_boolean_t
isoptout(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *origin,
	 dns_rdata_t *nsec3rdata)
{
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_nsec3_t nsec3;
	dns_rdata_nsec3param_t nsec3param;
	dns_fixedname_t fixed;
	dns_name_t *hashname;
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	unsigned char rawhash[NSEC3_MAX_HASH_LENGTH];
	size_t rhsize = sizeof(rawhash);
	isc_boolean_t ret;

	result = dns_rdata_tostruct(nsec3rdata, &nsec3param, NULL);
	check_result(result, "dns_rdata_tostruct()");

	dns_fixedname_init(&fixed);
	result = dns_nsec3_hashname(&fixed, rawhash, &rhsize, origin, origin,
				    nsec3param.hash, nsec3param.iterations,
				    nsec3param.salt, nsec3param.salt_length);
	check_result(result, "dns_nsec3_hashname()");

	dns_rdataset_init(&rdataset);
	hashname = dns_fixedname_name(&fixed);
	result = dns_db_findnsec3node(db, hashname, ISC_FALSE, &node);
	if (result == ISC_R_SUCCESS)
		result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3,
					     0, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS)
		return (ISC_FALSE);

	result = dns_rdataset_first(&rdataset);
	check_result(result, "dns_rdataset_first()");

	dns_rdataset_current(&rdataset, &rdata);

	result = dns_rdata_tostruct(&rdata, &nsec3, NULL);
	if (result != ISC_R_SUCCESS)
		ret = ISC_FALSE;
	else
		ret = ISC_TF((nsec3.flags & DNS_NSEC3FLAG_OPTOUT) != 0);

	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	return (ret);
}

static isc_result_t
verifynsec3(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *origin,
	    isc_mem_t *mctx, dns_name_t *name, dns_rdata_t *rdata,
	    isc_boolean_t delegation, isc_boolean_t empty,
	    unsigned char types[8192], unsigned int maxtype)
{
	char namebuf[DNS_NAME_FORMATSIZE];
	char hashbuf[DNS_NAME_FORMATSIZE];
	dns_rdataset_t rdataset;
	dns_rdata_nsec3param_t nsec3param;
	dns_fixedname_t fixed;
	dns_name_t *hashname;
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	unsigned char rawhash[NSEC3_MAX_HASH_LENGTH];
	size_t rhsize = sizeof(rawhash);
	isc_boolean_t optout;

	result = dns_rdata_tostruct(rdata, &nsec3param, NULL);
	check_result(result, "dns_rdata_tostruct()");

	if (nsec3param.flags != 0)
		return (ISC_R_SUCCESS);

	if (!dns_nsec3_supportedhash(nsec3param.hash))
		return (ISC_R_SUCCESS);

	optout = isoptout(db, ver, origin, rdata);

	dns_fixedname_init(&fixed);
	result = dns_nsec3_hashname(&fixed, rawhash, &rhsize, name, origin,
				    nsec3param.hash, nsec3param.iterations,
				    nsec3param.salt, nsec3param.salt_length);
	check_result(result, "dns_nsec3_hashname()");

	/*
	 * We don't use dns_db_find() here as it works with the choosen
	 * nsec3 chain and we may also be called with uncommitted data
	 * from dnssec-signzone so the secure status of the zone may not
	 * be up to date.
	 */
	dns_rdataset_init(&rdataset);
	hashname = dns_fixedname_name(&fixed);
	result = dns_db_findnsec3node(db, hashname, ISC_FALSE, &node);
	if (result == ISC_R_SUCCESS)
		result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3,
					     0, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS &&
	    (!delegation || (empty && !optout) ||
	     (!empty && dns_nsec_isset(types, dns_rdatatype_ds))))
	{
		dns_name_format(name, namebuf, sizeof(namebuf));
		dns_name_format(hashname, hashbuf, sizeof(hashbuf));
		fprintf(stderr, "Missing NSEC3 record for %s (%s)\n",
			namebuf, hashbuf);
	} else if (result == ISC_R_NOTFOUND &&
		   delegation && (!empty || optout))
	{
		result = ISC_R_SUCCESS;
	} else if (result == ISC_R_SUCCESS) {
		result = match_nsec3(name, mctx, &nsec3param, &rdataset,
				     types, maxtype, rawhash, rhsize);
	}

	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	return (result);
}

static isc_result_t
verifynsec3s(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *origin,
	     isc_mem_t *mctx, dns_name_t *name, dns_rdataset_t *nsec3paramset,
	     isc_boolean_t delegation, isc_boolean_t empty,
	     unsigned char types[8192], unsigned int maxtype)
{
	isc_result_t result;

	for (result = dns_rdataset_first(nsec3paramset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(nsec3paramset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(nsec3paramset, &rdata);
		result = verifynsec3(db, ver, origin, mctx, name, &rdata,
				     delegation, empty, types, maxtype);
		if (result != ISC_R_SUCCESS)
			break;
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
	return (result);
}

static void
verifyset(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *origin,
	  isc_mem_t *mctx, dns_rdataset_t *rdataset, dns_name_t *name,
	  dns_dbnode_t *node, dns_rdataset_t *keyrdataset,
	  unsigned char *act_algorithms, unsigned char *bad_algorithms)
{
	unsigned char set_algorithms[256];
	char namebuf[DNS_NAME_FORMATSIZE];
	char algbuf[80];
	char typebuf[80];
	dns_rdataset_t sigrdataset;
	dns_rdatasetiter_t *rdsiter = NULL;
	isc_result_t result;
	int i;

	dns_rdataset_init(&sigrdataset);
	result = dns_db_allrdatasets(db, node, ver, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	for (result = dns_rdatasetiter_first(rdsiter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(rdsiter)) {
		dns_rdatasetiter_current(rdsiter, &sigrdataset);
		if (sigrdataset.type == dns_rdatatype_rrsig &&
		    sigrdataset.covers == rdataset->type)
			break;
		dns_rdataset_disassociate(&sigrdataset);
	}
	if (result != ISC_R_SUCCESS) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		type_format(rdataset->type, typebuf, sizeof(typebuf));
		fprintf(stderr, "No signatures for %s/%s\n", namebuf, typebuf);
		for (i = 0; i < 256; i++)
			if (act_algorithms[i] != 0)
				bad_algorithms[i] = 1;
		dns_rdatasetiter_destroy(&rdsiter);
		return;
	}

	memset(set_algorithms, 0, sizeof(set_algorithms));
	for (result = dns_rdataset_first(&sigrdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&sigrdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdata_rrsig_t sig;

		dns_rdataset_current(&sigrdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &sig, NULL);
		check_result(result, "dns_rdata_tostruct()");
		if (rdataset->ttl != sig.originalttl) {
			dns_name_format(name, namebuf, sizeof(namebuf));
			type_format(rdataset->type, typebuf, sizeof(typebuf));
			fprintf(stderr, "TTL mismatch for %s %s keytag %u\n",
				namebuf, typebuf, sig.keyid);
			continue;
		}
		if ((set_algorithms[sig.algorithm] != 0) ||
		    (act_algorithms[sig.algorithm] == 0))
			continue;
		if (goodsig(origin, &rdata, name, keyrdataset, rdataset, mctx))
			set_algorithms[sig.algorithm] = 1;
	}
	dns_rdatasetiter_destroy(&rdsiter);
	if (memcmp(set_algorithms, act_algorithms, sizeof(set_algorithms))) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		type_format(rdataset->type, typebuf, sizeof(typebuf));
		for (i = 0; i < 256; i++)
			if ((act_algorithms[i] != 0) &&
			    (set_algorithms[i] == 0)) {
				dns_secalg_format(i, algbuf, sizeof(algbuf));
				fprintf(stderr, "No correct %s signature for "
					"%s %s\n", algbuf, namebuf, typebuf);
				bad_algorithms[i] = 1;
			}
	}
	dns_rdataset_disassociate(&sigrdataset);
}

static isc_result_t
verifynode(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *origin,
	   isc_mem_t *mctx, dns_name_t *name, dns_dbnode_t *node,
	   isc_boolean_t delegation, dns_rdataset_t *keyrdataset,
	   unsigned char *act_algorithms, unsigned char *bad_algorithms,
	   dns_rdataset_t *nsecset, dns_rdataset_t *nsec3paramset,
	   dns_name_t *nextname)
{
	unsigned char types[8192];
	unsigned int maxtype = 0;
	dns_rdataset_t rdataset; dns_rdatasetiter_t *rdsiter = NULL;
	isc_result_t result, tresult;

	memset(types, 0, sizeof(types));
	result = dns_db_allrdatasets(db, node, ver, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	result = dns_rdatasetiter_first(rdsiter);
	dns_rdataset_init(&rdataset);
	while (result == ISC_R_SUCCESS) {
		dns_rdatasetiter_current(rdsiter, &rdataset);
		/*
		 * If we are not at a delegation then everything should be
		 * signed.  If we are at a delegation then only the DS set
		 * is signed.  The NS set is not signed at a delegation but
		 * its existance is recorded in the bit map.  Anything else
		 * other than NSEC and DS is not signed at a delegation.
		 */
		if (rdataset.type != dns_rdatatype_rrsig &&
		    rdataset.type != dns_rdatatype_dnskey &&
		    (!delegation || rdataset.type == dns_rdatatype_ds ||
		     rdataset.type == dns_rdatatype_nsec)) {
			verifyset(db, ver, origin, mctx, &rdataset,
				  name, node, keyrdataset,
				  act_algorithms, bad_algorithms);
			dns_nsec_setbit(types, rdataset.type, 1);
			if (rdataset.type > maxtype)
				maxtype = rdataset.type;
		} else if (rdataset.type != dns_rdatatype_rrsig &&
			   rdataset.type != dns_rdatatype_dnskey) {
			if (rdataset.type == dns_rdatatype_ns)
				dns_nsec_setbit(types, rdataset.type, 1);
			check_no_rrsig(db, ver, &rdataset, name, node);
		} else
			dns_nsec_setbit(types, rdataset.type, 1);
		dns_rdataset_disassociate(&rdataset);
		result = dns_rdatasetiter_next(rdsiter);
	}
	if (result != ISC_R_NOMORE)
		fatal("rdataset iteration failed: %s",
		      isc_result_totext(result));
	dns_rdatasetiter_destroy(&rdsiter);

	result = ISC_R_SUCCESS;

	if (nsecset != NULL && dns_rdataset_isassociated(nsecset))
		result = verifynsec(db, ver, name, node, nextname);

	if (nsec3paramset != NULL && dns_rdataset_isassociated(nsec3paramset)) {
		tresult = verifynsec3s(db, ver, origin, mctx, name,
				       nsec3paramset, delegation, ISC_FALSE,
				       types, maxtype);
		if (result == ISC_R_SUCCESS && tresult != ISC_R_SUCCESS)
			result = tresult;
	}
	return (result);
}

static isc_boolean_t
is_empty(dns_db_t *db, dns_dbversion_t *ver, dns_dbnode_t *node) {
	dns_rdatasetiter_t *rdsiter = NULL;
	isc_result_t result;

	result = dns_db_allrdatasets(db, node, ver, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	result = dns_rdatasetiter_first(rdsiter);
	dns_rdatasetiter_destroy(&rdsiter);
	if (result == ISC_R_NOMORE)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static void
check_no_nsec(dns_name_t *name, dns_dbnode_t *node, dns_db_t *db,
	      dns_dbversion_t *ver)
{
	dns_rdataset_t rdataset;
	isc_result_t result;

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec,
				     0, 0, &rdataset, NULL);
	if (result != ISC_R_NOTFOUND) {
		char namebuf[DNS_NAME_FORMATSIZE];
		dns_name_format(name, namebuf, sizeof(namebuf));
		fatal("unexpected NSEC RRset at %s\n", namebuf);
	}

	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
}

static isc_boolean_t
newchain(const struct nsec3_chain_fixed *first,
	 const struct nsec3_chain_fixed *e)
{
	if (first->hash != e->hash ||
	    first->iterations != e->iterations ||
	    first->salt_length != e->salt_length ||
	    first->next_length != e->next_length ||
	    memcmp(first + 1, e + 1, first->salt_length) != 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static void
free_element(isc_mem_t *mctx, struct nsec3_chain_fixed *e) {
	size_t len;

	len = sizeof(*e) + e->salt_length + 2 * e->next_length;
	isc_mem_put(mctx, e, len);
}

static isc_boolean_t
checknext(const struct nsec3_chain_fixed *first,
	  const struct nsec3_chain_fixed *e)
{
	char buf[512];
	const unsigned char *d1 = (const unsigned char *)(first + 1);
	const unsigned char *d2 = (const unsigned char *)(e + 1);
	isc_buffer_t b;
	isc_region_t sr;

	d1 += first->salt_length + first->next_length;
	d2 += e->salt_length;

	if (memcmp(d1, d2, first->next_length) == 0)
		return (ISC_TRUE);

	DE_CONST(d1 - first->next_length, sr.base);
	sr.length = first->next_length;
	isc_buffer_init(&b, buf, sizeof(buf));
	isc_base32hex_totext(&sr, 1, "", &b);
	fprintf(stderr, "Break in NSEC3 chain at: %.*s\n",
		(int) isc_buffer_usedlength(&b), buf);

	DE_CONST(d1, sr.base);
	sr.length = first->next_length;
	isc_buffer_init(&b, buf, sizeof(buf));
	isc_base32hex_totext(&sr, 1, "", &b);
	fprintf(stderr, "Expected: %.*s\n", (int) isc_buffer_usedlength(&b),
		buf);

	DE_CONST(d2, sr.base);
	sr.length = first->next_length;
	isc_buffer_init(&b, buf, sizeof(buf));
	isc_base32hex_totext(&sr, 1, "", &b);
	fprintf(stderr, "Found: %.*s\n", (int) isc_buffer_usedlength(&b), buf);

	return (ISC_FALSE);
}

#define EXPECTEDANDFOUND "Expected and found NSEC3 chains not equal\n"

static isc_result_t
verify_nsec3_chains(isc_mem_t *mctx) {
	isc_result_t result = ISC_R_SUCCESS;
	struct nsec3_chain_fixed *e, *f = NULL;
	struct nsec3_chain_fixed *first = NULL, *prev = NULL;

	while ((e = isc_heap_element(expected_chains, 1)) != NULL) {
		isc_heap_delete(expected_chains, 1);
		if (f == NULL)
			f = isc_heap_element(found_chains, 1);
		if (f != NULL) {
			isc_heap_delete(found_chains, 1);

			/*
			 * Check that they match.
			 */
			if (chain_equal(e, f)) {
				free_element(mctx, f);
				f = NULL;
			} else {
				if (result == ISC_R_SUCCESS)
					fprintf(stderr, EXPECTEDANDFOUND);
				result = ISC_R_FAILURE;
				/*
				 * Attempt to resync found_chain.
				 */
				while (f != NULL && !chain_compare(e, f)) {
					free_element(mctx, f);
					f = isc_heap_element(found_chains, 1);
					if (f != NULL)
						isc_heap_delete(found_chains, 1);
					if (f != NULL && chain_equal(e, f)) {
						free_element(mctx, f);
						f = NULL;
						break;
					}
				}
			}
		} else if (result == ISC_R_SUCCESS) {
			fprintf(stderr, EXPECTEDANDFOUND);
			result = ISC_R_FAILURE;
		}
		if (first == NULL || newchain(first, e)) {
			if (prev != NULL) {
				if (!checknext(prev, first))
					result = ISC_R_FAILURE;
				if (prev != first)
					free_element(mctx, prev);
			}
			if (first != NULL)
				free_element(mctx, first);
			prev = first = e;
			continue;
		}
		if (!checknext(prev, e))
			result = ISC_R_FAILURE;
		if (prev != first)
			free_element(mctx, prev);
		prev = e;
	}
	if (prev != NULL) {
		if (!checknext(prev, first))
			result = ISC_R_FAILURE;
		if (prev != first)
			free_element(mctx, prev);
	}
	if (first != NULL)
		free_element(mctx, first);
	do {
		if (f != NULL) {
			if (result == ISC_R_SUCCESS) {
				fprintf(stderr, EXPECTEDANDFOUND);
				result = ISC_R_FAILURE;
			}
			free_element(mctx, f);
		}
		f = isc_heap_element(found_chains, 1);
		if (f != NULL)
			isc_heap_delete(found_chains, 1);
	} while (f != NULL);

	return (result);
}

static isc_result_t
verifyemptynodes(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *origin,
		 isc_mem_t *mctx, dns_name_t *name, dns_name_t *prevname,
		 isc_boolean_t isdelegation, dns_rdataset_t *nsec3paramset)
{
	dns_namereln_t reln;
	int order;
	unsigned int labels, nlabels, i;
	dns_name_t suffix;
	isc_result_t result = ISC_R_SUCCESS, tresult;

	reln = dns_name_fullcompare(prevname, name, &order, &labels);
	if (order >= 0)
		return (result);

	nlabels = dns_name_countlabels(name);

	if (reln == dns_namereln_commonancestor ||
	    reln == dns_namereln_contains) {
		dns_name_init(&suffix, NULL);
		for (i = labels + 1; i < nlabels; i++) {
			dns_name_getlabelsequence(name, nlabels - i, i,
						  &suffix);
			if (nsec3paramset != NULL &&
			     dns_rdataset_isassociated(nsec3paramset)) {
				tresult = verifynsec3s(db, ver, origin, mctx,
						       &suffix, nsec3paramset,
						       isdelegation, ISC_TRUE,
						       NULL, 0);
				if (result == ISC_R_SUCCESS &&
				    tresult != ISC_R_SUCCESS)
					result = tresult;
			}
		}
	}
	return (result);
}

/*%
 * Verify that certain things are sane:
 *
 *   The apex has a DNSKEY record with at least one KSK, and at least
 *   one ZSK if the -x flag was not used.
 *
 *   The DNSKEY record was signed with at least one of the KSKs in this
 *   set.
 *
 *   The rest of the zone was signed with at least one of the ZSKs
 *   present in the DNSKEY RRSET.
 */
void
verifyzone(dns_db_t *db, dns_dbversion_t *ver,
	   dns_name_t *origin, isc_mem_t *mctx,
	   isc_boolean_t ignore_kskflag, isc_boolean_t keyset_kskonly)
{
	char algbuf[80];
	dns_dbiterator_t *dbiter = NULL;
	dns_dbnode_t *node = NULL, *nextnode = NULL;
	dns_fixedname_t fname, fnextname, fprevname, fzonecut;
	dns_name_t *name, *nextname, *prevname, *zonecut;
	dns_rdata_dnskey_t dnskey;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t keyset, soaset;
	dns_rdataset_t keysigs, soasigs;
	dns_rdataset_t nsecset, nsecsigs;
	dns_rdataset_t nsec3paramset, nsec3paramsigs;
	int i;
	isc_boolean_t done = ISC_FALSE;
	isc_boolean_t first = ISC_TRUE;
	isc_boolean_t goodksk = ISC_FALSE;
	isc_boolean_t goodzsk = ISC_FALSE;
	isc_result_t result, vresult = ISC_R_UNSET;
	unsigned char revoked_ksk[256];
	unsigned char revoked_zsk[256];
	unsigned char standby_ksk[256];
	unsigned char standby_zsk[256];
	unsigned char ksk_algorithms[256];
	unsigned char zsk_algorithms[256];
	unsigned char bad_algorithms[256];
	unsigned char act_algorithms[256];

	result = isc_heap_create(mctx, chain_compare, NULL, 1024,
				 &expected_chains);
	check_result(result, "isc_heap_create()");
	result = isc_heap_create(mctx, chain_compare, NULL, 1024,
				 &found_chains);
	check_result(result, "isc_heap_create()");

	result = dns_db_findnode(db, origin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		fatal("failed to find the zone's origin: %s",
		      isc_result_totext(result));

	dns_rdataset_init(&keyset);
	dns_rdataset_init(&keysigs);
	dns_rdataset_init(&soaset);
	dns_rdataset_init(&soasigs);
	dns_rdataset_init(&nsecset);
	dns_rdataset_init(&nsecsigs);
	dns_rdataset_init(&nsec3paramset);
	dns_rdataset_init(&nsec3paramsigs);
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_dnskey,
				     0, 0, &keyset, &keysigs);
	if (result != ISC_R_SUCCESS)
		fatal("Zone contains no DNSSEC keys\n");

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_soa,
				     0, 0, &soaset, &soasigs);
	if (result != ISC_R_SUCCESS)
		fatal("Zone contains no SOA record\n");

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec,
				     0, 0, &nsecset, &nsecsigs);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		fatal("NSEC lookup failed\n");

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3param,
				     0, 0, &nsec3paramset, &nsec3paramsigs);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		fatal("NSEC3PARAM lookup failed\n");

	if (!dns_rdataset_isassociated(&keysigs))
		fatal("DNSKEY is not signed (keys offline or inactive?)\n");

	if (!dns_rdataset_isassociated(&soasigs))
		fatal("SOA is not signed (keys offline or inactive?)\n");

	if (dns_rdataset_isassociated(&nsecset) &&
	    !dns_rdataset_isassociated(&nsecsigs))
		fatal("NSEC is not signed (keys offline or inactive?)\n");

	if (dns_rdataset_isassociated(&nsec3paramset) &&
	    !dns_rdataset_isassociated(&nsec3paramsigs))
		fatal("NSEC3PARAM is not signed (keys offline or inactive?)\n");

	if (!dns_rdataset_isassociated(&nsecset) &&
	    !dns_rdataset_isassociated(&nsec3paramset))
		fatal("No valid NSEC/NSEC3 chain for testing\n");

	dns_db_detachnode(db, &node);

	memset(revoked_ksk, 0, sizeof(revoked_ksk));
	memset(revoked_zsk, 0, sizeof(revoked_zsk));
	memset(standby_ksk, 0, sizeof(standby_ksk));
	memset(standby_zsk, 0, sizeof(standby_zsk));
	memset(ksk_algorithms, 0, sizeof(ksk_algorithms));
	memset(zsk_algorithms, 0, sizeof(zsk_algorithms));
	memset(bad_algorithms, 0, sizeof(bad_algorithms));
	memset(act_algorithms, 0, sizeof(act_algorithms));

	/*
	 * Check that the DNSKEY RR has at least one self signing KSK
	 * and one ZSK per algorithm in it (or, if -x was used, one
	 * self-signing KSK).
	 */
	for (result = dns_rdataset_first(&keyset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&keyset)) {
		dns_rdataset_current(&keyset, &rdata);
		result = dns_rdata_tostruct(&rdata, &dnskey, NULL);
		check_result(result, "dns_rdata_tostruct");

		if ((dnskey.flags & DNS_KEYOWNER_ZONE) == 0)
			;
		else if ((dnskey.flags & DNS_KEYFLAG_REVOKE) != 0) {
			if ((dnskey.flags & DNS_KEYFLAG_KSK) != 0 &&
			    !dns_dnssec_selfsigns(&rdata, origin, &keyset,
						  &keysigs, ISC_FALSE,
						  mctx)) {
				char namebuf[DNS_NAME_FORMATSIZE];
				char buffer[1024];
				isc_buffer_t buf;

				dns_name_format(origin, namebuf,
						sizeof(namebuf));
				isc_buffer_init(&buf, buffer, sizeof(buffer));
				result = dns_rdata_totext(&rdata, NULL, &buf);
				check_result(result, "dns_rdata_totext");
				fatal("revoked KSK is not self signed:\n"
				      "%s DNSKEY %.*s", namebuf,
				      (int)isc_buffer_usedlength(&buf), buffer);
			}
			if ((dnskey.flags & DNS_KEYFLAG_KSK) != 0 &&
			     revoked_ksk[dnskey.algorithm] != 255)
				revoked_ksk[dnskey.algorithm]++;
			else if ((dnskey.flags & DNS_KEYFLAG_KSK) == 0 &&
				 revoked_zsk[dnskey.algorithm] != 255)
				revoked_zsk[dnskey.algorithm]++;
		} else if ((dnskey.flags & DNS_KEYFLAG_KSK) != 0) {
			if (dns_dnssec_selfsigns(&rdata, origin, &keyset,
						 &keysigs, ISC_FALSE, mctx)) {
				if (ksk_algorithms[dnskey.algorithm] != 255)
					ksk_algorithms[dnskey.algorithm]++;
				goodksk = ISC_TRUE;
			} else {
				if (standby_ksk[dnskey.algorithm] != 255)
					standby_ksk[dnskey.algorithm]++;
			}
		} else if (dns_dnssec_selfsigns(&rdata, origin, &keyset,
						&keysigs, ISC_FALSE, mctx)) {
			if (zsk_algorithms[dnskey.algorithm] != 255)
				zsk_algorithms[dnskey.algorithm]++;
			goodzsk = ISC_TRUE;
		} else if (dns_dnssec_signs(&rdata, origin, &soaset,
					    &soasigs, ISC_FALSE, mctx)) {
			if (zsk_algorithms[dnskey.algorithm] != 255)
				zsk_algorithms[dnskey.algorithm]++;
		} else {
			if (standby_zsk[dnskey.algorithm] != 255)
				standby_zsk[dnskey.algorithm]++;
		}
		dns_rdata_freestruct(&dnskey);
		dns_rdata_reset(&rdata);
	}
	dns_rdataset_disassociate(&keysigs);
	dns_rdataset_disassociate(&soaset);
	dns_rdataset_disassociate(&soasigs);
	if (dns_rdataset_isassociated(&nsecsigs))
		dns_rdataset_disassociate(&nsecsigs);
	if (dns_rdataset_isassociated(&nsec3paramsigs))
		dns_rdataset_disassociate(&nsec3paramsigs);

	if (ignore_kskflag ) {
		if (!goodksk && !goodzsk)
			fatal("No self-signed DNSKEY found.");
	} else if (!goodksk)
		fatal("No self-signed KSK DNSKEY found.  Supply an active\n"
		      "key with the KSK flag set, or use '-P'.");

	fprintf(stderr, "Verifying the zone using the following algorithms:");
	for (i = 0; i < 256; i++) {
		if (ignore_kskflag)
			act_algorithms[i] = (ksk_algorithms[i] != 0 ||
					     zsk_algorithms[i] != 0) ? 1 : 0;
		else
			act_algorithms[i] = ksk_algorithms[i] != 0 ? 1 : 0;
		if (act_algorithms[i] != 0) {
			dns_secalg_format(i, algbuf, sizeof(algbuf));
			fprintf(stderr, " %s", algbuf);
		}
	}
	fprintf(stderr, ".\n");

	if (!ignore_kskflag && !keyset_kskonly) {
		for (i = 0; i < 256; i++) {
			/*
			 * The counts should both be zero or both be non-zero.
			 * Mark the algorithm as bad if this is not met.
			 */
			if ((ksk_algorithms[i] != 0) ==
			    (zsk_algorithms[i] != 0))
				continue;
			dns_secalg_format(i, algbuf, sizeof(algbuf));
			fprintf(stderr, "Missing %s for algorithm %s\n",
				(ksk_algorithms[i] != 0)
				   ? "ZSK"
				   : "self-signed KSK",
				algbuf);
			bad_algorithms[i] = 1;
		}
	}

	/*
	 * Check that all the other records were signed by keys that are
	 * present in the DNSKEY RRSET.
	 */

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_fixedname_init(&fnextname);
	nextname = dns_fixedname_name(&fnextname);
	dns_fixedname_init(&fprevname);
	prevname = NULL;
	dns_fixedname_init(&fzonecut);
	zonecut = NULL;

	result = dns_db_createiterator(db, DNS_DB_NONSEC3, &dbiter);
	check_result(result, "dns_db_createiterator()");

	result = dns_dbiterator_first(dbiter);
	check_result(result, "dns_dbiterator_first()");

	while (!done) {
		isc_boolean_t isdelegation = ISC_FALSE;

		result = dns_dbiterator_current(dbiter, &node, name);
		check_dns_dbiterator_current(result);
		if (!dns_name_issubdomain(name, origin)) {
			check_no_nsec(name, node, db, ver);
			dns_db_detachnode(db, &node);
			result = dns_dbiterator_next(dbiter);
			if (result == ISC_R_NOMORE)
				done = ISC_TRUE;
			else
				check_result(result, "dns_dbiterator_next()");
			continue;
		}
		if (is_delegation(db, ver, origin, name, node, NULL)) {
			zonecut = dns_fixedname_name(&fzonecut);
			dns_name_copy(name, zonecut, NULL);
			isdelegation = ISC_TRUE;
		}
		nextnode = NULL;
		result = dns_dbiterator_next(dbiter);
		while (result == ISC_R_SUCCESS) {
			result = dns_dbiterator_current(dbiter, &nextnode,
							nextname);
			check_dns_dbiterator_current(result);
			if (!dns_name_issubdomain(nextname, origin) ||
			    (zonecut != NULL &&
			     dns_name_issubdomain(nextname, zonecut)))
			{
				check_no_nsec(nextname, nextnode, db, ver);
				dns_db_detachnode(db, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			if (is_empty(db, ver, nextnode)) {
				dns_db_detachnode(db, &nextnode);
				result = dns_dbiterator_next(dbiter);
				continue;
			}
			dns_db_detachnode(db, &nextnode);
			break;
		}
		if (result == ISC_R_NOMORE) {
			done = ISC_TRUE;
			nextname = origin;
		} else if (result != ISC_R_SUCCESS)
			fatal("iterating through the database failed: %s",
			      isc_result_totext(result));
		result = verifynode(db, ver, origin, mctx, name, node,
				    isdelegation, &keyset, act_algorithms,
				    bad_algorithms, &nsecset, &nsec3paramset,
				    nextname);
		if (vresult == ISC_R_UNSET)
			vresult = ISC_R_SUCCESS;
		if (vresult == ISC_R_SUCCESS && result != ISC_R_SUCCESS)
			vresult = result;
		if (prevname != NULL) {
			result = verifyemptynodes(db, ver, origin, mctx, name,
						  prevname, isdelegation,
						  &nsec3paramset);
		} else
			prevname = dns_fixedname_name(&fprevname);
		dns_name_copy(name, prevname, NULL);
		if (vresult == ISC_R_SUCCESS && result != ISC_R_SUCCESS)
			vresult = result;
		dns_db_detachnode(db, &node);
	}

	dns_dbiterator_destroy(&dbiter);

	result = dns_db_createiterator(db, DNS_DB_NSEC3ONLY, &dbiter);
	check_result(result, "dns_db_createiterator()");

	for (result = dns_dbiterator_first(dbiter);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(dbiter) ) {
		result = dns_dbiterator_current(dbiter, &node, name);
		check_dns_dbiterator_current(result);
		result = verifynode(db, ver, origin, mctx, name, node,
				    ISC_FALSE, &keyset, act_algorithms,
				    bad_algorithms, NULL, NULL, NULL);
		check_result(result, "verifynode");
		record_found(db, ver, mctx, name, node, &nsec3paramset);
		dns_db_detachnode(db, &node);
	}
	dns_dbiterator_destroy(&dbiter);

	dns_rdataset_disassociate(&keyset);
	if (dns_rdataset_isassociated(&nsecset))
		dns_rdataset_disassociate(&nsecset);
	if (dns_rdataset_isassociated(&nsec3paramset))
		dns_rdataset_disassociate(&nsec3paramset);

	result = verify_nsec3_chains(mctx);
	if (vresult == ISC_R_UNSET)
		vresult = ISC_R_SUCCESS;
	if (result != ISC_R_SUCCESS && vresult == ISC_R_SUCCESS)
		vresult = result;
	isc_heap_destroy(&expected_chains);
	isc_heap_destroy(&found_chains);

	/*
	 * If we made it this far, we have what we consider a properly signed
	 * zone.  Set the good flag.
	 */
	for (i = 0; i < 256; i++) {
		if (bad_algorithms[i] != 0) {
			if (first)
				fprintf(stderr, "The zone is not fully signed "
					"for the following algorithms:");
			dns_secalg_format(i, algbuf, sizeof(algbuf));
			fprintf(stderr, " %s", algbuf);
			first = ISC_FALSE;
		}
	}
	if (!first) {
		fprintf(stderr, ".\n");
		fatal("DNSSEC completeness test failed.");
	}

	if (vresult != ISC_R_SUCCESS)
		fatal("DNSSEC completeness test failed (%s).",
		      dns_result_totext(vresult));

	if (goodksk || ignore_kskflag) {
		/*
		 * Print the success summary.
		 */
		fprintf(stderr, "Zone fully signed:\n");
		for (i = 0; i < 256; i++) {
			if ((ksk_algorithms[i] != 0) ||
			    (standby_ksk[i] != 0) ||
			    (revoked_zsk[i] != 0) ||
			    (zsk_algorithms[i] != 0) ||
			    (standby_zsk[i] != 0) ||
			    (revoked_zsk[i] != 0)) {
				dns_secalg_format(i, algbuf, sizeof(algbuf));
				fprintf(stderr, "Algorithm: %s: KSKs: "
					"%u active, %u stand-by, %u revoked\n",
					algbuf, ksk_algorithms[i],
					standby_ksk[i], revoked_ksk[i]);
				fprintf(stderr, "%*sZSKs: "
					"%u active, %u %s, %u revoked\n",
					(int) strlen(algbuf) + 13, "",
					zsk_algorithms[i],
					standby_zsk[i],
					keyset_kskonly ? "present" : "stand-by",
					revoked_zsk[i]);
			}
		}
	}
}
