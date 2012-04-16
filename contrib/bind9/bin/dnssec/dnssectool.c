/*
 * Copyright (C) 2004, 2005, 2007, 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssectool.c,v 1.60.162.3 2011/10/21 03:56:32 marka Exp $ */

/*! \file */

/*%
 * DNSSEC Support Routines.
 */

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/list.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/time.h>
#include <isc/util.h>
#include <isc/print.h>

#include <dns/dnssec.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/rdatastruct.h>
#include <dns/rdataclass.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/secalg.h>
#include <dns/time.h>

#include "dnssectool.h"

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
setup_logging(int verbose, isc_mem_t *mctx, isc_log_t **logp) {
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

dns_ttl_t
strtottl(const char *str) {
	const char *orig = str;
	dns_ttl_t ttl;
	char *endp;

	ttl = strtol(str, &endp, 0);
	if (ttl == 0 && endp == str)
		fatal("TTL must be numeric");
	ttl = time_units(ttl, endp, orig);
	return (ttl);
}

isc_stdtime_t
strtotime(const char *str, isc_int64_t now, isc_int64_t base) {
	isc_int64_t val, offset;
	isc_result_t result;
	const char *orig = str;
	char *endp;

	if ((str[0] == '0' || str[0] == '-') && str[1] == '\0')
		return ((isc_stdtime_t) 0);

	if (strncmp(str, "now", 3) == 0) {
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
	} else if (strlen(str) == 8U) {
		char timestr[15];
		sprintf(timestr, "%s000000", str);
		result = dns_time64_fromtext(timestr, &val);
		if (result != ISC_R_SUCCESS)
			fatal("time value %s is invalid: %s", orig,
			      isc_result_totext(result));
	} else if (strlen(str) > 14U) {
		fatal("time value %s is invalid", orig);
	} else {
		result = dns_time64_fromtext(str, &val);
		if (result != ISC_R_SUCCESS)
			fatal("time value %s is invalid: %s", orig,
			      isc_result_totext(result));
	}

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
