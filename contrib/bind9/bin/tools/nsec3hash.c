/*
 * Copyright (C) 2006, 2008, 2009, 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id$ */

#include <config.h>

#include <stdlib.h>
#include <stdarg.h>

#include <isc/base32.h>
#include <isc/buffer.h>
#include <isc/hex.h>
#include <isc/iterated_hash.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/types.h>

#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/nsec3.h>
#include <dns/types.h>

const char *program = "nsec3hash";

ISC_PLATFORM_NORETURN_PRE static void
fatal(const char *format, ...) ISC_PLATFORM_NORETURN_POST;

static void
fatal(const char *format, ...) {
	va_list args;

	fprintf(stderr, "%s: ", program);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}

static void
check_result(isc_result_t result, const char *message) {
	if (result != ISC_R_SUCCESS)
		fatal("%s: %s", message, isc_result_totext(result));
}

static void
usage() {
	printf("Usage: %s salt algorithm iterations domain\n", program);
	exit(1);
}

int
main(int argc, char **argv) {
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_buffer_t buffer;
	isc_region_t region;
	isc_result_t result;
	unsigned char hash[NSEC3_MAX_HASH_LENGTH];
	unsigned char salt[DNS_NSEC3_SALTSIZE];
	unsigned char text[1024];
	unsigned int hash_alg;
	unsigned int length;
	unsigned int iterations;
	unsigned int salt_length;

	if (argc != 5)
		usage();

	if (strcmp(argv[1], "-") == 0) {
		salt_length = 0;
		salt[0] = 0;
	} else {
		isc_buffer_init(&buffer, salt, sizeof(salt));
		result = isc_hex_decodestring(argv[1], &buffer);
		check_result(result, "isc_hex_decodestring(salt)");
		salt_length = isc_buffer_usedlength(&buffer);
		if (salt_length > DNS_NSEC3_SALTSIZE)
			fatal("salt too long");
	}
	hash_alg = atoi(argv[2]);
	if (hash_alg > 255U)
		fatal("hash algorithm too large");
	iterations = atoi(argv[3]);
	if (iterations > 0xffffU)
		fatal("iterations to large");

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	isc_buffer_init(&buffer, argv[4], strlen(argv[4]));
	isc_buffer_add(&buffer, strlen(argv[4]));
	result = dns_name_fromtext(name, &buffer, dns_rootname, 0, NULL);
	check_result(result, "dns_name_fromtext() failed");

	dns_name_downcase(name, name, NULL);
	length = isc_iterated_hash(hash, hash_alg, iterations,  salt,
				   salt_length, name->ndata, name->length);
	if (length == 0)
		fatal("isc_iterated_hash failed");
	region.base = hash;
	region.length = length;
	isc_buffer_init(&buffer, text, sizeof(text));
	isc_base32hex_totext(&region, 1, "", &buffer);
	fprintf(stdout, "%.*s (salt=%s, hash=%u, iterations=%u)\n",
		(int)isc_buffer_usedlength(&buffer), text, argv[1], hash_alg, iterations);
	return(0);
}
