/*
 * Copyright (C) 2007-2010  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssec-keyfromlabel.c,v 1.32 2010-12-23 04:07:59 marka Exp $ */

/*! \file */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/region.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/dnssec.h>
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

#define DEFAULT_ALGORITHM "RSASHA1"
#define DEFAULT_NSEC3_ALGORITHM "NSEC3RSASHA1"

static const char *algs = "RSA | RSAMD5 | DH | DSA | RSASHA1 |"
			  " NSEC3DSA | NSEC3RSASHA1 |"
			  " RSASHA256 | RSASHA512 | ECCGOST";

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "    %s -l label [options] name\n\n",
		program);
	fprintf(stderr, "Version: %s\n", VERSION);
	fprintf(stderr, "Required options:\n");
	fprintf(stderr, "    -l label: label of the key pair\n");
	fprintf(stderr, "    name: owner of the key\n");
	fprintf(stderr, "Other options:\n");
	fprintf(stderr, "    -a algorithm: %s\n", algs);
	fprintf(stderr, "       (default: RSASHA1, or "
			       "NSEC3RSASHA1 if using -3)\n");
	fprintf(stderr, "    -3: use NSEC3-capable algorithm\n");
	fprintf(stderr, "    -c class (default: IN)\n");
#ifdef USE_PKCS11
	fprintf(stderr, "    -E enginename (default: pkcs11)\n");
#else
	fprintf(stderr, "    -E enginename\n");
#endif
	fprintf(stderr, "    -f keyflag: KSK | REVOKE\n");
	fprintf(stderr, "    -K directory: directory in which to place "
			"key files\n");
	fprintf(stderr, "    -k: generate a TYPE=KEY key\n");
	fprintf(stderr, "    -n nametype: ZONE | HOST | ENTITY | USER | OTHER\n");
	fprintf(stderr, "        (DNSKEY generation defaults to ZONE\n");
	fprintf(stderr, "    -p protocol: default: 3 [dnssec]\n");
	fprintf(stderr, "    -t type: "
		"AUTHCONF | NOAUTHCONF | NOAUTH | NOCONF "
		"(default: AUTHCONF)\n");
	fprintf(stderr, "    -y: permit keys that might collide\n");
	fprintf(stderr, "    -v verbose level\n");
	fprintf(stderr, "Date options:\n");
	fprintf(stderr, "    -P date/[+-]offset: set key publication date\n");
	fprintf(stderr, "    -A date/[+-]offset: set key activation date\n");
	fprintf(stderr, "    -R date/[+-]offset: set key revocation date\n");
	fprintf(stderr, "    -I date/[+-]offset: set key inactivation date\n");
	fprintf(stderr, "    -D date/[+-]offset: set key deletion date\n");
	fprintf(stderr, "    -G: generate key only; do not set -P or -A\n");
	fprintf(stderr, "    -C: generate a backward-compatible key, omitting"
			" all dates\n");
	fprintf(stderr, "Output:\n");
	fprintf(stderr, "     K<name>+<alg>+<id>.key, "
			"K<name>+<alg>+<id>.private\n");

	exit (-1);
}

