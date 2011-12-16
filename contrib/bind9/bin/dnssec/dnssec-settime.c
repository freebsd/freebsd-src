/*
 * Copyright (C) 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssec-settime.c,v 1.28.16.3 2011-06-02 20:24:11 each Exp $ */

/*! \file */

#include <config.h>

#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/keyvalues.h>
#include <dns/result.h>

#include <dst/dst.h>

#include "dnssectool.h"

const char *program = "dnssec-settime";
int verbose;

static isc_mem_t	*mctx = NULL;

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,	"    %s [options] keyfile\n\n", program);
	fprintf(stderr, "Version: %s\n", VERSION);
	fprintf(stderr, "General options:\n");
#ifdef USE_PKCS11
	fprintf(stderr, "    -E engine:          specify OpenSSL engine "
						 "(default \"pkcs11\")\n");
#else
	fprintf(stderr, "    -E engine:          specify OpenSSL engine\n");
#endif
	fprintf(stderr, "    -f:                 force update of old-style "
						 "keys\n");
	fprintf(stderr, "    -K directory:       set key file location\n");
	fprintf(stderr, "    -v level:           set level of verbosity\n");
	fprintf(stderr, "    -h:                 help\n");
	fprintf(stderr, "Timing options:\n");
	fprintf(stderr, "    -P date/[+-]offset/none: set/unset key "
						     "publication date\n");
	fprintf(stderr, "    -A date/[+-]offset/none: set/unset key "
						     "activation date\n");
	fprintf(stderr, "    -R date/[+-]offset/none: set/unset key "
						     "revocation date\n");
	fprintf(stderr, "    -I date/[+-]offset/none: set/unset key "
						     "inactivation date\n");
	fprintf(stderr, "    -D date/[+-]offset/none: set/unset key "
						     "deletion date\n");
	fprintf(stderr, "Printing options:\n");
	fprintf(stderr, "    -p C/P/A/R/I/D/all: print a particular time "
						"value or values\n");
	fprintf(stderr, "    -u:                 print times in unix epoch "
						"format\n");
	fprintf(stderr, "Output:\n");
	fprintf(stderr, "     K<name>+<alg>+<new id>.key, "
			     "K<name>+<alg>+<new id>.private\n");

	exit (-1);
}

static void
printtime(dst_key_t *key, int type, const char *tag, isc_boolean_t epoch,
	  FILE *stream)
{
	isc_result_t result;
	const char *output = NULL;
	isc_stdtime_t when;

	if (tag != NULL)
		fprintf(stream, "%s: ", tag);

	result = dst_key_gettime(key, type, &when);
	if (result == ISC_R_NOTFOUND) {
		fprintf(stream, "UNSET\n");
	} else if (epoch) {
		fprintf(stream, "%d\n", (int) when);
	} else {
		time_t time = when;
		output = ctime(&time);
		fprintf(stream, "%s", output);
	}
}

