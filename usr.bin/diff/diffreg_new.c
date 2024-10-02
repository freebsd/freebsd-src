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

#include <sys/types.h>
#include <sys/capsicum.h>
#ifndef DIFF_NO_MMAP
#include <sys/mman.h>
#endif
#include <sys/stat.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "diff.h"
#include <arraylist.h>
#include <diff_main.h>
#include <diff_output.h>

const char *format_label(const char *, struct stat *);

enum diffreg_algo {
	DIFFREG_ALGO_MYERS_THEN_MYERS_DIVIDE = 0,
	DIFFREG_ALGO_MYERS_THEN_PATIENCE = 1,
	DIFFREG_ALGO_PATIENCE = 2,
	DIFFREG_ALGO_NONE = 3,
};

int		 diffreg_new(char *, char *, int, int);
FILE *		 openfile(const char *, char **, struct stat *);

static const struct diff_algo_config myers_then_patience;
static const struct diff_algo_config myers_then_myers_divide;
static const struct diff_algo_config patience;
static const struct diff_algo_config myers_divide;

static const struct diff_algo_config myers_then_patience = (struct diff_algo_config){
	.impl = diff_algo_myers,
	.permitted_state_size = 1024 * 1024 * sizeof(int),
	.fallback_algo = &patience,
};

static const struct diff_algo_config myers_then_myers_divide =
	(struct diff_algo_config){
	.impl = diff_algo_myers,
	.permitted_state_size = 1024 * 1024 * sizeof(int),
	.fallback_algo = &myers_divide,
};

static const struct diff_algo_config patience = (struct diff_algo_config){
	.impl = diff_algo_patience,
	/* After subdivision, do Patience again: */
	.inner_algo = &patience,
	/* If subdivision failed, do Myers Divide et Impera: */
	.fallback_algo = &myers_then_myers_divide,
};

static const struct diff_algo_config myers_divide = (struct diff_algo_config){
	.impl = diff_algo_myers_divide,
	/* When division succeeded, start from the top: */
	.inner_algo = &myers_then_myers_divide,
	/* (fallback_algo = NULL implies diff_algo_none). */
};

static const struct diff_algo_config no_algo = (struct diff_algo_config){
	.impl = diff_algo_none,
};

/* If the state for a forward-Myers is small enough, use Myers, otherwise first
 * do a Myers-divide. */
static const struct diff_config diff_config_myers_then_myers_divide = {
	.atomize_func = diff_atomize_text_by_line,
	.algo = &myers_then_myers_divide,
};

/* If the state for a forward-Myers is small enough, use Myers, otherwise first
 * do a Patience. */
static const struct diff_config diff_config_myers_then_patience = {
	.atomize_func = diff_atomize_text_by_line,
	.algo = &myers_then_patience,
};

/* Directly force Patience as a first divider of the source file. */
static const struct diff_config diff_config_patience = {
	.atomize_func = diff_atomize_text_by_line,
	.algo = &patience,
};

/* Directly force Patience as a first divider of the source file. */
static const struct diff_config diff_config_no_algo = {
	.atomize_func = diff_atomize_text_by_line,
};

const char *
format_label(const char *oldlabel, struct stat *stb)
{
	const char *time_format = "%Y-%m-%d %H:%M:%S";
	char *newlabel;
	char buf[256];
	char end[10];
	struct tm tm, *tm_ptr;
	int nsec = stb->st_mtim.tv_nsec;
	size_t newlabellen, timelen, endlen;
	tm_ptr = localtime_r(&stb->st_mtime, &tm);

	timelen = strftime(buf, 256, time_format, tm_ptr);
	endlen = strftime(end, 10, "%z", tm_ptr);

	/*
	 * The new label is the length of the time, old label, timezone,
	 * 9 characters for nanoseconds, and 4 characters for a period
	 * and for formatting.
	 */
	newlabellen = timelen + strlen(oldlabel) + endlen + 9 + 4;
	newlabel = calloc(newlabellen, sizeof(char));

	snprintf(newlabel, newlabellen ,"%s\t%s.%.9d %s\n",
		oldlabel, buf, nsec, end);

	return newlabel;
}

