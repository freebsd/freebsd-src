/*
 * Copyright (c) 2017 Konsulko Group Inc. All rights reserved.
 *
 * Author:
 *	 Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <libfdt.h>

#include "util.h"

/* Usage related data. */
static const char usage_synopsis[] =
	"apply a number of overlays to a base blob\n"
	"	fdtoverlay <options> [<overlay.dtbo> [<overlay.dtbo>]]\n"
	"\n"
	USAGE_TYPE_MSG;
static const char usage_short_opts[] = "i:o:v" USAGE_COMMON_SHORT_OPTS;
static struct option const usage_long_opts[] = {
	{"input",            required_argument, NULL, 'i'},
	{"output",	     required_argument, NULL, 'o'},
	{"verbose",	           no_argument, NULL, 'v'},
	USAGE_COMMON_LONG_OPTS,
};
static const char * const usage_opts_help[] = {
	"Input base DT blob",
	"Output DT blob",
	"Verbose messages",
	USAGE_COMMON_OPTS_HELP
};

int verbose = 0;

static int do_fdtoverlay(const char *input_filename,
			 const char *output_filename,
			 int argc, char *argv[])
{
	char *blob = NULL;
	char **ovblob = NULL;
	off_t blob_len, ov_len, total_len;
	int i, ret = -1;

	blob = utilfdt_read_len(input_filename, &blob_len);
	if (!blob) {
		fprintf(stderr, "\nFailed to read base blob %s\n",
				input_filename);
		goto out_err;
	}
	if (fdt_totalsize(blob) > blob_len) {
		fprintf(stderr,
 "\nBase blob is incomplete (%lu / %" PRIu32 " bytes read)\n",
			(unsigned long)blob_len, fdt_totalsize(blob));
		goto out_err;
	}
	ret = 0;

	/* allocate blob pointer array */
	ovblob = malloc(sizeof(*ovblob) * argc);
	memset(ovblob, 0, sizeof(*ovblob) * argc);

	/* read and keep track of the overlay blobs */
	total_len = 0;
	for (i = 0; i < argc; i++) {
		ovblob[i] = utilfdt_read_len(argv[i], &ov_len);
		if (!ovblob[i]) {
			fprintf(stderr, "\nFailed to read overlay %s\n",
					argv[i]);
			goto out_err;
		}
		total_len += ov_len;
	}

	/* grow the blob to worst case */
	blob_len = fdt_totalsize(blob) + total_len;
	blob = xrealloc(blob, blob_len);
	fdt_open_into(blob, blob, blob_len);

	/* apply the overlays in sequence */
	for (i = 0; i < argc; i++) {
		ret = fdt_overlay_apply(blob, ovblob[i]);
		if (ret) {
			fprintf(stderr, "\nFailed to apply %s (%d)\n",
					argv[i], ret);
			goto out_err;
		}
	}

	fdt_pack(blob);
	ret = utilfdt_write(output_filename, blob);
	if (ret)
		fprintf(stderr, "\nFailed to write output blob %s\n",
				output_filename);

out_err:
	if (ovblob) {
		for (i = 0; i < argc; i++) {
			if (ovblob[i])
				free(ovblob[i]);
		}
		free(ovblob);
	}
	free(blob);

	return ret;
}

int main(int argc, char *argv[])
{
	int opt, i;
	char *input_filename = NULL;
	char *output_filename = NULL;

	while ((opt = util_getopt_long()) != EOF) {
		switch (opt) {
		case_USAGE_COMMON_FLAGS

		case 'i':
			input_filename = optarg;
			break;
		case 'o':
			output_filename = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		}
	}

	if (!input_filename)
		usage("missing input file");

	if (!output_filename)
		usage("missing output file");

	argv += optind;
	argc -= optind;

	if (argc <= 0)
		usage("missing overlay file(s)");

	if (verbose) {
		printf("input  = %s\n", input_filename);
		printf("output = %s\n", output_filename);
		for (i = 0; i < argc; i++)
			printf("overlay[%d] = %s\n", i, argv[i]);
	}

	if (do_fdtoverlay(input_filename, output_filename, argc, argv))
		return 1;

	return 0;
}
