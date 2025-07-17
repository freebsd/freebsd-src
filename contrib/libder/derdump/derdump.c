/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <err.h>
#include <stdio.h>

#include <libder.h>

int
main(int argc, char *argv[])
{
	FILE *fp;
	struct libder_ctx *ctx;
	struct libder_object *root;
	size_t rootsz;
	bool first = true;

	if (argc < 2) {
		fprintf(stderr, "usage: %s file [file...]\n", argv[0]);
		return (1);
	}

	ctx = libder_open();
	libder_set_verbose(ctx, 2);
	for (int i = 1; i < argc; i++) {
		fp = fopen(argv[i], "rb");
		if (fp == NULL) {
			warn("%s", argv[i]);
			continue;
		}

		if (!first)
			fprintf(stderr, "\n");
		fprintf(stdout, "[%s]\n", argv[i]);
		root = libder_read_file(ctx, fp, &rootsz);
		if (root != NULL) {
			libder_obj_dump(root, stdout);
			libder_obj_free(root);
			root = NULL;
		}

		first = false;
		fclose(fp);
	}

	libder_close(ctx);

	return (0);
}
