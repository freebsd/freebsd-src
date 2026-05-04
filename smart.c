/*
 * Copyright (c) 2016-2026 Chuck Tuffli <chuck@tuffli.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#ifdef LIBXO
#include <libxo/xo.h>
#endif

#include "libsmart.h"

#define SMART_NAME "smart"
#define SMART_VERSION	"1.0.2"

extern bool do_debug;

static const char *pn;
bool do_debug = false;
static int debugset = 0;

static struct option opts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "threshold", no_argument, NULL, 't' },
	{ "hex", no_argument, NULL, 'x' },
	{ "attribute", required_argument, NULL, 'a' },
	{ "info", no_argument, NULL, 'i' },
	{ "version", no_argument, NULL, 'v' },
	{ "decode", no_argument, NULL, 'd' },
	{ "no-decode", no_argument, NULL, 'D' },
	{ "debug", no_argument, &debugset, 1 },
	{ NULL, 0, NULL, 0 }
};

static void
usage(const char *name)
{
	printf("Usage: %s [-htxi] [-a attribute[,attribute]...] <device name>\n", name);
	printf("\t-h, --help\n");
	printf("\t-t, --threshold : also print out the threshold values\n");
	printf("\t-x, --hex : print the values out in hexadecimal\n");
	printf("\t-a, --attribute : print specified attribute(s)\n");
	printf("\t-i, --info : print general device information\n");
	printf("\t-d, --decode: decode the attribute IDs\n");
	printf("\t-D, --no-decode: don't decode the attribute IDs\n");
	printf("\t-v, --version : print the version and copyright\n");
	printf("\t    --debug : output diagnostic information\n");
}

/*
 * Convert string to an integer
 *
 * Returns -1 on error, converted value otherwise
 */
static int32_t
get_val(char *attr, char **next)
{
	char *sep = NULL;
	long val;

	*next = NULL;

	val = strtol(attr, &sep, 0);
	if ((val == 0) && (errno != 0)) {
		printf("Error parsing attribute %s", attr);
		switch (errno) {
		case EINVAL:
			printf(" (not a number?)\n");
			break;
		case ERANGE:
			printf(" (value out of range)\n");
			break;
		default:
			printf("\n");
		}
		return -1;
	}

	if (val > INT32_MAX) {
		printf("Attribute value %ld too big\n", val);
		return -1;
	}

	*next = sep;
	return ((int32_t)val);
}

/*
 * Create a match specification from the given attribute
 *
 * Attribute format is
 *     <Page ID>:<Attribute ID>
 * where page and attribute IDs are integers. If the page ID is missing,
 * match the specified attribute ID on any page (i.e. -1). Valid forms are
 *    <int>:<int>
 *    :<int>
 *    <int>
 *
 * Returns 0 on success
 */
static int
add_match(smart_matches_t **matches, char *attr)
{
	char *next;
	int32_t page = -1, id;
	uint32_t count = 0;

	id = get_val(attr, &next);
	if (id < 0)
		return id;

	if (*next == ':') {
		page = id;
		id = get_val(next + 1, &next);
		if (id < 0)
			return id;
	}

	if (*matches == NULL) {
		*matches = calloc(1, sizeof(smart_matches_t) + sizeof(smart_match_t));
		if (*matches == NULL)
			return ENOMEM;
	} else {
		void *tmp;

		count = (*matches)->count;
		tmp = realloc(*matches, sizeof(smart_matches_t) + ((count + 1) * sizeof(smart_match_t)));
		if (tmp == NULL)
			return ENOMEM;
		*matches = tmp;
	}

	(*matches)->m[count].page = page;
	(*matches)->m[count].id = id;
	(*matches)->count++;
	return 0;
}

/*
 * Parse the comma separated list of attributes to match
 *
 * Caller frees memory allocated for the smart_matches_t pointer.
 *
 * Returns 0 on success
 */
static int
parse_matches(smart_matches_t **matches, char *attr)
{
	int res;

	if (attr[0] == '\0')
		return -1;

	while (*attr != '\0') {
		char *next;
		size_t len;

		if ((next = strchr(attr, ',')) == NULL) {
			len = strlen(attr);
			next = attr + len;
		} else {
			len = next - attr;
			next++;
		}

		if (len == 0) {
			printf("Malformed attribute %s\n", attr);
			return -1;
		}

		res = add_match(matches, attr);
		if (res)
			return res;

		attr = next;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	smart_h h;
	smart_map_t *sm = NULL;
	char *devname = NULL;
	int ch;
	bool do_thresh = false, do_hex = false, do_info = false, do_version = false,
	     do_descr;
	smart_matches_t *matches = NULL;
	int rc = EXIT_SUCCESS;

	/*
	 * By default, keep the original behavior (output numbers only) if
	 * invoked as smart. Otherwise, default to printing the human-friendly
	 * text descriptions.
	 */
	pn = getprogname();
	if (strcmp(pn, SMART_NAME) == 0)
		do_descr = false;
	else
		do_descr = true;

#ifdef LIBXO
	argc = xo_parse_args(argc, argv);
#endif

	while ((ch = getopt_long(argc, argv, "htxa:idDv", opts, NULL)) != -1) {
		switch (ch) {
		case 'h':
			usage(pn);
#ifdef LIBXO
			xo_finish();
#endif
			return EXIT_SUCCESS;
			break;
		case 't':
			do_thresh = true;
			break;
		case 'x':
			do_hex = true;
			break;
		case 'a':
			if (parse_matches(&matches, optarg)) {
				usage(pn);
				return EXIT_FAILURE;
			}
			break;
		case 'i':
			do_info = true;
			break;
		case 'd':
			do_descr = true;
			break;
		case 'D':
			do_descr = false;
			break;
		case 'v':
			do_version = true;
			break;
		case 0:
			if (debugset)
				do_debug = true;
			break;
		default:
			usage(pn);
#ifdef LIBXO
			xo_finish();
#endif
			return EXIT_FAILURE;
		}
	}

	if (do_version) {
		printf("%s, version %s\n", pn, SMART_VERSION);
		printf("Copyright (c) 2016-2026 Chuck Tuffli\n"
				"This is free software; see the source for copying conditions.\n");
		return EXIT_SUCCESS;
	}

	argc -= optind;
	argv += optind;

	devname = argv[0];

	if (!devname) {
		printf("no device specified\n");
		usage(pn);
#ifdef LIBXO
		xo_finish();
#endif
		return EXIT_FAILURE;
	}

	h = smart_open(SMART_PROTO_AUTO, argv[0]);

	if (h == NULL) {
		printf("device open failed %s\n", argv[0]);
#ifdef LIBXO
		xo_finish();
#endif
		return EXIT_FAILURE;
	}

#ifdef LIBXO
	xo_open_container("drive");
#endif

	if (do_info) {
		smart_print_device_info(h);
	}

	if (smart_supported(h)) {
		sm = smart_read(h);

		if (sm) {
			uint32_t flags = 0;

			if (do_hex)
				flags |= SMART_OPEN_F_HEX;
			if (do_thresh)
				flags |= SMART_OPEN_F_THRESH;
			if (do_descr)
				flags |= SMART_OPEN_F_DESCR;

			smart_print(h, sm, matches, flags);

			smart_free(sm);
		}
	} else {
		rc = EXIT_FAILURE;
	}
#ifdef LIBXO
	xo_finish();
#endif
	smart_close(h);

	return rc;
}
