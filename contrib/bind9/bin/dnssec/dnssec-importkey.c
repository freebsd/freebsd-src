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

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/callbacks.h>
#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/ds.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/master.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#include <dst/dst.h>

#include "dnssectool.h"

#ifndef PATH_MAX
#define PATH_MAX 1024   /* AIX, WIN32, and others don't define this. */
#endif

const char *program = "dnssec-importkey";
int verbose;

static dns_rdataclass_t rdclass;
static dns_fixedname_t	fixed;
static dns_name_t	*name = NULL;
static isc_mem_t	*mctx = NULL;
static isc_boolean_t	setpub = ISC_FALSE, setdel = ISC_FALSE;
static isc_boolean_t	setttl = ISC_FALSE;
static isc_stdtime_t	pub = 0, del = 0;
static dns_ttl_t	ttl = 0;

static isc_result_t
initname(char *setname) {
	isc_result_t result;
	isc_buffer_t buf;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);

	isc_buffer_init(&buf, setname, strlen(setname));
	isc_buffer_add(&buf, strlen(setname));
	result = dns_name_fromtext(name, &buf, dns_rootname, 0, NULL);
	return (result);
}

static void
db_load_from_stream(dns_db_t *db, FILE *fp) {
	isc_result_t result;
	dns_rdatacallbacks_t callbacks;

	dns_rdatacallbacks_init(&callbacks);
	result = dns_db_beginload(db, &callbacks.add, &callbacks.add_private);
	if (result != ISC_R_SUCCESS)
		fatal("dns_db_beginload failed: %s", isc_result_totext(result));

	result = dns_master_loadstream(fp, name, name, rdclass, 0,
				       &callbacks, mctx);
	if (result != ISC_R_SUCCESS)
		fatal("can't load from input: %s", isc_result_totext(result));

	result = dns_db_endload(db, &callbacks.add_private);
	if (result != ISC_R_SUCCESS)
		fatal("dns_db_endload failed: %s", isc_result_totext(result));
}

static isc_result_t
loadset(const char *filename, dns_rdataset_t *rdataset) {
	isc_result_t	 result;
	dns_db_t	 *db = NULL;
	dns_dbnode_t	 *node = NULL;
	char setname[DNS_NAME_FORMATSIZE];

	dns_name_format(name, setname, sizeof(setname));

	result = dns_db_create(mctx, "rbt", name, dns_dbtype_zone,
			       rdclass, 0, NULL, &db);
	if (result != ISC_R_SUCCESS)
		fatal("can't create database");

	if (strcmp(filename, "-") == 0) {
		db_load_from_stream(db, stdin);
		filename = "input";
	} else {
		result = dns_db_load3(db, filename, dns_masterformat_text,
				      DNS_MASTER_NOTTL);
		if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE)
			fatal("can't load %s: %s", filename,
			      isc_result_totext(result));
	}

	result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		fatal("can't find %s node in %s", setname, filename);

	result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_dnskey,
				     0, 0, rdataset, NULL);

	if (result == ISC_R_NOTFOUND)
		fatal("no DNSKEY RR for %s in %s", setname, filename);
	else if (result != ISC_R_SUCCESS)
		fatal("dns_db_findrdataset");

	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	return (result);
}

static void
loadkey(char *filename, unsigned char *key_buf, unsigned int key_buf_size,
	dns_rdata_t *rdata)
{
	isc_result_t  result;
	dst_key_t     *key = NULL;
	isc_buffer_t  keyb;
	isc_region_t  r;

	dns_rdata_init(rdata);

	isc_buffer_init(&keyb, key_buf, key_buf_size);

	result = dst_key_fromnamedfile(filename, NULL, DST_TYPE_PUBLIC,
				       mctx, &key);
	if (result != ISC_R_SUCCESS)
		fatal("invalid keyfile name %s: %s",
		      filename, isc_result_totext(result));

	if (verbose > 2) {
		char keystr[DST_KEY_FORMATSIZE];

		dst_key_format(key, keystr, sizeof(keystr));
		fprintf(stderr, "%s: %s\n", program, keystr);
	}

	result = dst_key_todns(key, &keyb);
	if (result != ISC_R_SUCCESS)
		fatal("can't decode key");

	isc_buffer_usedregion(&keyb, &r);
	dns_rdata_fromregion(rdata, dst_key_class(key),
			     dns_rdatatype_dnskey, &r);

	rdclass = dst_key_class(key);

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	result = dns_name_copy(dst_key_name(key), name, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("can't copy name");

	dst_key_free(&key);
}

static void
emit(const char *dir, dns_rdata_t *rdata) {
	isc_result_t result;
	char keystr[DST_KEY_FORMATSIZE];
	char pubname[1024];
	char priname[1024];
	isc_buffer_t buf;
	dst_key_t *key = NULL, *tmp = NULL;

	isc_buffer_init(&buf, rdata->data, rdata->length);
	isc_buffer_add(&buf, rdata->length);
	result = dst_key_fromdns(name, rdclass, &buf, mctx, &key);
	if (result != ISC_R_SUCCESS) {
		fatal("dst_key_fromdns: %s", isc_result_totext(result));
	}

	isc_buffer_init(&buf, pubname, sizeof(pubname));
	result = dst_key_buildfilename(key, DST_TYPE_PUBLIC, dir, &buf);
	if (result != ISC_R_SUCCESS) {
		fatal("Failed to build public key filename: %s",
		      isc_result_totext(result));
	}
	isc_buffer_init(&buf, priname, sizeof(priname));
	result = dst_key_buildfilename(key, DST_TYPE_PRIVATE, dir, &buf);
	if (result != ISC_R_SUCCESS) {
		fatal("Failed to build private key filename: %s",
		      isc_result_totext(result));
	}

	result = dst_key_fromfile(dst_key_name(key), dst_key_id(key),
				  dst_key_alg(key),
				  DST_TYPE_PUBLIC | DST_TYPE_PRIVATE,
				  dir, mctx, &tmp);
	if (result == ISC_R_SUCCESS) {
		if (dst_key_isprivate(tmp) && !dst_key_isexternal(tmp))
			fatal("Private key already exists in %s", priname);
		dst_key_free(&tmp);
	}

	dst_key_setexternal(key, ISC_TRUE);
	if (setpub)
		dst_key_settime(key, DST_TIME_PUBLISH, pub);
	if (setdel)
		dst_key_settime(key, DST_TIME_DELETE, del);
	if (setttl)
		dst_key_setttl(key, ttl);

	result = dst_key_tofile(key, DST_TYPE_PUBLIC|DST_TYPE_PRIVATE,
				dir);
	if (result != ISC_R_SUCCESS) {
		dst_key_format(key, keystr, sizeof(keystr));
		fatal("Failed to write key %s: %s", keystr,
		      isc_result_totext(result));
	}
	printf("%s\n", pubname);

	isc_buffer_clear(&buf);
	result = dst_key_buildfilename(key, DST_TYPE_PRIVATE, dir, &buf);
	if (result != ISC_R_SUCCESS) {
		fatal("Failed to build private key filename: %s",
		      isc_result_totext(result));
	}
	printf("%s\n", priname);
	dst_key_free(&key);
}

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,	"    %s options [-K dir] keyfile\n\n", program);
	fprintf(stderr, "    %s options -f file [keyname]\n\n", program);
	fprintf(stderr, "Version: %s\n", VERSION);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "    -f file: read key from zone file\n");
	fprintf(stderr, "    -K <directory>: directory in which to store "
				"the key files\n");
	fprintf(stderr, "    -L ttl:             set default key TTL\n");
	fprintf(stderr, "    -v <verbose level>\n");
	fprintf(stderr, "    -V: print version information\n");
	fprintf(stderr, "    -h: print usage and exit\n");
	fprintf(stderr, "Timing options:\n");
	fprintf(stderr, "    -P date/[+-]offset/none: set/unset key "
						     "publication date\n");
	fprintf(stderr, "    -D date/[+-]offset/none: set/unset key "
						     "deletion date\n");

	exit (-1);
}

