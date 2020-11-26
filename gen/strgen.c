/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Generates a const array from a bc script.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <libgen.h>

static const char* const bc_gen_header =
	"// Copyright (c) 2018-2020 Gavin D. Howard and contributors.\n"
	"// Licensed under the 2-clause BSD license.\n"
	"// *** AUTOMATICALLY GENERATED FROM %s. DO NOT MODIFY. ***\n\n";

static const char* const bc_gen_label = "const char *%s = \"%s\";\n\n";
static const char* const bc_gen_label_extern = "extern const char *%s;\n\n";
static const char* const bc_gen_ifdef = "#if %s\n";
static const char* const bc_gen_endif = "#endif // %s\n";
static const char* const bc_gen_name = "const char %s[] = {\n";
static const char* const bc_gen_name_extern = "extern const char %s[];\n\n";

#define IO_ERR (1)
#define INVALID_INPUT_FILE (2)
#define INVALID_PARAMS (3)

#define MAX_WIDTH (74)

int main(int argc, char *argv[]) {

	FILE *in, *out;
	char *label, *define, *name;
	int c, count, slashes, err = IO_ERR;
	bool has_label, has_define, remove_tabs;

	if (argc < 5) {
		printf("usage: %s input output name header [label [define [remove_tabs]]]\n", argv[0]);
		return INVALID_PARAMS;
	}

	name = argv[3];

	has_label = (argc > 4 && strcmp("", argv[4]) != 0);
	label = has_label ? argv[4] : "";

	has_define = (argc > 5 && strcmp("", argv[5]) != 0);
	define = has_define ? argv[5] : "";

	remove_tabs = (argc > 6);

	in = fopen(argv[1], "r");
	if (!in) return INVALID_INPUT_FILE;

	out = fopen(argv[2], "w");
	if (!out) goto out_err;

	if (fprintf(out, bc_gen_header, argv[1]) < 0) goto err;
	if (has_label && fprintf(out, bc_gen_label_extern, label) < 0) goto err;
	if (fprintf(out, bc_gen_name_extern, name) < 0) goto err;
	if (has_define && fprintf(out, bc_gen_ifdef, define) < 0) goto err;
	if (has_label && fprintf(out, bc_gen_label, label, argv[1]) < 0) goto err;
	if (fprintf(out, bc_gen_name, name) < 0) goto err;

	c = count = slashes = 0;

	while (slashes < 2 && (c = fgetc(in)) >= 0) {
		slashes += (slashes == 1 && c == '/' && fgetc(in) == '\n');
		slashes += (!slashes && c == '/' && fgetc(in) == '*');
	}

	if (c < 0) {
		err = INVALID_INPUT_FILE;
		goto err;
	}

	while ((c = fgetc(in)) == '\n');

	while (c >= 0) {

		int val;

		if (!remove_tabs || c != '\t') {

			if (!count && fputc('\t', out) == EOF) goto err;

			val = fprintf(out, "%d,", c);
			if (val < 0) goto err;

			count += val;

			if (count > MAX_WIDTH) {
				count = 0;
				if (fputc('\n', out) == EOF) goto err;
			}
		}

		c = fgetc(in);
	}

	if (!count && (fputc(' ', out) == EOF || fputc(' ', out) == EOF)) goto err;
	if (fprintf(out, "0\n};\n") < 0) goto err;

	err = (has_define && fprintf(out, bc_gen_endif, define) < 0);

err:
	fclose(out);
out_err:
	fclose(in);
	return err;
}
