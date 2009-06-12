/*
 * Portions Copyright (C) 2004-2008  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssec-keygen.c,v 1.81 2008/09/25 04:02:38 tbox Exp $ */

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

const char *program = "dnssec-keygen";
int verbose;

static const char *algs = "RSA | RSAMD5 | DH | DSA | RSASHA1 | NSEC3DSA |"
			  " NSEC3RSASHA1 | HMAC-MD5 |"
			  " HMAC-SHA1 | HMAC-SHA224 | HMAC-SHA256 |"
			  " HMAC-SHA384 | HMAC-SHA512";

static isc_boolean_t
dsa_size_ok(int size) {
	return (ISC_TF(size >= 512 && size <= 1024 && size % 64 == 0));
}

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "    %s -a alg -b bits [-n type] [options] name\n\n",
		program);
	fprintf(stderr, "Version: %s\n", VERSION);
	fprintf(stderr, "Required options:\n");
	fprintf(stderr, "    -a algorithm: %s\n", algs);
	fprintf(stderr, "    -b key size, in bits:\n");
	fprintf(stderr, "        RSAMD5:\t\t[512..%d]\n", MAX_RSA);
	fprintf(stderr, "        RSASHA1:\t\t[512..%d]\n", MAX_RSA);
	fprintf(stderr, "        NSEC3RSASHA1:\t\t[512..%d]\n", MAX_RSA);
	fprintf(stderr, "        DH:\t\t[128..4096]\n");
	fprintf(stderr, "        DSA:\t\t[512..1024] and divisible by 64\n");
	fprintf(stderr, "        NSEC3DSA:\t\t[512..1024] and divisible by 64\n");
	fprintf(stderr, "        HMAC-MD5:\t[1..512]\n");
	fprintf(stderr, "        HMAC-SHA1:\t[1..160]\n");
	fprintf(stderr, "        HMAC-SHA224:\t[1..224]\n");
	fprintf(stderr, "        HMAC-SHA256:\t[1..256]\n");
	fprintf(stderr, "        HMAC-SHA384:\t[1..384]\n");
	fprintf(stderr, "        HMAC-SHA512:\t[1..512]\n");
	fprintf(stderr, "    -n nametype: ZONE | HOST | ENTITY | USER | OTHER\n");
	fprintf(stderr, "        (DNSKEY generation defaults to ZONE\n");
	fprintf(stderr, "    name: owner of the key\n");
	fprintf(stderr, "Other options:\n");
	fprintf(stderr, "    -c <class> (default: IN)\n");
	fprintf(stderr, "    -d <digest bits> (0 => max, default)\n");
	fprintf(stderr, "    -e use large exponent (RSAMD5/RSASHA1 only)\n");
	fprintf(stderr, "    -f keyflag: KSK\n");
	fprintf(stderr, "    -g <generator> use specified generator "
		"(DH only)\n");
	fprintf(stderr, "    -t <type>: "
		"AUTHCONF | NOAUTHCONF | NOAUTH | NOCONF "
		"(default: AUTHCONF)\n");
	fprintf(stderr, "    -p <protocol>: "
	       "default: 3 [dnssec]\n");
	fprintf(stderr, "    -s <strength> strength value this key signs DNS "
		"records with (default: 0)\n");
	fprintf(stderr, "    -r <randomdev>: a file containing random data\n");
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
	isc_boolean_t	conflict = ISC_FALSE, null_key = ISC_FALSE;
	isc_mem_t	*mctx = NULL;
	int		ch, rsa_exp = 0, generator = 0, param = 0;
	int		protocol = -1, size = -1, signatory = 0;
	isc_result_t	ret;
	isc_textregion_t r;
	char		filename[255];
	isc_buffer_t	buf;
	isc_log_t	*log = NULL;
	isc_entropy_t	*ectx = NULL;
	dns_rdataclass_t rdclass;
	int		options = DST_TYPE_PRIVATE | DST_TYPE_PUBLIC;
	int		dbits = 0;

	if (argc == 1)
		usage();

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	while ((ch = isc_commandline_parse(argc, argv,
					 "a:b:c:d:ef:g:kn:t:p:s:r:v:h")) != -1)
	{
	    switch (ch) {
		case 'a':
			algname = isc_commandline_argument;
			break;
		case 'b':
			size = strtol(isc_commandline_argument, &endp, 10);
			if (*endp != '\0' || size < 0)
				fatal("-b requires a non-negative number");
			break;
		case 'c':
			classname = isc_commandline_argument;
			break;
		case 'd':
			dbits = strtol(isc_commandline_argument, &endp, 10);
			if (*endp != '\0' || dbits < 0)
				fatal("-d requires a non-negative number");
			break;
		case 'e':
			rsa_exp = 1;
			break;
		case 'f':
			if (strcasecmp(isc_commandline_argument, "KSK") == 0)
				ksk = DNS_KEYFLAG_KSK;
			else
				fatal("unknown flag '%s'",
				      isc_commandline_argument);
			break;
		case 'g':
			generator = strtol(isc_commandline_argument,
					   &endp, 10);
			if (*endp != '\0' || generator <= 0)
				fatal("-g requires a positive number");
			break;
		case 'k':
			options |= DST_TYPE_KEY;
			break;
		case 'n':
			nametype = isc_commandline_argument;
			break;
		case 't':
			type = isc_commandline_argument;
			break;
		case 'p':
			protocol = strtol(isc_commandline_argument, &endp, 10);
			if (*endp != '\0' || protocol < 0 || protocol > 255)
				fatal("-p must be followed by a number "
				      "[0..255]");
			break;
		case 's':
			signatory = strtol(isc_commandline_argument,
					   &endp, 10);
			if (*endp != '\0' || signatory < 0 || signatory > 15)
				fatal("-s must be followed by a number "
				      "[0..15]");
			break;
		case 'r':
			setup_entropy(mctx, isc_commandline_argument, &ectx);
			break;
		case 'v':
			endp = NULL;
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
	} else if (strcasecmp(algname, "HMAC-MD5") == 0) {
		options |= DST_TYPE_KEY;
		alg = DST_ALG_HMACMD5;
	} else if (strcasecmp(algname, "HMAC-SHA1") == 0) {
		options |= DST_TYPE_KEY;
		alg = DST_ALG_HMACSHA1;
	} else if (strcasecmp(algname, "HMAC-SHA224") == 0) {
		options |= DST_TYPE_KEY;
		alg = DST_ALG_HMACSHA224;
	} else if (strcasecmp(algname, "HMAC-SHA256") == 0) {
		options |= DST_TYPE_KEY;
		alg = DST_ALG_HMACSHA256;
	} else if (strcasecmp(algname, "HMAC-SHA384") == 0) {
		options |= DST_TYPE_KEY;
		alg = DST_ALG_HMACSHA384;
	} else if (strcasecmp(algname, "HMAC-SHA512") == 0) {
		options |= DST_TYPE_KEY;
		alg = DST_ALG_HMACSHA512;
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
			if (size < 0)
				size = 0;
		}
		else if (strcasecmp(type, "AUTHCONF") == 0)
			/* nothing */;
		else
			fatal("invalid type %s", type);
	}

	if (size < 0)
		fatal("key size not specified (-b option)");

	switch (alg) {
	case DNS_KEYALG_RSAMD5:
	case DNS_KEYALG_RSASHA1:
	case DNS_KEYALG_NSEC3RSASHA1:
		if (size != 0 && (size < 512 || size > MAX_RSA))
			fatal("RSA key size %d out of range", size);
		break;
	case DNS_KEYALG_DH:
		if (size != 0 && (size < 128 || size > 4096))
			fatal("DH key size %d out of range", size);
		break;
	case DNS_KEYALG_DSA:
	case DNS_KEYALG_NSEC3DSA:
		if (size != 0 && !dsa_size_ok(size))
			fatal("invalid DSS key size: %d", size);
		break;
	case DST_ALG_HMACMD5:
		if (size < 1 || size > 512)
			fatal("HMAC-MD5 key size %d out of range", size);
		if (dbits != 0 && (dbits < 80 || dbits > 128))
			fatal("HMAC-MD5 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-MD5 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA1:
		if (size < 1 || size > 160)
			fatal("HMAC-SHA1 key size %d out of range", size);
		if (dbits != 0 && (dbits < 80 || dbits > 160))
			fatal("HMAC-SHA1 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA1 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA224:
		if (size < 1 || size > 224)
			fatal("HMAC-SHA224 key size %d out of range", size);
		if (dbits != 0 && (dbits < 112 || dbits > 224))
			fatal("HMAC-SHA224 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA224 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA256:
		if (size < 1 || size > 256)
			fatal("HMAC-SHA256 key size %d out of range", size);
		if (dbits != 0 && (dbits < 128 || dbits > 256))
			fatal("HMAC-SHA256 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA256 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA384:
		if (size < 1 || size > 384)
			fatal("HMAC-384 key size %d out of range", size);
		if (dbits != 0 && (dbits < 192 || dbits > 384))
			fatal("HMAC-SHA384 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA384 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA512:
		if (size < 1 || size > 512)
			fatal("HMAC-SHA512 key size %d out of range", size);
		if (dbits != 0 && (dbits < 256 || dbits > 512))
			fatal("HMAC-SHA512 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA512 digest bits %d not divisible by 8",
			      dbits);
		break;
	}

	if (!(alg == DNS_KEYALG_RSAMD5 || alg == DNS_KEYALG_RSASHA1 ||
	      alg == DNS_KEYALG_NSEC3RSASHA1) && rsa_exp != 0)
		fatal("specified RSA exponent for a non-RSA key");

	if (alg != DNS_KEYALG_DH && generator != 0)
		fatal("specified DH generator for a non-DH key");

	if (nametype == NULL) {
		if ((options & DST_TYPE_KEY) != 0) /* KEY / HMAC */
			fatal("no nametype specified");
		flags |= DNS_KEYOWNER_ZONE;	/* DNSKEY */
	} else if (strcasecmp(nametype, "zone") == 0)
		flags |= DNS_KEYOWNER_ZONE;
	else if ((options & DST_TYPE_KEY) != 0)	{ /* KEY / HMAC */
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

	if ((options & DST_TYPE_KEY) != 0)  /* KEY / HMAC */
		flags |= signatory;
	else if ((flags & DNS_KEYOWNER_ZONE) != 0) /* DNSKEY */
		flags |= ksk;

	if (protocol == -1)
		protocol = DNS_KEYPROTO_DNSSEC;
	else if ((options & DST_TYPE_KEY) == 0 &&
		 protocol != DNS_KEYPROTO_DNSSEC)
		fatal("invalid DNSKEY protocol: %d", protocol);

	if ((flags & DNS_KEYFLAG_TYPEMASK) == DNS_KEYTYPE_NOKEY) {
		if (size > 0)
			fatal("specified null key with non-zero size");
		if ((flags & DNS_KEYFLAG_SIGNATORYMASK) != 0)
			fatal("specified null key with signing authority");
	}

	if ((flags & DNS_KEYFLAG_OWNERMASK) == DNS_KEYOWNER_ZONE &&
	    (alg == DNS_KEYALG_DH || alg == DST_ALG_HMACMD5 ||
	     alg == DST_ALG_HMACSHA1 || alg == DST_ALG_HMACSHA224 ||
	     alg == DST_ALG_HMACSHA256 || alg == DST_ALG_HMACSHA384 ||
	     alg == DST_ALG_HMACSHA512))
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

	switch(alg) {
	case DNS_KEYALG_RSAMD5:
	case DNS_KEYALG_RSASHA1:
		param = rsa_exp;
		break;
	case DNS_KEYALG_DH:
		param = generator;
		break;
	case DNS_KEYALG_DSA:
	case DST_ALG_HMACMD5:
	case DST_ALG_HMACSHA1:
	case DST_ALG_HMACSHA224:
	case DST_ALG_HMACSHA256:
	case DST_ALG_HMACSHA384:
	case DST_ALG_HMACSHA512:
		param = 0;
		break;
	}

	if ((flags & DNS_KEYFLAG_TYPEMASK) == DNS_KEYTYPE_NOKEY)
		null_key = ISC_TRUE;

	isc_buffer_init(&buf, filename, sizeof(filename) - 1);

	do {
		conflict = ISC_FALSE;
		oldkey = NULL;

		/* generate the key */
		ret = dst_key_generate(name, alg, size, param, flags, protocol,
				       rdclass, mctx, &key);
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

		dst_key_setbits(key, dbits);

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
			dst_key_free(&oldkey);
			conflict = ISC_TRUE;
			if (null_key)
				break;
		}
		if (conflict == ISC_TRUE) {
			if (verbose > 0) {
				isc_buffer_clear(&buf);
				ret = dst_key_buildfilename(key, 0, NULL, &buf);
				fprintf(stderr,
					"%s: %s already exists, "
					"generating a new key\n",
					program, filename);
			}
			dst_key_free(&key);
		}

	} while (conflict == ISC_TRUE);

	if (conflict)
		fatal("cannot generate a null key when a key with id 0 "
		      "already exists");

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
