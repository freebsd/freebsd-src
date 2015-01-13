/*-
 * Copyright (c) 2012 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: t_openpam_readlinev.c 648 2013-03-05 17:54:27Z des $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

#include "openpam_impl.h"
#include "t.h"

/*
 * Read a line from the temp file and verify that the result matches our
 * expectations: whether a line was read at all, how many and which words
 * it contained, how many lines were read (in case of quoted or escaped
 * newlines) and whether we reached the end of the file.
 */
static int
orlv_expect(struct t_file *tf, const char **expectedv, int lines, int eof)
{
	int expectedc, gotc, i, lineno = 0;
	char **gotv;

	expectedc = 0;
	if (expectedv != NULL)
		while (expectedv[expectedc] != NULL)
			++expectedc;
	gotv = openpam_readlinev(tf->file, &lineno, &gotc);
	if (t_ferror(tf))
		err(1, "%s(): %s", __func__, tf->name);
	if (expectedv != NULL && gotv == NULL) {
		t_verbose("expected %d words, got nothing\n", expectedc);
		return (0);
	}
	if (expectedv == NULL && gotv != NULL) {
		t_verbose("expected nothing, got %d words\n", gotc);
		FREEV(gotc, gotv);
		return (0);
	}
	if (expectedv != NULL && gotv != NULL) {
		if (expectedc != gotc) {
			t_verbose("expected %d words, got %d\n",
			    expectedc, gotc);
			FREEV(gotc, gotv);
			return (0);
		}
		for (i = 0; i < gotc; ++i) {
			if (strcmp(expectedv[i], gotv[i]) != 0) {
				t_verbose("word %d: expected <<%s>>, "
				    "got <<%s>>\n", i, expectedv[i], gotv[i]);
				FREEV(gotc, gotv);
				return (0);
			}
		}
		FREEV(gotc, gotv);
	}
	if (lineno != lines) {
		t_verbose("expected to advance %d lines, advanced %d lines\n",
		    lines, lineno);
		return (0);
	}
	if (eof && !t_feof(tf)) {
		t_verbose("expected EOF, but didn't get it\n");
		return (0);
	}
	if (!eof && t_feof(tf)) {
		t_verbose("didn't expect EOF, but got it anyway\n");
		return (0);
	}
	return (1);
}


/***************************************************************************
 * Commonly-used lines
 */

static const char *empty[] = {
	NULL
};

static const char *hello[] = {
	"hello",
	NULL
};

static const char *hello_world[] = {
	"hello",
	"world",
	NULL
};


/***************************************************************************
 * Lines without words
 */

T_FUNC(empty_input, "empty input")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	ret = orlv_expect(tf, NULL, 0 /*lines*/, 1 /*eof*/);
	t_fclose(tf);
	return (ret);
}

T_FUNC(empty_line, "empty line")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	t_fprintf(tf, "\n");
	t_frewind(tf);
	ret = orlv_expect(tf, empty, 1 /*lines*/, 0 /*eof*/);
	t_fclose(tf);
	return (ret);
}

T_FUNC(unterminated_empty_line, "unterminated empty line")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	t_fprintf(tf, " ");
	t_frewind(tf);
	ret = orlv_expect(tf, NULL, 0 /*lines*/, 1 /*eof*/);
	t_fclose(tf);
	return (ret);
}

T_FUNC(whitespace, "whitespace")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	t_fprintf(tf, " \n");
	t_frewind(tf);
	ret = orlv_expect(tf, empty, 1 /*lines*/, 0 /*eof*/);
	t_fclose(tf);
	return (ret);
}

T_FUNC(comment, "comment")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	t_fprintf(tf, "# comment\n");
	t_frewind(tf);
	ret = orlv_expect(tf, empty, 1 /*lines*/, 0 /*eof*/);
	t_fclose(tf);
	return (ret);
}

T_FUNC(whitespace_before_comment, "whitespace before comment")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	t_fprintf(tf, " # comment\n");
	t_frewind(tf);
	ret = orlv_expect(tf, empty, 1 /*lines*/, 0 /*eof*/);
	t_fclose(tf);
	return (ret);
}


/***************************************************************************
 * Simple words
 */

T_FUNC(one_word, "one word")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	t_fprintf(tf, "hello\n");
	t_frewind(tf);
	ret = orlv_expect(tf, hello, 1 /*lines*/, 0 /*eof*/);
	t_fclose(tf);
	return (ret);
}

T_FUNC(two_words, "two words")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	t_fprintf(tf, "hello world\n");
	t_frewind(tf);
	ret = orlv_expect(tf, hello_world, 1 /*lines*/, 0 /*eof*/);
	t_fclose(tf);
	return (ret);
}

T_FUNC(unterminated_line, "unterminated line")
{
	struct t_file *tf;
	int ret;

	tf = t_fopen(NULL);
	t_fprintf(tf, "hello world");
	t_frewind(tf);
	ret = orlv_expect(tf, hello_world, 0 /*lines*/, 1 /*eof*/);
	t_fclose(tf);
	return (ret);
}


/***************************************************************************
 * Boilerplate
 */

static const struct t_test *t_plan[] = {
	T(empty_input),
	T(empty_line),
	T(unterminated_empty_line),
	T(whitespace),
	T(comment),
	T(whitespace_before_comment),

	T(one_word),
	T(two_words),
	T(unterminated_line),

	NULL
};

const struct t_test **
t_prepare(int argc, char *argv[])
{

	(void)argc;
	(void)argv;
	return (t_plan);
}

void
t_cleanup(void)
{
}
