/* Commandline diff utility to test diff implementations. */
/*
 * Copyright (c) 2018 Martin Pieuchot
 * Copyright (c) 2020 Neels Hofmeyr <neels@hofmeyr.de>
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

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <arraylist.h>
#include <diff_main.h>
#include <diff_output.h>

enum diffreg_algo {
	DIFFREG_ALGO_MYERS_THEN_MYERS_DIVIDE = 0,
	DIFFREG_ALGO_MYERS_THEN_PATIENCE = 1,
	DIFFREG_ALGO_PATIENCE = 2,
	DIFFREG_ALGO_NONE = 3,
};

__dead void	 usage(void);
int		 diffreg(char *, char *, enum diffreg_algo, bool, bool, bool,
			 int, bool);
FILE *		 openfile(const char *, char **, struct stat *);

__dead void
usage(void)
{
	fprintf(stderr,
		"usage: %s [-apPQTwe] [-U n] file1 file2\n"
		"\n"
		"  -a   Treat input as ASCII even if binary data is detected\n"
		"  -p   Show function prototypes in hunk headers\n"
		"  -P   Use Patience Diff (slower but often nicer)\n"
		"  -Q   Use forward-Myers for small files, otherwise Patience\n"
		"  -T   Trivial algo: detect similar start and end only\n"
		"  -w   Ignore Whitespace\n"
		"  -U n Number of Context Lines\n"
		"  -e   Produce ed script output\n"
		, getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	int ch, rc;
	bool force_text = false;
	bool ignore_whitespace = false;
	bool show_function_prototypes = false;
	bool edscript = false;
	int context_lines = 3;
	enum diffreg_algo algo = DIFFREG_ALGO_MYERS_THEN_MYERS_DIVIDE;

	while ((ch = getopt(argc, argv, "apPQTwU:e")) != -1) {
		switch (ch) {
		case 'a':
			force_text = true;
			break;
		case 'p':
			show_function_prototypes = true;
			break;
		case 'P':
			algo = DIFFREG_ALGO_PATIENCE;
			break;
		case 'Q':
			algo = DIFFREG_ALGO_MYERS_THEN_PATIENCE;
			break;
		case 'T':
			algo = DIFFREG_ALGO_NONE;
			break;
		case 'w':
			ignore_whitespace = true;
			break;
		case 'U':
			context_lines = atoi(optarg);
			break;
		case 'e':
			edscript = true;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	rc = diffreg(argv[0], argv[1], algo, force_text, ignore_whitespace,
	    show_function_prototypes, context_lines, edscript);
	if (rc != DIFF_RC_OK) {
		fprintf(stderr, "diff: %s\n", strerror(rc));
		return 1;
	}
	return 0;
}

const struct diff_algo_config myers_then_patience;
const struct diff_algo_config myers_then_myers_divide;
const struct diff_algo_config patience;
const struct diff_algo_config myers_divide;

const struct diff_algo_config myers_then_patience = (struct diff_algo_config){
	.impl = diff_algo_myers,
	.permitted_state_size = 1024 * 1024 * sizeof(int),
	.fallback_algo = &patience,
};

const struct diff_algo_config myers_then_myers_divide =
	(struct diff_algo_config){
	.impl = diff_algo_myers,
	.permitted_state_size = 1024 * 1024 * sizeof(int),
	.fallback_algo = &myers_divide,
};

const struct diff_algo_config patience = (struct diff_algo_config){
	.impl = diff_algo_patience,
	/* After subdivision, do Patience again: */
	.inner_algo = &patience,
	/* If subdivision failed, do Myers Divide et Impera: */
	.fallback_algo = &myers_then_myers_divide,
};

const struct diff_algo_config myers_divide = (struct diff_algo_config){
	.impl = diff_algo_myers_divide,
	/* When division succeeded, start from the top: */
	.inner_algo = &myers_then_myers_divide,
	/* (fallback_algo = NULL implies diff_algo_none). */
};

