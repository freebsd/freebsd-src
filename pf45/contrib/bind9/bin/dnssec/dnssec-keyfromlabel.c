/*
 * Copyright (C) 2007, 2008, 2010  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssec-keyfromlabel.c,v 1.4.50.2 2010/01/15 23:47:31 tbox Exp $ */

/*! \file */

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/region.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/rdataclass.h>
#include <dns/result.h>
#include <dns/secalg.h>

#include <dst/dst.h>

#include "dnssectool.h"

#define MAX_RSA 4096 /* should be long enough... */

const char *program = "dnssec-keyfromlabel";
int verbose;

static const char *algs = "RSA | RSAMD5 | DH | DSA | RSASHA1 |"
			  " NSEC3DSA | NSEC3RSASHA1 |"
			  " RSASHA256 | RSASHA512";

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "    %s -a alg -l label [options] name\n\n",
		program);
	fprintf(stderr, "Version: %s\n", VERSION);
	fprintf(stderr, "Required options:\n");
	fprintf(stderr, "    -a algorithm: %s\n", algs);
	fprintf(stderr, "    -l label: label of the key\n");
	fprintf(stderr, "    name: owner of the key\n");
	fprintf(stderr, "Other options:\n");
	fprintf(stderr, "    -n nametype: ZONE | HOST | ENTITY | USER | OTHER\n");
	fprintf(stderr, "        (DNSKEY generation defaults to ZONE\n");
	fprintf(stderr, "    -c <class> (default: IN)\n");
	fprintf(stderr, "    -f keyflag: KSK\n");
	fprintf(stderr, "    -t <type>: "
		"AUTHCONF | NOAUTHCONF | NOAUTH | NOCONF "
		"(default: AUTHCONF)\n");
	fprintf(stderr, "    -p <protocol>: "
	       "default: 3 [dnssec]\n");
	fprintf(stderr, "    -v <verbose level>\n");
	fprintf(stderr, "    -k : generate a TYPE=KEY key\n");
	fprintf(stderr, "Output:\n");
	fprintf(stderr, "     K<name>+<alg>+<id>.key, "
		"K<name>+<alg>+<id>.private\n");

	exit (-1);
}

