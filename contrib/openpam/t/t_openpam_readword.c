/*-
 * Copyright (c) 2012 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 * $Id: t_openpam_readword.c 584 2012-04-07 22:47:16Z des $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

#include "t.h"

static char filename[1024];
static FILE *f;

/*
 * Open the temp file and immediately unlink it so it doesn't leak in case
 * of premature exit.
 */
static void
orw_open(void)
{
	int fd;

	if ((fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0600)) < 0)
		err(1, "%s(): %s", __func__, filename);
	if ((f = fdopen(fd, "r+")) == NULL)
		err(1, "%s(): %s", __func__, filename);
	if (unlink(filename) < 0)
		err(1, "%s(): %s", __func__, filename);
}

/*
 * Write text to the temp file.
 */
static void
orw_output(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	if (ferror(f))
		err(1, "%s", filename);
}

/*
 * Rewind the temp file.
 */
static void
orw_rewind(void)
{

	errno = 0;
	rewind(f);
	if (errno != 0)
		err(1, "%s(): %s", __func__, filename);
}

/*
 * Read a word from the temp file and verify that the result matches our
 * expectations: whether a word was read at all, how many lines were read
 * (in case of quoted or escaped newlines), whether we reached the end of
 * the file and whether we reached the end of the line.
 */
static int
orw_expect(const char *expected, int lines, int eof, int eol)
{
	int ch, lineno = 0;
	char *got;
	size_t len;

	got = openpam_readword(f, &lineno, &len);
	if (ferror(f))
		err(1, "%s(): %s", __func__, filename);
	if (expected != NULL && got == NULL) {
		t_verbose("expected <<%s>>, got nothing\n", expected);
		return (0);
	}
	if (expected == NULL && got != NULL) {
		t_verbose("expected nothing, got <<%s>>\n", got);
		return (0);
	}
	if (expected != NULL && got != NULL && strcmp(expected, got) != 0) {
		t_verbose("expected <<%s>>, got <<%s>>\n", expected, got);
		return (0);
	}
	if (lineno != lines) {
		t_verbose("expected to advance %d lines, advanced %d lines\n",
		    lines, lineno);
		return (0);
	}
	if (eof && !feof(f)) {
		t_verbose("expected EOF, but didn't get it\n");
		return (0);
	}
	if (!eof && feof(f)) {
		t_verbose("didn't expect EOF, but got it anyway\n");
		return (0);
	}
	ch = fgetc(f);
	if (ferror(f))
		err(1, "%s(): %s", __func__, filename);
	if (eol && ch != '\n') {
		t_verbose("expected EOL, but didn't get it\n");
		return (0);
	}
	if (!eol && ch == '\n') {
		t_verbose("didn't expect EOL, but got it anyway\n");
		return (0);
	}
	if (ch != EOF)
		ungetc(ch, f);
	return (1);
}

/*
 * Close the temp file.
 */
void
orw_close(void)
{

	if (fclose(f) != 0)
		err(1, "%s(): %s", __func__, filename);
	f = NULL;
}


/***************************************************************************
 * Lines without words
 */