int
diffreg_new(char *file1, char *file2, int flags, int capsicum)
{
	char *str1, *str2;
	FILE *f1, *f2;
	struct stat st1, st2;
	struct diff_input_info info;
	struct diff_data left = {}, right = {};
	struct diff_result *result = NULL;
	bool force_text, have_binary;
	int rc, atomizer_flags, rflags, diff_flags = 0;
	int context_lines = diff_context;
	const struct diff_config *cfg;
	enum diffreg_algo algo;
	cap_rights_t rights_ro;

	algo = DIFFREG_ALGO_MYERS_THEN_MYERS_DIVIDE;

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

	if (capsicum) {
		cap_rights_init(&rights_ro, CAP_READ, CAP_FSTAT, CAP_SEEK);
		if (caph_rights_limit(fileno(f1), &rights_ro) < 0)
			err(2, "unable to limit rights on: %s", file1);
		if (caph_rights_limit(fileno(f2), &rights_ro) < 0)
			err(2, "unable to limit rights on: %s", file2);
		if (fileno(f1) == STDIN_FILENO || fileno(f2) == STDIN_FILENO) {
			/* stdin has already been limited */
			if (caph_limit_stderr() == -1)
				err(2, "unable to limit stderr");
			if (caph_limit_stdout() == -1)
				err(2, "unable to limit stdout");
		} else if (caph_limit_stdio() == -1)
				err(2, "unable to limit stdio");
		caph_cache_catpages();
		caph_cache_tzdata();
		if (caph_enter() < 0)
			err(2, "unable to enter capability mode");
	}
	/*
	 * If we have been given a label use that for the paths, if not format
	 * the path with the files modification time.
	 */
	info.flags = 0;
	info.left_path = (label[0] != NULL) ?
		label[0] : format_label(file1, &stb1);
	info.right_path = (label[1] != NULL) ?
		label[1] : format_label(file2, &stb2);

	if (flags & D_FORCEASCII)
		diff_flags |= DIFF_FLAG_FORCE_TEXT_DATA;
	if (flags & D_IGNOREBLANKS)
		diff_flags |= DIFF_FLAG_IGNORE_WHITESPACE;
	if (flags & D_PROTOTYPE)
		diff_flags |= DIFF_FLAG_SHOW_PROTOTYPES;

	if (diff_atomize_file(&left, cfg, f1, (uint8_t *)str1, st1.st_size, diff_flags)) {
		rc = D_ERROR;
		goto done;
	}
	if (left.atomizer_flags & DIFF_ATOMIZER_FILE_TRUNCATED)
		warnx("%s truncated", file1);
	if (diff_atomize_file(&right, cfg, f2, (uint8_t *)str2, st2.st_size, diff_flags)) {
		rc = D_ERROR;
		goto done;
	}
	if (right.atomizer_flags & DIFF_ATOMIZER_FILE_TRUNCATED)
		warnx("%s truncated", file2);

	result = diff_main(cfg, &left, &right);
	if (result->rc != DIFF_RC_OK) {
		rc = D_ERROR;
		status |= 2;
		goto done;
	}
	/*
	 * If there wasn't an error, but we don't have any printable chunks
	 * then the files must match.
	 */
	if (!diff_result_contains_printable_chunks(result)) {
		rc = D_SAME;
		goto done;
	}

	atomizer_flags = (result->left->atomizer_flags | result->right->atomizer_flags);
	rflags = (result->left->root->diff_flags | result->right->root->diff_flags);
	force_text = (rflags & DIFF_FLAG_FORCE_TEXT_DATA);
	have_binary = (atomizer_flags & DIFF_ATOMIZER_FOUND_BINARY_DATA);

	if (have_binary && !force_text) {
		rc = D_BINARY;
		status |= 1;
		goto done;
	}

	if (color)
		diff_output_set_colors(color, del_code, add_code);
	if (diff_format == D_NORMAL) {
		rc = diff_output_plain(NULL, stdout, &info, result, false);
	} else if (diff_format == D_EDIT) {
		rc = diff_output_edscript(NULL, stdout, &info, result);
	} else {
		rc = diff_output_unidiff(NULL, stdout, &info, result,
		    context_lines);
	}
	if (rc != DIFF_RC_OK) {
		rc = D_ERROR;
		status |= 2;
	} else {
		rc = D_DIFFER;
		status |= 1;
	}
done:
	diff_result_free(result);
	diff_data_free(&left);
	diff_data_free(&right);
#ifndef DIFF_NO_MMAP
	if (str1)
		munmap(str1, st1.st_size);
	if (str2)
		munmap(str2, st2.st_size);
#endif
	fclose(f1);
	fclose(f2);

	return rc;
}

FILE *
openfile(const char *path, char **p, struct stat *st)
{
	FILE *f = NULL;

	if (strcmp(path, "-") == 0)
		f = stdin;
	else
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

bool
can_libdiff(int flags)
{
	/* We can't use fifos with libdiff yet */
	if (S_ISFIFO(stb1.st_mode) || S_ISFIFO(stb2.st_mode))
		return false;

	/* Is this one of the supported input/output modes for diffreg_new? */
	if ((flags == 0 || !(flags & ~D_NEWALGO_FLAGS)) &&
		ignore_pats == NULL && (
		diff_format == D_NORMAL ||
#if 0
		diff_format == D_EDIT ||
#endif
		diff_format == D_UNIFIED) &&
		(diff_algorithm == D_DIFFMYERS || diff_algorithm == D_DIFFPATIENCE)) {
		return true;
	}

	/* Fallback to using stone. */
	return false;
}
