/*
 * Portions Copyright (C) 2004-2011  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssec-keygen.c,v 1.115.14.4 2011/11/30 00:51:38 marka Exp $ */

/*! \file */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/region.h>
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

const char *program = "dnssec-keygen";
int verbose;

#define DEFAULT_ALGORITHM "RSASHA1"
#define DEFAULT_NSEC3_ALGORITHM "NSEC3RSASHA1"

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void progress(int p);

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "    %s [options] name\n\n", program);
	fprintf(stderr, "Version: %s\n", VERSION);
	fprintf(stderr, "    name: owner of the key\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "    -K <directory>: write keys into directory\n");
	fprintf(stderr, "    -a <algorithm>:\n");
	fprintf(stderr, "        RSA | RSAMD5 | DSA | RSASHA1 | NSEC3RSASHA1"
				" | NSEC3DSA |\n");
	fprintf(stderr, "        RSASHA256 | RSASHA512 | ECCGOST |\n");
	fprintf(stderr, "        DH | HMAC-MD5 | HMAC-SHA1 | HMAC-SHA224 | "
				"HMAC-SHA256 | \n");
	fprintf(stderr, "        HMAC-SHA384 | HMAC-SHA512\n");
	fprintf(stderr, "       (default: RSASHA1, or "
			       "NSEC3RSASHA1 if using -3)\n");
	fprintf(stderr, "    -3: use NSEC3-capable algorithm\n");
	fprintf(stderr, "    -b <key size in bits>:\n");
	fprintf(stderr, "        RSAMD5:\t[512..%d]\n", MAX_RSA);
	fprintf(stderr, "        RSASHA1:\t[512..%d]\n", MAX_RSA);
	fprintf(stderr, "        NSEC3RSASHA1:\t[512..%d]\n", MAX_RSA);
	fprintf(stderr, "        RSASHA256:\t[512..%d]\n", MAX_RSA);
	fprintf(stderr, "        RSASHA512:\t[1024..%d]\n", MAX_RSA);
	fprintf(stderr, "        DH:\t\t[128..4096]\n");
	fprintf(stderr, "        DSA:\t\t[512..1024] and divisible by 64\n");
	fprintf(stderr, "        NSEC3DSA:\t[512..1024] and divisible "
				"by 64\n");
	fprintf(stderr, "        ECCGOST:\tignored\n");
	fprintf(stderr, "        HMAC-MD5:\t[1..512]\n");
	fprintf(stderr, "        HMAC-SHA1:\t[1..160]\n");
	fprintf(stderr, "        HMAC-SHA224:\t[1..224]\n");
	fprintf(stderr, "        HMAC-SHA256:\t[1..256]\n");
	fprintf(stderr, "        HMAC-SHA384:\t[1..384]\n");
	fprintf(stderr, "        HMAC-SHA512:\t[1..512]\n");
	fprintf(stderr, "        (if using the default algorithm, key size\n"
			"        defaults to 2048 for KSK, or 1024 for all "
			"others)\n");
	fprintf(stderr, "    -n <nametype>: ZONE | HOST | ENTITY | "
					    "USER | OTHER\n");
	fprintf(stderr, "        (DNSKEY generation defaults to ZONE)\n");
	fprintf(stderr, "    -c <class>: (default: IN)\n");
	fprintf(stderr, "    -d <digest bits> (0 => max, default)\n");
#ifdef USE_PKCS11
	fprintf(stderr, "    -E <engine name> (default \"pkcs11\")\n");
#else
	fprintf(stderr, "    -E <engine name>\n");
#endif
	fprintf(stderr, "    -e: use large exponent (RSAMD5/RSASHA1 only)\n");
	fprintf(stderr, "    -f <keyflag>: KSK | REVOKE\n");
	fprintf(stderr, "    -g <generator>: use specified generator "
			"(DH only)\n");
	fprintf(stderr, "    -p <protocol>: (default: 3 [dnssec])\n");
	fprintf(stderr, "    -s <strength>: strength value this key signs DNS "
			"records with (default: 0)\n");
	fprintf(stderr, "    -T <rrtype>: DNSKEY | KEY (default: DNSKEY; "
			"use KEY for SIG(0))\n");
	fprintf(stderr, "        ECCGOST:\tignored\n");
	fprintf(stderr, "    -t <type>: "
			"AUTHCONF | NOAUTHCONF | NOAUTH | NOCONF "
			"(default: AUTHCONF)\n");
	fprintf(stderr, "    -r <randomdev>: a file containing random data\n");

	fprintf(stderr, "    -h: print usage and exit\n");
	fprintf(stderr, "    -m <memory debugging mode>:\n");
	fprintf(stderr, "       usage | trace | record | size | mctx\n");
	fprintf(stderr, "    -v <level>: set verbosity level (0 - 10)\n");
	fprintf(stderr, "Timing options:\n");
	fprintf(stderr, "    -P date/[+-]offset/none: set key publication date "
						"(default: now)\n");
	fprintf(stderr, "    -A date/[+-]offset/none: set key activation date "
						"(default: now)\n");
	fprintf(stderr, "    -R date/[+-]offset/none: set key "
						     "revocation date\n");
	fprintf(stderr, "    -I date/[+-]offset/none: set key "
						     "inactivation date\n");
	fprintf(stderr, "    -D date/[+-]offset/none: set key deletion date\n");
	fprintf(stderr, "    -G: generate key only; do not set -P or -A\n");
	fprintf(stderr, "    -C: generate a backward-compatible key, omitting "
			"all dates\n");
	fprintf(stderr, "    -S <key>: generate a successor to an existing "
				      "key\n");
	fprintf(stderr, "    -i <interval>: prepublication interval for "
					   "successor key "
					   "(default: 30 days)\n");
	fprintf(stderr, "Output:\n");
	fprintf(stderr, "     K<name>+<alg>+<id>.key, "
			"K<name>+<alg>+<id>.private\n");

	exit (-1);
}