int
main(int argc, char **argv) {
	char		*algname = NULL, *nametype = NULL, *type = NULL;
	const char	*directory = NULL;
#ifdef USE_PKCS11
	const char	*engine = "pkcs11";
#else
	const char	*engine = NULL;
#endif
	char		*classname = NULL;
	char		*endp;
	dst_key_t	*key = NULL;
	dns_fixedname_t	fname;
	dns_name_t	*name;
	isc_uint16_t	flags = 0, kskflag = 0, revflag = 0;
	dns_secalg_t	alg;
	isc_boolean_t	oldstyle = ISC_FALSE;
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
	isc_stdtime_t	publish = 0, activate = 0, revoke = 0;
	isc_stdtime_t	inactive = 0, delete = 0;
	isc_stdtime_t	now;
	isc_boolean_t	setpub = ISC_FALSE, setact = ISC_FALSE;
	isc_boolean_t	setrev = ISC_FALSE, setinact = ISC_FALSE;
	isc_boolean_t	setdel = ISC_FALSE;
	isc_boolean_t	unsetpub = ISC_FALSE, unsetact = ISC_FALSE;
	isc_boolean_t	unsetrev = ISC_FALSE, unsetinact = ISC_FALSE;
	isc_boolean_t	unsetdel = ISC_FALSE;
	isc_boolean_t	genonly = ISC_FALSE;
	isc_boolean_t	use_nsec3 = ISC_FALSE;
	isc_boolean_t   avoid_collisions = ISC_TRUE;
	isc_boolean_t	exact;
	unsigned char	c;

	if (argc == 1)
		usage();

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	isc_stdtime_get(&now);

	while ((ch = isc_commandline_parse(argc, argv,
				"3a:Cc:E:f:K:kl:n:p:t:v:yFhGP:A:R:I:D:")) != -1)
	{
	    switch (ch) {
		case '3':
			use_nsec3 = ISC_TRUE;
			break;
		case 'a':
			algname = isc_commandline_argument;
			break;
		case 'C':
			oldstyle = ISC_TRUE;
			break;
		case 'c':
			classname = isc_commandline_argument;
			break;
		case 'E':
			engine = isc_commandline_argument;
			break;
		case 'f':
			c = (unsigned char)(isc_commandline_argument[0]);
			if (toupper(c) == 'K')
				kskflag = DNS_KEYFLAG_KSK;
			else if (toupper(c) == 'R')
				revflag = DNS_KEYFLAG_REVOKE;
			else
				fatal("unknown flag '%s'",
				      isc_commandline_argument);
			break;
		case 'K':
			directory = isc_commandline_argument;
			ret = try_dir(directory);
			if (ret != ISC_R_SUCCESS)
				fatal("cannot open directory %s: %s",
				      directory, isc_result_totext(ret));
			break;
		case 'k':
			options |= DST_TYPE_KEY;
			break;
		case 'l':
			label = isc_mem_strdup(mctx, isc_commandline_argument);
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
		case 'y':
			avoid_collisions = ISC_FALSE;
			break;
		case 'G':
			genonly = ISC_TRUE;
			break;
		case 'P':
			if (setpub || unsetpub)
				fatal("-P specified more than once");

			if (strcasecmp(isc_commandline_argument, "none")) {
				setpub = ISC_TRUE;
				publish = strtotime(isc_commandline_argument,
						    now, now);
			} else {
				unsetpub = ISC_TRUE;
			}
			break;
		case 'A':
			if (setact || unsetact)
				fatal("-A specified more than once");

			if (strcasecmp(isc_commandline_argument, "none")) {
				setact = ISC_TRUE;
				activate = strtotime(isc_commandline_argument,
						     now, now);
			} else {
				unsetact = ISC_TRUE;
			}
			break;
		case 'R':
			if (setrev || unsetrev)
				fatal("-R specified more than once");

			if (strcasecmp(isc_commandline_argument, "none")) {
				setrev = ISC_TRUE;
				revoke = strtotime(isc_commandline_argument,
						   now, now);
			} else {
				unsetrev = ISC_TRUE;
			}
			break;
		case 'I':
			if (setinact || unsetinact)
				fatal("-I specified more than once");

			if (strcasecmp(isc_commandline_argument, "none")) {
				setinact = ISC_TRUE;
				inactive = strtotime(isc_commandline_argument,
						     now, now);
			} else {
				unsetinact = ISC_TRUE;
			}
			break;
		case 'D':
			if (setdel || unsetdel)
				fatal("-D specified more than once");

			if (strcasecmp(isc_commandline_argument, "none")) {
				setdel = ISC_TRUE;
				delete = strtotime(isc_commandline_argument,
						   now, now);
			} else {
				unsetdel = ISC_TRUE;
			}
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
			usage();

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);
	ret = dst_lib_init2(mctx, ectx, engine,
			    ISC_ENTROPY_BLOCKING | ISC_ENTROPY_GOODONLY);
	if (ret != ISC_R_SUCCESS)
		fatal("could not initialize dst: %s",
		      isc_result_totext(ret));

	setup_logging(verbose, mctx, &log);

	if (label == NULL)
		fatal("the key label was not specified");
	if (argc < isc_commandline_index + 1)
		fatal("the key name was not specified");
	if (argc > isc_commandline_index + 1)
		fatal("extraneous arguments");

	if (strchr(label, ':') == NULL &&
	    engine != NULL && strlen(engine) != 0U) {
		char *l;
		int len;

		len = strlen(label) + strlen(engine) + 2;
		l = isc_mem_allocate(mctx, len);
		if (l == NULL)
			fatal("cannot allocate memory");
		snprintf(l, len, "%s:%s", engine, label);
		isc_mem_free(mctx, label);
		label = l;
	}

	if (algname == NULL) {
		if (use_nsec3)
			algname = strdup(DEFAULT_NSEC3_ALGORITHM);
		else
			algname = strdup(DEFAULT_ALGORITHM);
		if (verbose > 0)
			fprintf(stderr, "no algorithm specified; "
				"defaulting to %s\n", algname);
	}

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

	if (use_nsec3 &&
	    alg != DST_ALG_NSEC3DSA && alg != DST_ALG_NSEC3RSASHA1 &&
	    alg != DST_ALG_RSASHA256 && alg != DST_ALG_RSASHA512 &&
	    alg != DST_ALG_ECCGOST) {
		fatal("%s is incompatible with NSEC3; "
		      "do not use the -3 option", algname);
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

	if (directory == NULL)
		directory = ".";

	if ((options & DST_TYPE_KEY) != 0)  /* KEY */
		flags |= signatory;
	else if ((flags & DNS_KEYOWNER_ZONE) != 0) { /* DNSKEY */
		flags |= kskflag;
		flags |= revflag;
	}

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
	ret = dns_name_fromtext(name, &buf, dns_rootname, 0, NULL);
	if (ret != ISC_R_SUCCESS)
		fatal("invalid key name %s: %s", argv[isc_commandline_index],
		      isc_result_totext(ret));

	isc_buffer_init(&buf, filename, sizeof(filename) - 1);

	/* associate the key */
	ret = dst_key_fromlabel(name, alg, flags, protocol,
				rdclass, engine, label, NULL, mctx, &key);
	isc_entropy_stopcallbacksources(ectx);

	if (ret != ISC_R_SUCCESS) {
		char namestr[DNS_NAME_FORMATSIZE];
		char algstr[DNS_SECALG_FORMATSIZE];
		dns_name_format(name, namestr, sizeof(namestr));
		dns_secalg_format(alg, algstr, sizeof(algstr));
		fatal("failed to get key %s/%s: %s\n",
		      namestr, algstr, isc_result_totext(ret));
		/* NOTREACHED */
		exit(-1);
	}

	/*
	 * Set key timing metadata (unless using -C)
	 *
	 * Publish and activation dates are set to "now" by default, but
	 * can be overridden.  Creation date is always set to "now".
	 */
	if (!oldstyle) {
		dst_key_settime(key, DST_TIME_CREATED, now);

		if (genonly && (setpub || setact))
			fatal("cannot use -G together with -P or -A options");

		if (setpub)
			dst_key_settime(key, DST_TIME_PUBLISH, publish);
		else if (setact)
			dst_key_settime(key, DST_TIME_PUBLISH, activate);
		else if (!genonly && !unsetpub)
			dst_key_settime(key, DST_TIME_PUBLISH, now);

		if (setact)
			dst_key_settime(key, DST_TIME_ACTIVATE, activate);
		else if (!genonly && !unsetact)
			dst_key_settime(key, DST_TIME_ACTIVATE, now);

		if (setrev) {
			if (kskflag == 0)
				fprintf(stderr, "%s: warning: Key is "
					"not flagged as a KSK, but -R "
					"was used. Revoking a ZSK is "
					"legal, but undefined.\n",
					program);
			dst_key_settime(key, DST_TIME_REVOKE, revoke);
		}

		if (setinact)
			dst_key_settime(key, DST_TIME_INACTIVE, inactive);

		if (setdel)
			dst_key_settime(key, DST_TIME_DELETE, delete);
	} else {
		if (setpub || setact || setrev || setinact ||
		    setdel || unsetpub || unsetact ||
		    unsetrev || unsetinact || unsetdel || genonly)
			fatal("cannot use -C together with "
			      "-P, -A, -R, -I, -D, or -G options");
		/*
		 * Compatibility mode: Private-key-format
		 * should be set to 1.2.
		 */
		dst_key_setprivateformat(key, 1, 2);
	}

	/*
	 * Do not overwrite an existing key.  Warn LOUDLY if there
	 * is a risk of ID collision due to this key or another key
	 * being revoked.
	 */
	if (key_collision(dst_key_id(key), name, directory, alg, mctx, &exact))
	{
		isc_buffer_clear(&buf);
		ret = dst_key_buildfilename(key, 0, directory, &buf);
		if (exact)
			fatal("%s: %s already exists\n", program, filename);

		if (avoid_collisions)
			fatal("%s: %s could collide with another key upon "
			      "revokation\n", program, filename);

		fprintf(stderr, "%s: WARNING: Key %s could collide with "
				"another key upon revokation.  If you plan "
				"to revoke keys, destroy this key and "
				"generate a different one.\n",
				program, filename);
	}

	ret = dst_key_tofile(key, options, directory);
	if (ret != ISC_R_SUCCESS) {
		char keystr[DST_KEY_FORMATSIZE];
		dst_key_format(key, keystr, sizeof(keystr));
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
	isc_mem_free(mctx, label);
	isc_mem_destroy(&mctx);

	return (0);
}
