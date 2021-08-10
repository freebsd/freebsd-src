/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
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

// For some reason, Windows needs this header.
#ifndef _WIN32
#include <libgen.h>
#endif // _WIN32

// This is exactly what it looks like. It just slaps a simple license header on
// the generated C source file.
static const char* const bc_gen_header =
	"// Copyright (c) 2018-2021 Gavin D. Howard and contributors.\n"
	"// Licensed under the 2-clause BSD license.\n"
	"// *** AUTOMATICALLY GENERATED FROM %s. DO NOT MODIFY. ***\n\n";

// These are just format strings used to generate the C source.
static const char* const bc_gen_label = "const char *%s = \"%s\";\n\n";
static const char* const bc_gen_label_extern = "extern const char *%s;\n\n";
static const char* const bc_gen_ifdef = "#if %s\n";
static const char* const bc_gen_endif = "#endif // %s\n";
static const char* const bc_gen_name = "const char %s[] = {\n";
static const char* const bc_gen_name_extern = "extern const char %s[];\n\n";

// Error codes. We can't use 0 because these are used as exit statuses, and 0
// as an exit status is not an error.
#define IO_ERR (1)
#define INVALID_INPUT_FILE (2)
#define INVALID_PARAMS (3)

// This is the max width to print characters to the screen. This is to ensure
// that lines don't go much over 80 characters.
#define MAX_WIDTH (72)

/**
 * Open a file. This function is to smooth over differences between POSIX and
 * Windows.
 * @param f         A pointer to the FILE pointer that will be initialized.
 * @param filename  The name of the file.
 * @param mode      The mode to open the file in.
 */
static void open_file(FILE** f, const char* filename, const char* mode) {

#ifndef _WIN32

	*f = fopen(filename, mode);

#else // _WIN32

	// We want the file pointer to be NULL on failure, but fopen_s() is not
	// guaranteed to set it.
	*f = NULL;
	fopen_s(f, filename, mode);

#endif // _WIN32
}

/**
 * Outputs a label, which is a string literal that the code can use as a name
 * for the file that is being turned into a string. This is important for the
 * math libraries because the parse and lex code expects a filename. The label
 * becomes the filename for the purposes of lexing and parsing.
 *
 * The label is generated from bc_gen_label (above). It has the form:
 *
 * const char *<label_name> = <label>;
 *
 * This function is also needed to smooth out differences between POSIX and
 * Windows, specifically, the fact that Windows uses backslashes for filenames
 * and that backslashes have to be escaped in a string literal.
 *
 * @param out    The file to output to.
 * @param label  The label name.
 * @param name   The actual label text, which is a filename.
 * @return       Positive if no error, negative on error, just like *printf().
 */
static int output_label(FILE* out, const char* label, const char* name) {

#ifndef _WIN32

	return fprintf(out, bc_gen_label, label, name);

#else // _WIN32

	size_t i, count = 0, len = strlen(name);
	char* buf;
	int ret;

	// This loop counts how many backslashes there are in the label.
	for (i = 0; i < len; ++i) count += (name[i] == '\\');

	buf = (char*) malloc(len + 1 + count);
	if (buf == NULL) return -1;

	count = 0;

	// This loop is the meat of the Windows version. What it does is copy the
	// label byte-for-byte, unless it encounters a backslash, in which case, it
	// copies the backslash twice to have it escaped properly in the string
	// literal.
	for (i = 0; i < len; ++i) {

		buf[i + count] = name[i];

		if (name[i] == '\\') {
			count += 1;
			buf[i + count] = name[i];
		}
	}

	buf[i + count] = '\0';

	ret = fprintf(out, bc_gen_label, label, buf);

	free(buf);

	return ret;

#endif // _WIN32
}

/**
 * This program generates C strings (well, actually, C char arrays) from text
 * files. It generates 1 C source file. The resulting file has this structure:
 *
 * <Copyright Header>
 *
 * [<Label Extern>]
 *
 * <Char Array Extern>
 *
 * [<Preprocessor Guard Begin>]
 * [<Label Definition>]
 *
 * <Char Array Definition>
 * [<Preprocessor Guard End>]
 *
 * Anything surrounded by square brackets may not be in the final generated
 * source file.
 *
 * The required command-line parameters are:
 *
 * input   Input filename.
 * output  Output filename.
 * name    The name of the char array.
 *
 * The optional parameters are:
 *
 * label        If given, a label for the char array. See the comment for the
 *              output_label() function. It is meant as a "filename" for the
 *              text when processed by bc and dc. If label is given, then the
 *              <Label Extern> and <Label Definition> will exist in the
 *              generated source file.
 * define       If given, a preprocessor macro that should be used as a guard
 *              for the char array and its label. If define is given, then
 *              <Preprocessor Guard Begin> will exist in the form
 *              "#if <define>" as part of the generated source file, and
 *              <Preprocessor Guard End> will exist in the form
 *              "endif // <define>".
 * remove_tabs  If this parameter exists, it must be an integer. If it is
 *              non-zero, then tabs are removed from the input file text before
 *              outputting to the output char array.
 *
 * All text files that are transformed have license comments. This program finds
 * the end of that comment and strips it out as well.
 */
int main(int argc, char *argv[]) {

	FILE *in, *out;
	char *label, *define, *name;
	int c, count, slashes, err = IO_ERR;
	bool has_label, has_define, remove_tabs;

	if (argc < 4) {
		printf("usage: %s input output name [label [define [remove_tabs]]]\n",
		       argv[0]);
		return INVALID_PARAMS;
	}

	name = argv[3];

	has_label = (argc > 4 && strcmp("", argv[4]) != 0);
	label = has_label ? argv[4] : "";

	has_define = (argc > 5 && strcmp("", argv[5]) != 0);
	define = has_define ? argv[5] : "";

	remove_tabs = (argc > 6);

	open_file(&in, argv[1], "r");
	if (!in) return INVALID_INPUT_FILE;

	open_file(&out, argv[2], "w");
	if (!out) goto out_err;

	if (fprintf(out, bc_gen_header, argv[1]) < 0) goto err;
	if (has_label && fprintf(out, bc_gen_label_extern, label) < 0) goto err;
	if (fprintf(out, bc_gen_name_extern, name) < 0) goto err;
	if (has_define && fprintf(out, bc_gen_ifdef, define) < 0) goto err;
	if (has_label && output_label(out, label, argv[1]) < 0) goto err;
	if (fprintf(out, bc_gen_name, name) < 0) goto err;

	c = count = slashes = 0;

	// This is where the end of the license comment is found.
	while (slashes < 2 && (c = fgetc(in)) >= 0) {
		slashes += (slashes == 1 && c == '/' && fgetc(in) == '\n');
		slashes += (!slashes && c == '/' && fgetc(in) == '*');
	}

	// The file is invalid if the end of the license comment could not be found.
	if (c < 0) {
		err = INVALID_INPUT_FILE;
		goto err;
	}

	// Do not put extra newlines at the beginning of the char array.
	while ((c = fgetc(in)) == '\n');

	// This loop is what generates the actual char array. It counts how many
	// chars it has printed per line in order to insert newlines at appropriate
	// places. It also skips tabs if they should be removed.
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

	// Make sure the end looks nice and insert the NUL byte at the end.
	if (!count && (fputc(' ', out) == EOF || fputc(' ', out) == EOF)) goto err;
	if (fprintf(out, "0\n};\n") < 0) goto err;

	err = (has_define && fprintf(out, bc_gen_endif, define) < 0);

err:
	fclose(out);
out_err:
	fclose(in);
	return err;
}