static isc_boolean_t
dsa_size_ok(int size) {
	return (ISC_TF(size >= 512 && size <= 1024 && size % 64 == 0));
}

static void
progress(int p)
{
	char c = '*';

	switch (p) {
	case 0:
		c = '.';
		break;
	case 1:
		c = '+';
		break;
	case 2:
		c = '*';
		break;
	case 3:
		c = ' ';
		break;
	default:
		break;
	}
	(void) putc(c, stderr);
	(void) fflush(stderr);
}

int
main(int argc, char **argv) {
	char		*algname = NULL, *freeit = NULL;
	char		*nametype = NULL, *type = NULL;
	char		*classname = NULL;
	char		*endp;
	dst_key_t	*key = NULL;
	dns_fixedname_t	fname;
	dns_name_t	*name;
	isc_uint16_t	flags = 0, kskflag = 0, revflag = 0;
	dns_secalg_t	alg;
	isc_boolean_t	conflict = ISC_FALSE, null_key = ISC_FALSE;
	isc_boolean_t	oldstyle = ISC_FALSE;
	isc_mem_t	*mctx = NULL;
	int		ch, rsa_exp = 0, generator = 0, param = 0;
	int		protocol = -1, size = -1, signatory = 0;
	isc_result_t	ret;
	isc_textregion_t r;
	char		filename[255];
	const char	*directory = NULL;
	const char	*predecessor = NULL;
	dst_key_t	*prevkey = NULL;
	isc_buffer_t	buf;
	isc_log_t	*log = NULL;
	isc_entropy_t	*ectx = NULL;
#ifdef USE_PKCS11
	const char	*engine = "pkcs11";
#else
	const char	*engine = NULL;
#endif
	dns_rdataclass_t rdclass;
	int		options = DST_TYPE_PRIVATE | DST_TYPE_PUBLIC;
	int		dbits = 0;
	isc_boolean_t	use_default = ISC_FALSE, use_nsec3 = ISC_FALSE;
	isc_stdtime_t	publish = 0, activate = 0, revoke = 0;
	isc_stdtime_t	inactive = 0, delete = 0;
	isc_stdtime_t	now;
	int		prepub = -1;
	isc_boolean_t	setpub = ISC_FALSE, setact = ISC_FALSE;
	isc_boolean_t	setrev = ISC_FALSE, setinact = ISC_FALSE;
	isc_boolean_t	setdel = ISC_FALSE;
	isc_boolean_t	unsetpub = ISC_FALSE, unsetact = ISC_FALSE;
	isc_boolean_t	unsetrev = ISC_FALSE, unsetinact = ISC_FALSE;
	isc_boolean_t	unsetdel = ISC_FALSE;
	isc_boolean_t	genonly = ISC_FALSE;
	isc_boolean_t	quiet = ISC_FALSE;
	isc_boolean_t	show_progress = ISC_FALSE;
	unsigned char	c;

	if (argc == 1)
		usage();

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	/*
	 * Process memory debugging argument first.
	 */
#define CMDLINE_FLAGS "3A:a:b:Cc:D:d:E:eFf:Gg:hI:i:K:km:n:P:p:qR:r:S:s:T:t:v:"
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

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	isc_stdtime_get(&now);

	while ((ch = isc_commandline_parse(argc, argv, CMDLINE_FLAGS)) != -1) {
	    switch (ch) {
		case '3':
			use_nsec3 = ISC_TRUE;
			break;
		case 'a':
			algname = isc_commandline_argument;
			break;
		case 'b':
			size = strtol(isc_commandline_argument, &endp, 10);
			if (*endp != '\0' || size < 0)
				fatal("-b requires a non-negative number");
			break;
		case 'C':
			oldstyle = ISC_TRUE;
			break;
		case 'c':
			classname = isc_commandline_argument;
			break;
		case 'd':
			dbits = strtol(isc_commandline_argument, &endp, 10);
			if (*endp != '\0' || dbits < 0)
				fatal("-d requires a non-negative number");
			break;
		case 'E':
			engine = isc_commandline_argument;
			break;
		case 'e':
			rsa_exp = 1;
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
		case 'g':
			generator = strtol(isc_commandline_argument,
					   &endp, 10);
			if (*endp != '\0' || generator <= 0)
				fatal("-g requires a positive number");
			break;
		case 'K':
			directory = isc_commandline_argument;
			ret = try_dir(directory);
			if (ret != ISC_R_SUCCESS)
				fatal("cannot open directory %s: %s",
				      directory, isc_result_totext(ret));
			break;
		case 'k':
			fatal("The -k option has been deprecated.\n"
			      "To generate a key-signing key, use -f KSK.\n"
			      "To generate a key with TYPE=KEY, use -T KEY.\n");
			break;
		case 'n':
			nametype = isc_commandline_argument;
			break;
		case 'm':
			break;
		case 'p':
			protocol = strtol(isc_commandline_argument, &endp, 10);
			if (*endp != '\0' || protocol < 0 || protocol > 255)
				fatal("-p must be followed by a number "
				      "[0..255]");
			break;
		case 'q':
			quiet = ISC_TRUE;
			break;
		case 'r':
			setup_entropy(mctx, isc_commandline_argument, &ectx);
			break;
		case 's':
			signatory = strtol(isc_commandline_argument,
					   &endp, 10);
			if (*endp != '\0' || signatory < 0 || signatory > 15)
				fatal("-s must be followed by a number "
				      "[0..15]");
			break;
		case 'T':
			if (strcasecmp(isc_commandline_argument, "KEY") == 0)
				options |= DST_TYPE_KEY;
			else if (strcasecmp(isc_commandline_argument,
				 "DNSKEY") == 0)
				/* default behavior */
				;
			else
				fatal("unknown type '%s'",
				      isc_commandline_argument);
			break;
		case 't':
			type = isc_commandline_argument;
			break;
		case 'v':
			endp = NULL;
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("-v must be followed by a number");
			break;
		case 'z':
			/* already the default */
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
		case 'S':
			predecessor = isc_commandline_argument;
			break;
		case 'i':
			prepub = strtottl(isc_commandline_argument);
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

	if (!isatty(0))
		quiet = ISC_TRUE;

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);
	ret = dst_lib_init2(mctx, ectx, engine,
			    ISC_ENTROPY_BLOCKING | ISC_ENTROPY_GOODONLY);
	if (ret != ISC_R_SUCCESS)
		fatal("could not initialize dst: %s",
		      isc_result_totext(ret));

	setup_logging(verbose, mctx, &log);

	if (predecessor == NULL) {
		if (prepub == -1)
			prepub = 0;

		if (argc < isc_commandline_index + 1)
			fatal("the key name was not specified");
		if (argc > isc_commandline_index + 1)
			fatal("extraneous arguments");

		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		isc_buffer_init(&buf, argv[isc_commandline_index],
				strlen(argv[isc_commandline_index]));
		isc_buffer_add(&buf, strlen(argv[isc_commandline_index]));
		ret = dns_name_fromtext(name, &buf, dns_rootname, 0, NULL);
		if (ret != ISC_R_SUCCESS)
			fatal("invalid key name %s: %s",
			      argv[isc_commandline_index],
			      isc_result_totext(ret));

		if (algname == NULL) {
			use_default = ISC_TRUE;
			if (use_nsec3)
				algname = strdup(DEFAULT_NSEC3_ALGORITHM);
			else
				algname = strdup(DEFAULT_ALGORITHM);
			if (algname == NULL)
				fatal("strdup failed");
			freeit = algname;
			if (verbose > 0)
				fprintf(stderr, "no algorithm specified; "
						"defaulting to %s\n", algname);
		}

		if (strcasecmp(algname, "RSA") == 0) {
			fprintf(stderr, "The use of RSA (RSAMD5) is not "
					"recommended.\nIf you still wish to "
					"use RSA (RSAMD5) please specify "
					"\"-a RSAMD5\"\n");
			return (1);
		} else if (strcasecmp(algname, "HMAC-MD5") == 0)
			alg = DST_ALG_HMACMD5;
		else if (strcasecmp(algname, "HMAC-SHA1") == 0)
			alg = DST_ALG_HMACSHA1;
		else if (strcasecmp(algname, "HMAC-SHA224") == 0)
			alg = DST_ALG_HMACSHA224;
		else if (strcasecmp(algname, "HMAC-SHA256") == 0)
			alg = DST_ALG_HMACSHA256;
		else if (strcasecmp(algname, "HMAC-SHA384") == 0)
			alg = DST_ALG_HMACSHA384;
		else if (strcasecmp(algname, "HMAC-SHA512") == 0)
			alg = DST_ALG_HMACSHA512;
		else {
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
		    alg != DST_ALG_RSASHA256 && alg!= DST_ALG_RSASHA512 &&
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
				flags |= (DNS_KEYTYPE_NOAUTH |
					  DNS_KEYTYPE_NOCONF);
				if (size < 0)
					size = 0;
			}
			else if (strcasecmp(type, "AUTHCONF") == 0)
				/* nothing */;
			else
				fatal("invalid type %s", type);
		}

		if (size < 0) {
			if (use_default) {
				if ((kskflag & DNS_KEYFLAG_KSK) != 0)
					size = 2048;
				else
					size = 1024;
				if (verbose > 0)
					fprintf(stderr, "key size not "
							"specified; defaulting "
							"to %d\n", size);
			} else if (alg != DST_ALG_ECCGOST)
				fatal("key size not specified (-b option)");
		}

		if (!oldstyle && prepub > 0) {
			if (setpub && setact && (activate - prepub) < publish)
				fatal("Activation and publication dates "
				      "are closer together than the\n\t"
				      "prepublication interval.");

			if (!setpub && !setact) {
				setpub = setact = ISC_TRUE;
				publish = now;
				activate = now + prepub;
			} else if (setpub && !setact) {
				setact = ISC_TRUE;
				activate = publish + prepub;
			} else if (setact && !setpub) {
				setpub = ISC_TRUE;
				publish = activate - prepub;
			}

			if ((activate - prepub) < now)
				fatal("Time until activation is shorter "
				      "than the\n\tprepublication interval.");
		}
	} else {
		char keystr[DST_KEY_FORMATSIZE];
		isc_stdtime_t when;
		int major, minor;

		if (prepub == -1)
			prepub = (30 * 86400);

		if (algname != NULL)
			fatal("-S and -a cannot be used together");
		if (size >= 0)
			fatal("-S and -b cannot be used together");
		if (nametype != NULL)
			fatal("-S and -n cannot be used together");
		if (type != NULL)
			fatal("-S and -t cannot be used together");
		if (setpub || unsetpub)
			fatal("-S and -P cannot be used together");
		if (setact || unsetact)
			fatal("-S and -A cannot be used together");
		if (use_nsec3)
			fatal("-S and -3 cannot be used together");
		if (oldstyle)
			fatal("-S and -C cannot be used together");
		if (genonly)
			fatal("-S and -G cannot be used together");

		ret = dst_key_fromnamedfile(predecessor, directory,
					    DST_TYPE_PUBLIC | DST_TYPE_PRIVATE,
					    mctx, &prevkey);
		if (ret != ISC_R_SUCCESS)
			fatal("Invalid keyfile %s: %s",
			      filename, isc_result_totext(ret));
		if (!dst_key_isprivate(prevkey))
			fatal("%s is not a private key", filename);

		name = dst_key_name(prevkey);
		alg = dst_key_alg(prevkey);
		size = dst_key_size(prevkey);
		flags = dst_key_flags(prevkey);

		dst_key_format(prevkey, keystr, sizeof(keystr));
		dst_key_getprivateformat(prevkey, &major, &minor);
		if (major != DST_MAJOR_VERSION || minor < DST_MINOR_VERSION)
			fatal("Key %s has incompatible format version %d.%d\n\t"
			      "It is not possible to generate a successor key.",
			      keystr, major, minor);

		ret = dst_key_gettime(prevkey, DST_TIME_ACTIVATE, &when);
		if (ret != ISC_R_SUCCESS)
			fatal("Key %s has no activation date.\n\t"
			      "You must use dnssec-settime -A to set one "
			      "before generating a successor.", keystr);

		ret = dst_key_gettime(prevkey, DST_TIME_INACTIVE, &activate);
		if (ret != ISC_R_SUCCESS)
			fatal("Key %s has no inactivation date.\n\t"
			      "You must use dnssec-settime -I to set one "
			      "before generating a successor.", keystr);

		publish = activate - prepub;
		if (publish < now)
			fatal("Key %s becomes inactive\n\t"
			      "sooner than the prepublication period "
			      "for the new key ends.\n\t"
			      "Either change the inactivation date with "
			      "dnssec-settime -I,\n\t"
			      "or use the -i option to set a shorter "
			      "prepublication interval.", keystr);

		ret = dst_key_gettime(prevkey, DST_TIME_DELETE, &when);
		if (ret != ISC_R_SUCCESS)
			fprintf(stderr, "%s: WARNING: Key %s has no removal "
					"date;\n\t it will remain in the zone "
					"indefinitely after rollover.\n\t "
					"You can use dnssec-settime -D to "
					"change this.\n", program, keystr);

		setpub = setact = ISC_TRUE;
	}

	switch (alg) {
	case DNS_KEYALG_RSAMD5:
	case DNS_KEYALG_RSASHA1:
	case DNS_KEYALG_NSEC3RSASHA1:
	case DNS_KEYALG_RSASHA256:
		if (size != 0 && (size < 512 || size > MAX_RSA))
			fatal("RSA key size %d out of range", size);
		break;
	case DNS_KEYALG_RSASHA512:
		if (size != 0 && (size < 1024 || size > MAX_RSA))
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
	case DST_ALG_ECCGOST:
		break;
	case DST_ALG_HMACMD5:
		options |= DST_TYPE_KEY;
		if (size < 1 || size > 512)
			fatal("HMAC-MD5 key size %d out of range", size);
		if (dbits != 0 && (dbits < 80 || dbits > 128))
			fatal("HMAC-MD5 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-MD5 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA1:
		options |= DST_TYPE_KEY;
		if (size < 1 || size > 160)
			fatal("HMAC-SHA1 key size %d out of range", size);
		if (dbits != 0 && (dbits < 80 || dbits > 160))
			fatal("HMAC-SHA1 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA1 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA224:
		options |= DST_TYPE_KEY;
		if (size < 1 || size > 224)
			fatal("HMAC-SHA224 key size %d out of range", size);
		if (dbits != 0 && (dbits < 112 || dbits > 224))
			fatal("HMAC-SHA224 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA224 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA256:
		options |= DST_TYPE_KEY;
		if (size < 1 || size > 256)
			fatal("HMAC-SHA256 key size %d out of range", size);
		if (dbits != 0 && (dbits < 128 || dbits > 256))
			fatal("HMAC-SHA256 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA256 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA384:
		options |= DST_TYPE_KEY;
		if (size < 1 || size > 384)
			fatal("HMAC-384 key size %d out of range", size);
		if (dbits != 0 && (dbits < 192 || dbits > 384))
			fatal("HMAC-SHA384 digest bits %d out of range", dbits);
		if ((dbits % 8) != 0)
			fatal("HMAC-SHA384 digest bits %d not divisible by 8",
			      dbits);
		break;
	case DST_ALG_HMACSHA512:
		options |= DST_TYPE_KEY;
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
	      alg == DNS_KEYALG_NSEC3RSASHA1 || alg == DNS_KEYALG_RSASHA256 ||
	      alg == DNS_KEYALG_RSASHA512 || alg == DST_ALG_ECCGOST) &&
	    rsa_exp != 0)
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

	if (directory == NULL)
		directory = ".";

	if ((options & DST_TYPE_KEY) != 0)  /* KEY / HMAC */
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

	switch(alg) {
	case DNS_KEYALG_RSAMD5:
	case DNS_KEYALG_RSASHA1:
	case DNS_KEYALG_NSEC3RSASHA1:
	case DNS_KEYALG_RSASHA256:
	case DNS_KEYALG_RSASHA512:
		param = rsa_exp;
		show_progress = ISC_TRUE;
		break;

	case DNS_KEYALG_DH:
		param = generator;
		break;

	case DNS_KEYALG_DSA:
	case DNS_KEYALG_NSEC3DSA:
	case DST_ALG_ECCGOST:
		show_progress = ISC_TRUE;
		/* fall through */

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

		if (!quiet && show_progress) {
			fprintf(stderr, "Generating key pair.");
			ret = dst_key_generate2(name, alg, size, param, flags,
						protocol, rdclass, mctx, &key,
						&progress);
			putc('\n', stderr);
			fflush(stderr);
		} else {
			ret = dst_key_generate2(name, alg, size, param, flags,
						protocol, rdclass, mctx, &key,
						NULL);
		}

		isc_entropy_stopcallbacksources(ectx);

		if (ret != ISC_R_SUCCESS) {
			char namestr[DNS_NAME_FORMATSIZE];
			char algstr[DNS_SECALG_FORMATSIZE];
			dns_name_format(name, namestr, sizeof(namestr));
			dns_secalg_format(alg, algstr, sizeof(algstr));
			fatal("failed to generate key %s/%s: %s\n",
			      namestr, algstr, isc_result_totext(ret));
			/* NOTREACHED */
			exit(-1);
		}

		dst_key_setbits(key, dbits);

		/*
		 * Set key timing metadata (unless using -C)
		 *
		 * Creation date is always set to "now".
		 *
		 * For a new key without an explicit predecessor, publish
		 * and activation dates are set to "now" by default, but
		 * can both be overridden.
		 *
		 * For a successor key, activation is set to match the
		 * predecessor's inactivation date.  Publish is set to 30
		 * days earlier than that (XXX: this should be configurable).
		 * If either of the resulting dates are in the past, that's
		 * an error; the inactivation date of the predecessor key
		 * must be updated before a successor key can be created.
		 */
		if (!oldstyle) {
			dst_key_settime(key, DST_TIME_CREATED, now);

			if (genonly && (setpub || setact))
				fatal("cannot use -G together with "
				      "-P or -A options");

			if (setpub)
				dst_key_settime(key, DST_TIME_PUBLISH, publish);
			else if (setact)
				dst_key_settime(key, DST_TIME_PUBLISH,
						activate);
			else if (!genonly && !unsetpub)
				dst_key_settime(key, DST_TIME_PUBLISH, now);

			if (setact)
				dst_key_settime(key, DST_TIME_ACTIVATE,
						activate);
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
				dst_key_settime(key, DST_TIME_INACTIVE,
						inactive);

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
		 * Do not overwrite an existing key, or create a key
		 * if there is a risk of ID collision due to this key
		 * or another key being revoked.
		 */
		if (key_collision(key, name, directory, mctx, NULL)) {
			conflict = ISC_TRUE;
			if (null_key) {
				dst_key_free(&key);
				break;
			}

			if (verbose > 0) {
				isc_buffer_clear(&buf);
				ret = dst_key_buildfilename(key, 0,
							    directory, &buf);
				if (ret == ISC_R_SUCCESS)
					fprintf(stderr,
						"%s: %s already exists, or "
						"might collide with another "
						"key upon revokation.  "
						"Generating a new key\n",
						program, filename);
			}

			dst_key_free(&key);
		}
	} while (conflict == ISC_TRUE);

	if (conflict)
		fatal("cannot generate a null key due to possible key ID "
		      "collision");

	ret = dst_key_tofile(key, options, directory);
	if (ret != ISC_R_SUCCESS) {
		char keystr[DST_KEY_FORMATSIZE];
		dst_key_format(key, keystr, sizeof(keystr));
		fatal("failed to write key %s: %s\n", keystr,
		      isc_result_totext(ret));
	}

	isc_buffer_clear(&buf);
	ret = dst_key_buildfilename(key, 0, NULL, &buf);
	if (ret != ISC_R_SUCCESS)
		fatal("dst_key_buildfilename returned: %s\n",
		      isc_result_totext(ret));
	printf("%s\n", filename);
	dst_key_free(&key);
	if (prevkey != NULL)
		dst_key_free(&prevkey);

	cleanup_logging(&log);
	cleanup_entropy(&ectx);
	dst_lib_destroy();
	dns_name_destroy();
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	if (freeit != NULL)
		free(freeit);

	return (0);
}
