/*
 * Copyright (C) 2009-2012, 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
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
#include <unistd.h>

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

const char *program = "dnssec-revoke";
int verbose;

static isc_mem_t	*mctx = NULL;

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,	"    %s [options] keyfile\n\n", program);
	fprintf(stderr, "Version: %s\n", VERSION);
#ifdef USE_PKCS11
	fprintf(stderr, "    -E engine:    specify OpenSSL engine "
					   "(default \"pkcs11\")\n");
#else
	fprintf(stderr, "    -E engine:    specify OpenSSL engine\n");
#endif
	fprintf(stderr, "    -f:	   force overwrite\n");
	fprintf(stderr, "    -K directory: use directory for key files\n");
	fprintf(stderr, "    -h:	   help\n");
	fprintf(stderr, "    -r:	   remove old keyfiles after "
					   "creating revoked version\n");
	fprintf(stderr, "    -v level:	   set level of verbosity\n");
	fprintf(stderr, "    -V: print version information\n");
	fprintf(stderr, "Output:\n");
	fprintf(stderr, "     K<name>+<alg>+<new id>.key, "
			     "K<name>+<alg>+<new id>.private\n");

	exit (-1);
}

int
main(int argc, char **argv) {
	isc_result_t result;
#ifdef USE_PKCS11
	const char *engine = "pkcs11";
#else
	const char *engine = NULL;
#endif
	char *filename = NULL, *dir = NULL;
	char newname[1024], oldname[1024];
	char keystr[DST_KEY_FORMATSIZE];
	char *endp;
	int ch;
	isc_entropy_t *ectx = NULL;
	dst_key_t *key = NULL;
	isc_uint32_t flags;
	isc_buffer_t buf;
	isc_boolean_t force = ISC_FALSE;
	isc_boolean_t removefile = ISC_FALSE;
	isc_boolean_t id = ISC_FALSE;

	if (argc == 1)
		usage();

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("Out of memory");

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	while ((ch = isc_commandline_parse(argc, argv, "E:fK:rRhv:V")) != -1) {
		switch (ch) {
		    case 'E':
			engine = isc_commandline_argument;
			break;
		    case 'f':
			force = ISC_TRUE;
			break;
		    case 'K':
			/*
			 * We don't have to copy it here, but do it to
			 * simplify cleanup later
			 */
			dir = isc_mem_strdup(mctx, isc_commandline_argument);
			if (dir == NULL) {
				fatal("Failed to allocate memory for "
				      "directory");
			}
			break;
		    case 'r':
			removefile = ISC_TRUE;
			break;
		    case 'R':
			id = ISC_TRUE;
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
			/* Falls into */
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

	if (argc < isc_commandline_index + 1 ||
	    argv[isc_commandline_index] == NULL)
		fatal("The key file name was not specified");
	if (argc > isc_commandline_index + 1)
		fatal("Extraneous arguments");

	if (dir != NULL) {
		filename = argv[isc_commandline_index];
	} else {
		result = isc_file_splitpath(mctx, argv[isc_commandline_index],
					    &dir, &filename);
		if (result != ISC_R_SUCCESS)
			fatal("cannot process filename %s: %s",
			      argv[isc_commandline_index],
			      isc_result_totext(result));
		if (strcmp(dir, ".") == 0) {
			isc_mem_free(mctx, dir);
			dir = NULL;
		}
	}

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

	result = dst_key_fromnamedfile(filename, dir,
				       DST_TYPE_PUBLIC|DST_TYPE_PRIVATE,
				       mctx, &key);
	if (result != ISC_R_SUCCESS)
		fatal("Invalid keyfile name %s: %s",
		      filename, isc_result_totext(result));

	if (id) {
		fprintf(stdout, "%u\n", dst_key_rid(key));
		goto cleanup;
	}
	dst_key_format(key, keystr, sizeof(keystr));

	if (verbose > 2)
		fprintf(stderr, "%s: %s\n", program, keystr);

	if (force)
		set_keyversion(key);
	else
		check_keyversion(key, keystr);


	flags = dst_key_flags(key);
	if ((flags & DNS_KEYFLAG_REVOKE) == 0) {
		isc_stdtime_t now;

		if ((flags & DNS_KEYFLAG_KSK) == 0)
			fprintf(stderr, "%s: warning: Key is not flagged "
					"as a KSK. Revoking a ZSK is "
					"legal, but undefined.\n",
					program);

		isc_stdtime_get(&now);
		dst_key_settime(key, DST_TIME_REVOKE, now);

		dst_key_setflags(key, flags | DNS_KEYFLAG_REVOKE);

		isc_buffer_init(&buf, newname, sizeof(newname));
		dst_key_buildfilename(key, DST_TYPE_PUBLIC, dir, &buf);

		if (access(newname, F_OK) == 0 && !force) {
			fatal("Key file %s already exists; "
			      "use -f to force overwrite", newname);
		}

		result = dst_key_tofile(key, DST_TYPE_PUBLIC|DST_TYPE_PRIVATE,
					dir);
		if (result != ISC_R_SUCCESS) {
			dst_key_format(key, keystr, sizeof(keystr));
			fatal("Failed to write key %s: %s", keystr,
			      isc_result_totext(result));
		}

		isc_buffer_clear(&buf);
		dst_key_buildfilename(key, 0, dir, &buf);
		printf("%s\n", newname);

		/*
		 * Remove old key file, if told to (and if
		 * it isn't the same as the new file)
		 */
		if (removefile && dst_key_alg(key) != DST_ALG_RSAMD5) {
			isc_buffer_init(&buf, oldname, sizeof(oldname));
			dst_key_setflags(key, flags & ~DNS_KEYFLAG_REVOKE);
			dst_key_buildfilename(key, DST_TYPE_PRIVATE, dir, &buf);
			if (strcmp(oldname, newname) == 0)
				goto cleanup;
			(void)unlink(oldname);
			isc_buffer_clear(&buf);
			dst_key_buildfilename(key, DST_TYPE_PUBLIC, dir, &buf);
			(void)unlink(oldname);
		}
	} else {
		dst_key_format(key, keystr, sizeof(keystr));
		fatal("Key %s is already revoked", keystr);
	}

cleanup:
	dst_key_free(&key);
	dst_lib_destroy();
	isc_hash_destroy();
	cleanup_entropy(&ectx);
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	if (dir != NULL)
		isc_mem_free(mctx, dir);
	isc_mem_destroy(&mctx);

	return (0);
}