int
main(int argc, char **argv) {
	char		*classname = NULL;
	char		*filename = NULL, *dir = NULL, *namestr;
	char		*endp;
	int		ch;
	isc_result_t	result;
	isc_log_t	*log = NULL;
	isc_entropy_t	*ectx = NULL;
	dns_rdataset_t	rdataset;
	dns_rdata_t	rdata;
	isc_stdtime_t   now;

	dns_rdata_init(&rdata);
	isc_stdtime_get(&now);

	if (argc == 1)
		usage();

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("out of memory");

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

#define CMDLINE_FLAGS "D:f:hK:L:P:v:V"
	while ((ch = isc_commandline_parse(argc, argv, CMDLINE_FLAGS)) != -1) {
		switch (ch) {
		case 'D':
			if (setdel)
				fatal("-D specified more than once");

			del = strtotime(isc_commandline_argument,
					now, now, &setdel);
			break;
		case 'K':
			dir = isc_commandline_argument;
			if (strlen(dir) == 0U)
				fatal("directory must be non-empty string");
			break;
		case 'L':
			ttl = strtottl(isc_commandline_argument);
			setttl = ISC_TRUE;
			break;
		case 'P':
			if (setpub)
				fatal("-P specified more than once");

			pub = strtotime(isc_commandline_argument,
					now, now, &setpub);
			break;
		case 'f':
			filename = isc_commandline_argument;
			break;
		case 'v':
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("-v must be followed by a number");
			break;
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

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	rdclass = strtoclass(classname);

	if (argc < isc_commandline_index + 1 && filename == NULL)
		fatal("the key file name was not specified");
	if (argc > isc_commandline_index + 1)
		fatal("extraneous arguments");

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);
	result = isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize hash");
	result = dst_lib_init(mctx, ectx,
			      ISC_ENTROPY_BLOCKING | ISC_ENTROPY_GOODONLY);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize dst: %s",
		      isc_result_totext(result));
	isc_entropy_stopcallbacksources(ectx);

	setup_logging(mctx, &log);

	dns_rdataset_init(&rdataset);

	if (filename != NULL) {
		if (argc < isc_commandline_index + 1) {
			/* using filename as zone name */
			namestr = filename;
		} else
			namestr = argv[isc_commandline_index];

		result = initname(namestr);
		if (result != ISC_R_SUCCESS)
			fatal("could not initialize name %s", namestr);

		result = loadset(filename, &rdataset);

		if (result != ISC_R_SUCCESS)
			fatal("could not load DNSKEY set: %s\n",
			      isc_result_totext(result));

		for (result = dns_rdataset_first(&rdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&rdataset)) {

			dns_rdata_init(&rdata);
			dns_rdataset_current(&rdataset, &rdata);
			emit(dir, &rdata);
		}
	} else {
		unsigned char key_buf[DST_KEY_MAXSIZE];

		loadkey(argv[isc_commandline_index], key_buf,
			DST_KEY_MAXSIZE, &rdata);

		emit(dir, &rdata);
	}

	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	cleanup_logging(&log);
	dst_lib_destroy();
	isc_hash_destroy();
	cleanup_entropy(&ectx);
	dns_name_destroy();
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	fflush(stdout);
	if (ferror(stdout)) {
		fprintf(stderr, "write error\n");
		return (1);
	} else
		return (0);
}