int
main(int argc, char **argv) {
	isc_result_t	result;
#ifdef USE_PKCS11
	const char	*engine = "pkcs11";
#else
	const char	*engine = NULL;
#endif
	char		*filename = NULL, *directory = NULL;
	char		newname[1024];
	char		keystr[DST_KEY_FORMATSIZE];
	char		*endp, *p;
	int		ch;
	isc_entropy_t	*ectx = NULL;
	const char	*predecessor = NULL;
	dst_key_t	*prevkey = NULL;
	dst_key_t	*key = NULL;
	isc_buffer_t	buf;
	dns_name_t	*name = NULL;
	dns_secalg_t 	alg = 0;
	unsigned int 	size = 0;
	isc_uint16_t	flags = 0;
	int		prepub = -1;
	isc_stdtime_t	now;
	isc_stdtime_t	pub = 0, act = 0, rev = 0, inact = 0, del = 0;
	isc_boolean_t	setpub = ISC_FALSE, setact = ISC_FALSE;
	isc_boolean_t	setrev = ISC_FALSE, setinact = ISC_FALSE;
	isc_boolean_t	setdel = ISC_FALSE;
	isc_boolean_t	unsetpub = ISC_FALSE, unsetact = ISC_FALSE;
	isc_boolean_t	unsetrev = ISC_FALSE, unsetinact = ISC_FALSE;
	isc_boolean_t	unsetdel = ISC_FALSE;
	isc_boolean_t	printcreate = ISC_FALSE, printpub = ISC_FALSE;
	isc_boolean_t	printact = ISC_FALSE,  printrev = ISC_FALSE;
	isc_boolean_t	printinact = ISC_FALSE, printdel = ISC_FALSE;
	isc_boolean_t	force = ISC_FALSE;
	isc_boolean_t   epoch = ISC_FALSE;
	isc_boolean_t   changed = ISC_FALSE;

	if (argc == 1)
		usage();

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("Out of memory");

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	isc_stdtime_get(&now);

#define CMDLINE_FLAGS "A:D:E:fhI:i:K:P:p:R:S:uv:"
	while ((ch = isc_commandline_parse(argc, argv, CMDLINE_FLAGS)) != -1) {
		switch (ch) {
		case 'E':
			engine = isc_commandline_argument;
			break;
		case 'f':
			force = ISC_TRUE;
			break;
		case 'p':
			p = isc_commandline_argument;
			if (!strcasecmp(p, "all")) {
				printcreate = ISC_TRUE;
				printpub = ISC_TRUE;
				printact = ISC_TRUE;
				printrev = ISC_TRUE;
				printinact = ISC_TRUE;
				printdel = ISC_TRUE;
				break;
			}

			do {
				switch (*p++) {
				case 'C':
					printcreate = ISC_TRUE;
					break;
				case 'P':
					printpub = ISC_TRUE;
					break;
				case 'A':
					printact = ISC_TRUE;
					break;
				case 'R':
					printrev = ISC_TRUE;
					break;
				case 'I':
					printinact = ISC_TRUE;
					break;
				case 'D':
					printdel = ISC_TRUE;
					break;
				case ' ':
					break;
				default:
					usage();
					break;
				}
			} while (*p != '\0');
			break;
		case 'u':
			epoch = ISC_TRUE;
			break;
		case 'K':
			/*
			 * We don't have to copy it here, but do it to
			 * simplify cleanup later
			 */
			directory = isc_mem_strdup(mctx,
						   isc_commandline_argument);
			if (directory == NULL) {
				fatal("Failed to allocate memory for "
				      "directory");
			}
			break;
		case 'v':
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("-v must be followed by a number");
			break;
		case 'P':
			if (setpub || unsetpub)
				fatal("-P specified more than once");

			changed = ISC_TRUE;
			if (!strcasecmp(isc_commandline_argument, "none")) {
				unsetpub = ISC_TRUE;
			} else {
				setpub = ISC_TRUE;
				pub = strtotime(isc_commandline_argument,
						now, now);
			}
			break;
		case 'A':
			if (setact || unsetact)
				fatal("-A specified more than once");

			changed = ISC_TRUE;
			if (!strcasecmp(isc_commandline_argument, "none")) {
				unsetact = ISC_TRUE;
			} else {
				setact = ISC_TRUE;
				act = strtotime(isc_commandline_argument,
						now, now);
			}
			break;
		case 'R':
			if (setrev || unsetrev)
				fatal("-R specified more than once");

			changed = ISC_TRUE;
			if (!strcasecmp(isc_commandline_argument, "none")) {
				unsetrev = ISC_TRUE;
			} else {
				setrev = ISC_TRUE;
				rev = strtotime(isc_commandline_argument,
						now, now);
			}
			break;
		case 'I':
			if (setinact || unsetinact)
				fatal("-I specified more than once");

			changed = ISC_TRUE;
			if (!strcasecmp(isc_commandline_argument, "none")) {
				unsetinact = ISC_TRUE;
			} else {
				setinact = ISC_TRUE;
				inact = strtotime(isc_commandline_argument,
						now, now);
			}
			break;
		case 'D':
			if (setdel || unsetdel)
				fatal("-D specified more than once");

			changed = ISC_TRUE;
			if (!strcasecmp(isc_commandline_argument, "none")) {
				unsetdel = ISC_TRUE;
			} else {
				setdel = ISC_TRUE;
				del = strtotime(isc_commandline_argument,
						now, now);
			}
			break;
		case 'S':
			predecessor = isc_commandline_argument;
			break;
		case 'i':
			prepub = strtottl(isc_commandline_argument);
			break;
		case '?':
			if (isc_commandline_option != '?')
				fprintf(stderr, "%s: invalid argument -%c\n",
					program, isc_commandline_option);
			/* Falls into */
		case 'h':
			usage();

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	if (argc < isc_commandline_index + 1 ||
	    argv[isc_commandline_index] == NULL)
		fatal("The key file name was not specified");
	if (argc > isc_commandline_index + 1)
		fatal("Extraneous arguments");

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);
	result = isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE);
	if (result != ISC_R_SUCCESS)
		fatal("Could not initialize hash");
	result = dst_lib_init2(mctx, ectx, engine,
			       ISC_ENTROPY_BLOCKING | ISC_ENTROPY_GOODONLY);
	if (result != ISC_R_SUCCESS)
		fatal("Could not initialize dst: %s",
		      isc_result_totext(result));
	isc_entropy_stopcallbacksources(ectx);

	if (predecessor != NULL) {
		char keystr[DST_KEY_FORMATSIZE];
		isc_stdtime_t when;
		int major, minor;

		if (prepub == -1)
			prepub = (30 * 86400);

		if (setpub || unsetpub)
			fatal("-S and -P cannot be used together");
		if (setact || unsetact)
			fatal("-S and -A cannot be used together");

		result = dst_key_fromnamedfile(predecessor, directory,
					       DST_TYPE_PUBLIC |
					       DST_TYPE_PRIVATE,
					       mctx, &prevkey);
		if (result != ISC_R_SUCCESS)
			fatal("Invalid keyfile %s: %s",
			      filename, isc_result_totext(result));
		if (!dst_key_isprivate(prevkey))
			fatal("%s is not a private key", filename);

		name = dst_key_name(prevkey);
		alg = dst_key_alg(prevkey);
		size = dst_key_size(prevkey);
		flags = dst_key_flags(prevkey);

		dst_key_format(prevkey, keystr, sizeof(keystr));
		dst_key_getprivateformat(prevkey, &major, &minor);
		if (major != DST_MAJOR_VERSION || minor < DST_MINOR_VERSION)
			fatal("Predecessor has incompatible format "
			      "version %d.%d\n\t", major, minor);

		result = dst_key_gettime(prevkey, DST_TIME_ACTIVATE, &when);
		if (result != ISC_R_SUCCESS)
			fatal("Predecessor has no activation date. "
			      "You must set one before\n\t"
			      "generating a successor.");

		result = dst_key_gettime(prevkey, DST_TIME_INACTIVE, &act);
		if (result != ISC_R_SUCCESS)
			fatal("Predecessor has no inactivation date. "
			      "You must set one before\n\t"
			      "generating a successor.");

		pub = act - prepub;
		if (pub < now && prepub != 0)
			fatal("Predecessor will become inactive before the\n\t"
			      "prepublication period ends.  Either change "
			      "its inactivation date,\n\t"
			      "or use the -i option to set a shorter "
			      "prepublication interval.");

		result = dst_key_gettime(prevkey, DST_TIME_DELETE, &when);
		if (result != ISC_R_SUCCESS)
			fprintf(stderr, "%s: WARNING: Predecessor has no "
					"removal date;\n\t"
					"it will remain in the zone "
					"indefinitely after rollover.\n",
					program);

		changed = setpub = setact = ISC_TRUE;
		dst_key_free(&prevkey);
	} else {
		if (prepub < 0)
			prepub = 0;

		if (prepub > 0) {
			if (setpub && setact && (act - prepub) < pub)
				fatal("Activation and publication dates "
				      "are closer together than the\n\t"
				      "prepublication interval.");

			if (setpub && !setact) {
				setact = ISC_TRUE;
				act = pub + prepub;
			} else if (setact && !setpub) {
				setpub = ISC_TRUE;
				pub = act - prepub;
			}

			if ((act - prepub) < now)
				fatal("Time until activation is shorter "
				      "than the\n\tprepublication interval.");
		}
	}

	if (directory != NULL) {
		filename = argv[isc_commandline_index];
	} else {
		result = isc_file_splitpath(mctx, argv[isc_commandline_index],
					    &directory, &filename);
		if (result != ISC_R_SUCCESS)
			fatal("cannot process filename %s: %s",
			      argv[isc_commandline_index],
			      isc_result_totext(result));
	}

	result = dst_key_fromnamedfile(filename, directory,
				       DST_TYPE_PUBLIC | DST_TYPE_PRIVATE,
				       mctx, &key);
	if (result != ISC_R_SUCCESS)
		fatal("Invalid keyfile %s: %s",
		      filename, isc_result_totext(result));

	if (!dst_key_isprivate(key))
		fatal("%s is not a private key", filename);

	dst_key_format(key, keystr, sizeof(keystr));

	if (predecessor != NULL) {
		if (!dns_name_equal(name, dst_key_name(key)))
			fatal("Key name mismatch");
		if (alg != dst_key_alg(key))
			fatal("Key algorithm mismatch");
		if (size != dst_key_size(key))
			fatal("Key size mismatch");
		if (flags != dst_key_flags(key))
			fatal("Key flags mismatch");
	}

	if (force)
		set_keyversion(key);
	else
		check_keyversion(key, keystr);

	if (verbose > 2)
		fprintf(stderr, "%s: %s\n", program, keystr);

	/*
	 * Set time values.
	 */
	if (setpub)
		dst_key_settime(key, DST_TIME_PUBLISH, pub);
	else if (unsetpub)
		dst_key_unsettime(key, DST_TIME_PUBLISH);

	if (setact)
		dst_key_settime(key, DST_TIME_ACTIVATE, act);
	else if (unsetact)
		dst_key_unsettime(key, DST_TIME_ACTIVATE);

	if (setrev) {
		if ((dst_key_flags(key) & DNS_KEYFLAG_REVOKE) != 0)
			fprintf(stderr, "%s: warning: Key %s is already "
					"revoked; changing the revocation date "
					"will not affect this.\n",
					program, keystr);
		if ((dst_key_flags(key) & DNS_KEYFLAG_KSK) == 0)
			fprintf(stderr, "%s: warning: Key %s is not flagged as "
					"a KSK, but -R was used.  Revoking a "
					"ZSK is legal, but undefined.\n",
					program, keystr);
		dst_key_settime(key, DST_TIME_REVOKE, rev);
	} else if (unsetrev) {
		if ((dst_key_flags(key) & DNS_KEYFLAG_REVOKE) != 0)
			fprintf(stderr, "%s: warning: Key %s is already "
					"revoked; removing the revocation date "
					"will not affect this.\n",
					program, keystr);
		dst_key_unsettime(key, DST_TIME_REVOKE);
	}

	if (setinact)
		dst_key_settime(key, DST_TIME_INACTIVE, inact);
	else if (unsetinact)
		dst_key_unsettime(key, DST_TIME_INACTIVE);

	if (setdel)
		dst_key_settime(key, DST_TIME_DELETE, del);
	else if (unsetdel)
		dst_key_unsettime(key, DST_TIME_DELETE);

	/*
	 * No metadata changes were made but we're forcing an upgrade
	 * to the new format anyway: use "-P now -A now" as the default
	 */
	if (force && !changed) {
		dst_key_settime(key, DST_TIME_PUBLISH, now);
		dst_key_settime(key, DST_TIME_ACTIVATE, now);
		changed = ISC_TRUE;
	}

	/*
	 * Print out time values, if -p was used.
	 */
	if (printcreate)
		printtime(key, DST_TIME_CREATED, "Created", epoch, stdout);

	if (printpub)
		printtime(key, DST_TIME_PUBLISH, "Publish", epoch, stdout);

	if (printact)
		printtime(key, DST_TIME_ACTIVATE, "Activate", epoch, stdout);

	if (printrev)
		printtime(key, DST_TIME_REVOKE, "Revoke", epoch, stdout);

	if (printinact)
		printtime(key, DST_TIME_INACTIVE, "Inactive", epoch, stdout);

	if (printdel)
		printtime(key, DST_TIME_DELETE, "Delete", epoch, stdout);

	if (changed) {
		isc_buffer_init(&buf, newname, sizeof(newname));
		result = dst_key_buildfilename(key, DST_TYPE_PUBLIC, directory,
					       &buf);
		if (result != ISC_R_SUCCESS) {
			fatal("Failed to build public key filename: %s",
			      isc_result_totext(result));
		}

		result = dst_key_tofile(key, DST_TYPE_PUBLIC|DST_TYPE_PRIVATE,
					directory);
		if (result != ISC_R_SUCCESS) {
			dst_key_format(key, keystr, sizeof(keystr));
			fatal("Failed to write key %s: %s", keystr,
			      isc_result_totext(result));
		}

		printf("%s\n", newname);

		isc_buffer_clear(&buf);
		result = dst_key_buildfilename(key, DST_TYPE_PRIVATE, directory,
					       &buf);
		if (result != ISC_R_SUCCESS) {
			fatal("Failed to build private key filename: %s",
			      isc_result_totext(result));
		}
		printf("%s\n", newname);
	}

	dst_key_free(&key);
	dst_lib_destroy();
	isc_hash_destroy();
	cleanup_entropy(&ectx);
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_free(mctx, directory);
	isc_mem_destroy(&mctx);

	return (0);
}