T_FUNC(empty_input, "empty input")
{
	int ret;

	orw_open();
	ret = orw_expect(NULL, 0 /*lines*/, 1 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(empty_line, "empty line")
{
	int ret;

	orw_open();
	orw_output("\n");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(unterminated_line, "unterminated line")
{
	int ret;

	orw_open();
	orw_output(" ");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 1 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(single_whitespace, "single whitespace")
{
	int ret;

	orw_open();
	orw_output(" \n");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(multiple_whitespace, "multiple whitespace")
{
	int ret;

	orw_open();
	orw_output(" \t\r\n");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(comment, "comment")
{
	int ret;

	orw_open();
	orw_output("# comment\n");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(whitespace_before_comment, "whitespace before comment")
{
	int ret;

	orw_open();
	orw_output(" # comment\n");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}


/***************************************************************************
 * Simple cases - no quotes or escapes
 */

T_FUNC(single_word, "single word")
{
	const char *word = "hello";
	int ret;

	orw_open();
	orw_output("%s\n", word);
	orw_rewind();
	ret = orw_expect(word, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(single_whitespace_before_word, "single whitespace before word")
{
	const char *word = "hello";
	int ret;

	orw_open();
	orw_output(" %s\n", word);
	orw_rewind();
	ret = orw_expect(word, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(double_whitespace_before_word, "double whitespace before word")
{
	const char *word = "hello";
	int ret;

	orw_open();
	orw_output("  %s\n", word);
	orw_rewind();
	ret = orw_expect(word, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(single_whitespace_after_word, "single whitespace after word")
{
	const char *word = "hello";
	int ret;

	orw_open();
	orw_output("%s \n", word);
	orw_rewind();
	ret = orw_expect(word, 0 /*lines*/, 0 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(double_whitespace_after_word, "double whitespace after word")
{
	const char *word = "hello";
	int ret;

	orw_open();
	orw_output("%s  \n", word);
	orw_rewind();
	ret = orw_expect(word, 0 /*lines*/, 0 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(comment_after_word, "comment after word")
{
	const char *word = "hello";
	int ret;

	orw_open();
	orw_output("%s # comment\n", word);
	orw_rewind();
	ret = orw_expect(word, 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect(NULL, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(word_containing_hash, "word containing hash")
{
	const char *word = "hello#world";
	int ret;

	orw_open();
	orw_output("%s\n", word);
	orw_rewind();
	ret = orw_expect(word, 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(two_words, "two words")
{
	const char *word[] = { "hello", "world" };
	int ret;

	orw_open();
	orw_output("%s %s\n", word[0], word[1]);
	orw_rewind();
	ret = orw_expect(word[0], 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect(word[1], 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}


/***************************************************************************
 * Escapes
 */

T_FUNC(naked_escape, "naked escape")
{
	int ret;

	orw_open();
	orw_output("\\");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 1 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_escape, "escaped escape")
{
	int ret;

	orw_open();
	orw_output("\\\\\n");
	orw_rewind();
	ret = orw_expect("\\", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_whitespace, "escaped whitespace")
{
	int ret;

	orw_open();
	orw_output("\\  \\\t \\\r \\\n\n");
	orw_rewind();
	ret = orw_expect(" ", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\t", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\r", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    /* this last one is a line continuation */
	    orw_expect(NULL, 1 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_newline_before_word, "escaped newline before word")
{
	int ret;

	orw_open();
	orw_output("\\\nhello world\n");
	orw_rewind();
	ret = orw_expect("hello", 1 /*lines*/, 0 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_newline_within_word, "escaped newline within word")
{
	int ret;

	orw_open();
	orw_output("hello\\\nworld\n");
	orw_rewind();
	ret = orw_expect("helloworld", 1 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_newline_after_word, "escaped newline after word")
{
	int ret;

	orw_open();
	orw_output("hello\\\n world\n");
	orw_rewind();
	ret = orw_expect("hello", 1 /*lines*/, 0 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_letter, "escaped letter")
{
	int ret;

	orw_open();
	orw_output("\\z\n");
	orw_rewind();
	ret = orw_expect("z", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}


/***************************************************************************
 * Quotes
 */

T_FUNC(naked_single_quote, "naked single quote")
{
	int ret;

	orw_open();
	orw_output("'");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 1 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(naked_double_quote, "naked double quote")
{
	int ret;

	orw_open();
	orw_output("\"");
	orw_rewind();
	ret = orw_expect(NULL, 0 /*lines*/, 1 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(empty_single_quotes, "empty single quotes")
{
	int ret;

	orw_open();
	orw_output("''\n");
	orw_rewind();
	ret = orw_expect("", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(empty_double_quotes, "empty double quotes")
{
	int ret;

	orw_open();
	orw_output("\"\"\n");
	orw_rewind();
	ret = orw_expect("", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(single_quotes_within_double_quotes, "single quotes within double quotes")
{
	int ret;

	orw_open();
	orw_output("\"' '\"\n");
	orw_rewind();
	ret = orw_expect("' '", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(double_quotes_within_single_quotes, "double quotes within single quotes")
{
	int ret;

	orw_open();
	orw_output("'\" \"'\n");
	orw_rewind();
	ret = orw_expect("\" \"", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(single_quoted_whitespace, "single-quoted whitespace")
{
	int ret;

	orw_open();
	orw_output("' ' '\t' '\r' '\n'\n");
	orw_rewind();
	ret = orw_expect(" ", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\t", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\r", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\n", 1 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(double_quoted_whitespace, "double-quoted whitespace")
{
	int ret;

	orw_open();
	orw_output("\" \" \"\t\" \"\r\" \"\n\"\n");
	orw_rewind();
	ret = orw_expect(" ", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\t", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\r", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\n", 1 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(single_quoted_words, "single-quoted words")
{
	int ret;

	orw_open();
	orw_output("'hello world'\n");
	orw_rewind();
	ret = orw_expect("hello world", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(double_quoted_words, "double-quoted words")
{
	int ret;

	orw_open();
	orw_output("\"hello world\"\n");
	orw_rewind();
	ret = orw_expect("hello world", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}


/***************************************************************************
 * Combinations of escape and quotes
 */

T_FUNC(escaped_single_quote,
    "escaped single quote")
{
	int ret;

	orw_open();
	orw_output("\\'\n");
	orw_rewind();
	ret = orw_expect("'", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_double_quote,
    "escaped double quote")
{
	int ret;

	orw_open();
	orw_output("\\\"\n");
	orw_rewind();
	ret = orw_expect("\"", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_whitespace_within_single_quotes,
    "escaped whitespace within single quotes")
{
	int ret;

	orw_open();
	orw_output("'\\ ' '\\\t' '\\\r' '\\\n'\n");
	orw_rewind();
	ret = orw_expect("\\ ", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\\\t", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\\\r", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\\\n", 1 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_whitespace_within_double_quotes,
    "escaped whitespace within double quotes")
{
	int ret;

	orw_open();
	orw_output("\"\\ \" \"\\\t\" \"\\\r\" \"\\\n\"\n");
	orw_rewind();
	ret = orw_expect("\\ ", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\\\t", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    orw_expect("\\\r", 0 /*lines*/, 0 /*eof*/, 0 /*eol*/) &&
	    /* this last one is a line continuation */
	    orw_expect("", 1 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_letter_within_single_quotes,
    "escaped letter within single quotes")
{
	int ret;

	orw_open();
	orw_output("'\\z'\n");
	orw_rewind();
	ret = orw_expect("\\z", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_letter_within_double_quotes,
    "escaped letter within double quotes")
{
	int ret;

	orw_open();
	orw_output("\"\\z\"\n");
	orw_rewind();
	ret = orw_expect("\\z", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_escape_within_single_quotes,
    "escaped escape within single quotes")
{
	int ret;

	orw_open();
	orw_output("'\\\\'\n");
	orw_rewind();
	ret = orw_expect("\\\\", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_escape_within_double_quotes,
    "escaped escape within double quotes")
{
	int ret;

	orw_open();
	orw_output("\"\\\\\"\n");
	orw_rewind();
	ret = orw_expect("\\", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_single_quote_within_single_quotes,
    "escaped single quote within single quotes")
{
	int ret;

	orw_open();
	orw_output("'\\''\n");
	orw_rewind();
	ret = orw_expect(NULL, 1 /*lines*/, 1 /*eof*/, 0 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_double_quote_within_single_quotes,
    "escaped double quote within single quotes")
{
	int ret;

	orw_open();
	orw_output("'\\\"'\n");
	orw_rewind();
	ret = orw_expect("\\\"", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_single_quote_within_double_quotes,
    "escaped single quote within double quotes")
{
	int ret;

	orw_open();
	orw_output("\"\\'\"\n");
	orw_rewind();
	ret = orw_expect("\\'", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}

T_FUNC(escaped_double_quote_within_double_quotes,
    "escaped double quote within double quotes")
{
	int ret;

	orw_open();
	orw_output("\"\\\"\"\n");
	orw_rewind();
	ret = orw_expect("\"", 0 /*lines*/, 0 /*eof*/, 1 /*eol*/);
	orw_close();
	return (ret);
}


/***************************************************************************
 * Boilerplate
 */

const struct t_test *t_plan[] = {
	T(empty_input),
	T(empty_line),
	T(single_whitespace),
	T(multiple_whitespace),
	T(comment),
	T(whitespace_before_comment),

	T(single_word),
	T(single_whitespace_before_word),
	T(double_whitespace_before_word),
	T(single_whitespace_after_word),
	T(double_whitespace_after_word),
	T(comment_after_word),
	T(word_containing_hash),
	T(two_words),

	T(naked_escape),
	T(escaped_escape),
	T(escaped_whitespace),
	T(escaped_newline_before_word),
	T(escaped_newline_within_word),
	T(escaped_newline_after_word),
	T(escaped_letter),

	T(naked_single_quote),
	T(naked_double_quote),
	T(empty_single_quotes),
	T(empty_double_quotes),
	T(single_quotes_within_double_quotes),
	T(double_quotes_within_single_quotes),
	T(single_quoted_whitespace),
	T(double_quoted_whitespace),
	T(single_quoted_words),
	T(double_quoted_words),

	T(escaped_single_quote),
	T(escaped_double_quote),
	T(escaped_whitespace_within_single_quotes),
	T(escaped_whitespace_within_double_quotes),
	T(escaped_letter_within_single_quotes),
	T(escaped_letter_within_double_quotes),
	T(escaped_escape_within_single_quotes),
	T(escaped_escape_within_double_quotes),
	T(escaped_single_quote_within_single_quotes),
	T(escaped_double_quote_within_single_quotes),
	T(escaped_single_quote_within_double_quotes),
	T(escaped_double_quote_within_double_quotes),

	NULL
};

const struct t_test **
t_prepare(int argc, char *argv[])
{

	(void)argc;
	(void)argv;
	snprintf(filename, sizeof filename, "%s.%d.tmp", t_progname, getpid());
	if (filename == NULL)
		err(1, "asprintf()");
	return (t_plan);
}

void
t_cleanup(void)
{
}