const struct diff_algo_config no_algo = (struct diff_algo_config){
	.impl = diff_algo_none,
};

/* If the state for a forward-Myers is small enough, use Myers, otherwise first
 * do a Myers-divide. */
const struct diff_config diff_config_myers_then_myers_divide = {
	.atomize_func = diff_atomize_text_by_line,
	.algo = &myers_then_myers_divide,
};

/* If the state for a forward-Myers is small enough, use Myers, otherwise first
 * do a Patience. */
const struct diff_config diff_config_myers_then_patience = {
	.atomize_func = diff_atomize_text_by_line,
	.algo = &myers_then_patience,
};

/* Directly force Patience as a first divider of the source file. */
const struct diff_config diff_config_patience = {
	.atomize_func = diff_atomize_text_by_line,
	.algo = &patience,
};

/* Directly force Patience as a first divider of the source file. */
const struct diff_config diff_config_no_algo = {
	.atomize_func = diff_atomize_text_by_line,
};

int
diffreg(char *file1, char *file2, enum diffreg_algo algo, bool force_text,
    bool ignore_whitespace, bool show_function_prototypes, int context_lines,
    bool edscript)
{
	char *str1, *str2;
	FILE *f1, *f2;
	struct stat st1, st2;
	struct diff_input_info info = {
		.left_path = file1,
		.right_path = file2,
	};
	struct diff_data left = {}, right = {};
	struct diff_result *result = NULL;
	int rc;
	const struct diff_config *cfg;
	int diff_flags = 0;
	
	switch (algo) {
	default:
	case DIFFREG_ALGO_MYERS_THEN_MYERS_DIVIDE:
		cfg = &diff_config_myers_then_myers_divide;
		break;
	case DIFFREG_ALGO_MYERS_THEN_PATIENCE:
		cfg = &diff_config_myers_then_patience;
		break;
	case DIFFREG_ALGO_PATIENCE:
		cfg = &diff_config_patience;
		break;
	case DIFFREG_ALGO_NONE:
		cfg = &diff_config_no_algo;
		break;
	}

	f1 = openfile(file1, &str1, &st1);
	f2 = openfile(file2, &str2, &st2);

	if (force_text)
		diff_flags |= DIFF_FLAG_FORCE_TEXT_DATA;
	if (ignore_whitespace)
		diff_flags |= DIFF_FLAG_IGNORE_WHITESPACE;
	if (show_function_prototypes)
		diff_flags |= DIFF_FLAG_SHOW_PROTOTYPES;

	rc = diff_atomize_file(&left, cfg, f1, str1, st1.st_size, diff_flags);
	if (rc)
		goto done;
	rc = diff_atomize_file(&right, cfg, f2, str2, st2.st_size, diff_flags);
	if (rc)
		goto done;

	result = diff_main(cfg, &left, &right);
#if 0
	rc = diff_output_plain(stdout, &info, result);
#else
	if (edscript)
		rc = diff_output_edscript(NULL, stdout, &info, result);
	else {
		rc = diff_output_unidiff(NULL, stdout, &info, result,
		    context_lines);
	}
#endif
done:
	diff_result_free(result);
	diff_data_free(&left);
	diff_data_free(&right);
	if (str1)
		munmap(str1, st1.st_size);
	if (str2)
		munmap(str2, st2.st_size);
	fclose(f1);
	fclose(f2);

	return rc;
}

FILE *
openfile(const char *path, char **p, struct stat *st)
{
	FILE *f = NULL;

	f = fopen(path, "r");
	if (f == NULL)
		err(2, "%s", path);

	if (fstat(fileno(f), st) == -1)
		err(2, "%s", path);

#ifndef DIFF_NO_MMAP
	*p = mmap(NULL, st->st_size, PROT_READ, MAP_PRIVATE, fileno(f), 0);
	if (*p == MAP_FAILED)
#endif
		*p = NULL; /* fall back on file I/O */

	return f;
}