int
main(int argc, char **argv) {
	char		*algname = NULL, *nametype = NULL, *type = NULL;
	char		*classname = NULL;
	char		*endp;
	dst_key_t	*key = NULL, *oldkey;
	dns_fixedname_t	fname;
	dns_name_t	*name;
	isc_uint16_t	flags = 0, ksk = 0;
	dns_secalg_t	alg;
	isc_boolean_t	null_key = ISC_FALSE;
	isc_mem_t	*mctx = NULL;
	int		ch;
	int		protocol = -1, signatory = 0;
	isc_result_t	ret;
	isc_textregion_t r;
	char		filename[255];
	isc_buffer_t	buf;
	isc_log_t	*log = NULL;
	isc_entropy_t	*ectx = NULL;
	dns_rdataclass_t rdclass;
	int		options = DST_TYPE_PRIVATE | DST_TYPE_PUBLIC;
	char		*label = NULL;

	if (argc == 1)
		usage();

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	while ((ch = isc_commandline_parse(argc, argv,
					 "a:c:f:kl:n:p:t:v:h")) != -1)
	{
	    switch (ch) {
		case 'a':
			algname = isc_commandline_argument;
			break;
		case 'c':
			classname = isc_commandline_argument;
			break;
		case 'f':
			if (strcasecmp(isc_commandline_argument, "KSK") == 0)
				ksk = DNS_KEYFLAG_KSK;
			else
				fatal("unknown flag '%s'",
				      isc_commandline_argument);
			break;
		case 'k':
			options |= DST_TYPE_KEY;
			break;
		case 'l':
			label = isc_commandline_argument;
			break;
		case 'n':
			nametype = isc_commandline_argument;
			break;
		case 'p':
			protocol = strtol(isc_commandline_argument, &endp, 10);
			if (*endp != '\0' || protocol < 0 || protocol > 255)
				fatal("-p must be followed by a number "
				      "[0..255]");
			break;
		case 't':
			type = isc_commandline_argument;
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
		case 'h':
			usage();

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);
	ret = dst_lib_init(mctx, ectx,
			   ISC_ENTROPY_BLOCKING | ISC_ENTROPY_GOODONLY);
	if (ret != ISC_R_SUCCESS)
		fatal("could not initialize dst");

	setup_logging(verbose, mctx, &log);

	if (label == NULL)
		fatal("the key label was not specified");
	if (argc < isc_commandline_index + 1)
		fatal("the key name was not specified");
	if (argc > isc_commandline_index + 1)
		fatal("extraneous arguments");

	if (algname == NULL)
		fatal("no algorithm was specified");
	if (strcasecmp(algname, "RSA") == 0) {
		fprintf(stderr, "The use of RSA (RSAMD5) is not recommended.\n"
				"If you still wish to use RSA (RSAMD5) please "
				"specify \"-a RSAMD5\"\n");
		return (1);
	} else {
		r.base = algname;
		r.length = strlen(algname);
		ret = dns_secalg_fromtext(&alg, &r);
		if (ret != ISC_R_SUCCESS)
			fatal("unknown algorithm %s", algname);
		if (alg == DST_ALG_DH)
			options |= DST_TYPE_KEY;
	}

	if (type != NULL && (options & DST_TYPE_KEY) != 0) {
		if (strcasecmp(type, "NOAUTH") == 0)
			flags |= DNS_KEYTYPE_NOAUTH;
		else if (strcasecmp(type, "NOCONF") == 0)
			flags |= DNS_KEYTYPE_NOCONF;
		else if (strcasecmp(type, "NOAUTHCONF") == 0) {
			flags |= (DNS_KEYTYPE_NOAUTH | DNS_KEYTYPE_NOCONF);
		}
		else if (strcasecmp(type, "AUTHCONF") == 0)
			/* nothing */;
		else
			fatal("invalid type %s", type);
	}

	if (nametype == NULL) {
		if ((options & DST_TYPE_KEY) != 0) /* KEY */
			fatal("no nametype specified");
		flags |= DNS_KEYOWNER_ZONE;	/* DNSKEY */
	} else if (strcasecmp(nametype, "zone") == 0)
		flags |= DNS_KEYOWNER_ZONE;
	else if ((options & DST_TYPE_KEY) != 0)	{ /* KEY */
		if (strcasecmp(nametype, "host") == 0 ||
			 strcasecmp(nametype, "entity") == 0)
			flags |= DNS_KEYOWNER_ENTITY;
		else if (strcasecmp(nametype, "user") == 0)
			flags |= DNS_KEYOWNER_USER;
		else
			fatal("invalid KEY nametype %s", nametype);
	} else if (strcasecmp(nametype, "other") != 0) /* DNSKEY */
		fatal("invalid DNSKEY nametype %s", nametype);

	rdclass = strtoclass(classname);

	if ((options & DST_TYPE_KEY) != 0)  /* KEY */
		flags |= signatory;
	else if ((flags & DNS_KEYOWNER_ZONE) != 0) /* DNSKEY */
		flags |= ksk;

	if (protocol == -1)
		protocol = DNS_KEYPROTO_DNSSEC;
	else if ((options & DST_TYPE_KEY) == 0 &&
		 protocol != DNS_KEYPROTO_DNSSEC)
		fatal("invalid DNSKEY protocol: %d", protocol);

	if ((flags & DNS_KEYFLAG_TYPEMASK) == DNS_KEYTYPE_NOKEY) {
		if ((flags & DNS_KEYFLAG_SIGNATORYMASK) != 0)
			fatal("specified null key with signing authority");
	}

	if ((flags & DNS_KEYFLAG_OWNERMASK) == DNS_KEYOWNER_ZONE &&
	    alg == DNS_KEYALG_DH)
		fatal("a key with algorithm '%s' cannot be a zone key",
		      algname);

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	isc_buffer_init(&buf, argv[isc_commandline_index],
			strlen(argv[isc_commandline_index]));
	isc_buffer_add(&buf, strlen(argv[isc_commandline_index]));
	ret = dns_name_fromtext(name, &buf, dns_rootname, ISC_FALSE, NULL);
	if (ret != ISC_R_SUCCESS)
		fatal("invalid key name %s: %s", argv[isc_commandline_index],
		      isc_result_totext(ret));

	if ((flags & DNS_KEYFLAG_TYPEMASK) == DNS_KEYTYPE_NOKEY)
		null_key = ISC_TRUE;

	isc_buffer_init(&buf, filename, sizeof(filename) - 1);

	/* associate the key */
	ret = dst_key_fromlabel(name, alg, flags, protocol,
				rdclass, "", label, NULL, mctx, &key);
	isc_entropy_stopcallbacksources(ectx);

	if (ret != ISC_R_SUCCESS) {
		char namestr[DNS_NAME_FORMATSIZE];
		char algstr[ALG_FORMATSIZE];
		dns_name_format(name, namestr, sizeof(namestr));
		alg_format(alg, algstr, sizeof(algstr));
		fatal("failed to generate key %s/%s: %s\n",
		      namestr, algstr, isc_result_totext(ret));
		exit(-1);
	}

	/*
	 * Try to read a key with the same name, alg and id from disk.
	 * If there is one we must continue generating a new one
	 * unless we were asked to generate a null key, in which
	 * case we return failure.
	 */
	ret = dst_key_fromfile(name, dst_key_id(key), alg,
			       DST_TYPE_PRIVATE, NULL, mctx, &oldkey);
	/* do not overwrite an existing key  */
	if (ret == ISC_R_SUCCESS) {
		isc_buffer_clear(&buf);
		ret = dst_key_buildfilename(key, 0, NULL, &buf);
		fprintf(stderr, "%s: %s already exists\n",
			program, filename);
		dst_key_free(&key);
		exit (1);
	}

	ret = dst_key_tofile(key, options, NULL);
	if (ret != ISC_R_SUCCESS) {
		char keystr[KEY_FORMATSIZE];
		key_format(key, keystr, sizeof(keystr));
		fatal("failed to write key %s: %s\n", keystr,
		      isc_result_totext(ret));
	}

	isc_buffer_clear(&buf);
	ret = dst_key_buildfilename(key, 0, NULL, &buf);
	printf("%s\n", filename);
	dst_key_free(&key);

	cleanup_logging(&log);
	cleanup_entropy(&ectx);
	dst_lib_destroy();
	dns_name_destroy();
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	return (0);
}
