/*
 * Regress test for misc helper functions.
 *
 * Placed in the public domain.
 */

#include "includes.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "log.h"
#include "misc.h"
#include "xmalloc.h"

void test_misc(void);

static void
test_chop(void)
{
	char *s;

	TEST_START("chop newline");
	s = xstrdup("hello\n");
	ASSERT_STRING_EQ(chop(s), "hello");
	free(s);
	TEST_DONE();

	TEST_START("chop carriage return");
	s = xstrdup("hello\r");
	ASSERT_STRING_EQ(chop(s), "hello");
	free(s);
	TEST_DONE();

	TEST_START("chop CRLF");
	s = xstrdup("hello\r\n");
	ASSERT_STRING_EQ(chop(s), "hello");
	free(s);
	TEST_DONE();

	TEST_START("chop newline in middle");
	s = xstrdup("he\nllo");
	ASSERT_STRING_EQ(chop(s), "he");
	free(s);
	TEST_DONE();

	TEST_START("chop no newline");
	s = xstrdup("hello");
	ASSERT_STRING_EQ(chop(s), "hello");
	free(s);
	TEST_DONE();

	TEST_START("chop empty string");
	s = xstrdup("");
	ASSERT_STRING_EQ(chop(s), "");
	free(s);
	TEST_DONE();

	TEST_START("chop only newline");
	s = xstrdup("\n");
	ASSERT_STRING_EQ(chop(s), "");
	free(s);
	TEST_DONE();

	TEST_START("chop only CR");
	s = xstrdup("\r");
	ASSERT_STRING_EQ(chop(s), "");
	free(s);
	TEST_DONE();

	TEST_START("chop only CRLF");
	s = xstrdup("\r\n");
	ASSERT_STRING_EQ(chop(s), "");
	free(s);
	TEST_DONE();
}

static void
test_rtrim(void)
{
	char *s;

	TEST_START("rtrim trailing space");
	s = xstrdup("hello  ");
	rtrim(s);
	ASSERT_STRING_EQ(s, "hello");
	free(s);
	TEST_DONE();

	TEST_START("rtrim trailing tab");
	s = xstrdup("hello\t\t");
	rtrim(s);
	ASSERT_STRING_EQ(s, "hello");
	free(s);
	TEST_DONE();

	TEST_START("rtrim trailing mixed whitespace");
	s = xstrdup("hello \t ");
	rtrim(s);
	ASSERT_STRING_EQ(s, "hello");
	free(s);
	TEST_DONE();

	TEST_START("rtrim no trailing whitespace");
	s = xstrdup("hello");
	rtrim(s);
	ASSERT_STRING_EQ(s, "hello");
	free(s);
	TEST_DONE();

	TEST_START("rtrim whitespace in middle");
	s = xstrdup("he llo");
	rtrim(s);
	ASSERT_STRING_EQ(s, "he llo");
	free(s);
	TEST_DONE();

	TEST_START("rtrim empty string");
	s = xstrdup("");
	rtrim(s);
	ASSERT_STRING_EQ(s, "");
	free(s);
	TEST_DONE();

	TEST_START("rtrim only whitespace");
	s = xstrdup("   \t");
	rtrim(s);
	ASSERT_STRING_EQ(s, "");
	free(s);
	TEST_DONE();
}

static void
test_strprefix(void)
{
	const char *s;

	TEST_START("strprefix basic match");
	s = strprefix("hello world", "hello", 0);
	ASSERT_PTR_NE(s, NULL);
	ASSERT_STRING_EQ(s, " world");
	TEST_DONE();

	TEST_START("strprefix no match");
	s = strprefix("hello world", "world", 0);
	ASSERT_PTR_EQ(s, NULL);
	TEST_DONE();

	TEST_START("strprefix full match");
	s = strprefix("hello", "hello", 0);
	ASSERT_PTR_NE(s, NULL);
	ASSERT_STRING_EQ(s, "");
	TEST_DONE();

	TEST_START("strprefix empty string");
	s = strprefix("", "hello", 0);
	ASSERT_PTR_EQ(s, NULL);
	TEST_DONE();

	TEST_START("strprefix empty prefix");
	s = strprefix("hello", "", 0);
	ASSERT_PTR_NE(s, NULL);
	ASSERT_STRING_EQ(s, "hello");
	TEST_DONE();

	TEST_START("strprefix case sensitive no match");
	s = strprefix("Hello world", "hello", 0);
	ASSERT_PTR_EQ(s, NULL);
	TEST_DONE();

	TEST_START("strprefix case insensitive match");
	s = strprefix("Hello world", "hello", 1);
	ASSERT_PTR_NE(s, NULL);
	ASSERT_STRING_EQ(s, " world");
	TEST_DONE();

	TEST_START("strprefix case insensitive full match");
	s = strprefix("HELLO", "hello", 1);
	ASSERT_PTR_NE(s, NULL);
	ASSERT_STRING_EQ(s, "");
	TEST_DONE();
}

static void
test_fmt_timeframe(void)
{
	TEST_START("fmt_timeframe seconds");
	ASSERT_STRING_EQ(fmt_timeframe(0), "00:00:00");
	ASSERT_STRING_EQ(fmt_timeframe(59), "00:00:59");
	ASSERT_STRING_EQ(fmt_timeframe(60), "00:01:00");
	ASSERT_STRING_EQ(fmt_timeframe(3599), "00:59:59");
	ASSERT_STRING_EQ(fmt_timeframe(3600), "01:00:00");
	ASSERT_STRING_EQ(fmt_timeframe(86399), "23:59:59");
	TEST_DONE();

	TEST_START("fmt_timeframe days");
	ASSERT_STRING_EQ(fmt_timeframe(86400), "1d00h00m");
	ASSERT_STRING_EQ(fmt_timeframe(90061), "1d01h01m");
	ASSERT_STRING_EQ(fmt_timeframe(604799), "6d23h59m");
	TEST_DONE();

	TEST_START("fmt_timeframe weeks");
	ASSERT_STRING_EQ(fmt_timeframe(604800), "01w0d00h");
	ASSERT_STRING_EQ(fmt_timeframe(694861), "01w1d01h");
	TEST_DONE();
}

static void
test_arglist(void)
{
	arglist args;
	u_int i;

	memset(&args, 0, sizeof(args));

	TEST_START("addargs initial");
	addargs(&args, "one");
	ASSERT_U_INT_EQ(args.num, 1);
	ASSERT_U_INT_EQ(args.nalloc, 32);
	ASSERT_PTR_NE(args.list, NULL);
	ASSERT_STRING_EQ(args.list[0], "one");
	ASSERT_PTR_EQ(args.list[1], NULL);
	TEST_DONE();

	TEST_START("addargs second");
	addargs(&args, "two");
	ASSERT_U_INT_EQ(args.num, 2);
	ASSERT_U_INT_EQ(args.nalloc, 32);
	ASSERT_PTR_NE(args.list, NULL);
	ASSERT_STRING_EQ(args.list[0], "one");
	ASSERT_STRING_EQ(args.list[1], "two");
	ASSERT_PTR_EQ(args.list[2], NULL);
	TEST_DONE();

	TEST_START("addargs with format");
	addargs(&args, "three=%d", 3);
	ASSERT_U_INT_EQ(args.num, 3);
	ASSERT_U_INT_EQ(args.nalloc, 32);
	ASSERT_PTR_NE(args.list, NULL);
	ASSERT_STRING_EQ(args.list[0], "one");
	ASSERT_STRING_EQ(args.list[1], "two");
	ASSERT_STRING_EQ(args.list[2], "three=3");
	ASSERT_PTR_EQ(args.list[3], NULL);
	TEST_DONE();

	TEST_START("replacearg middle");
	replacearg(&args, 1, "TWO!");
	ASSERT_U_INT_EQ(args.num, 3);
	ASSERT_STRING_EQ(args.list[0], "one");
	ASSERT_STRING_EQ(args.list[1], "TWO!");
	ASSERT_STRING_EQ(args.list[2], "three=3");
	ASSERT_PTR_EQ(args.list[3], NULL);
	TEST_DONE();

	TEST_START("replacearg first");
	replacearg(&args, 0, "ONE!");
	ASSERT_U_INT_EQ(args.num, 3);
	ASSERT_STRING_EQ(args.list[0], "ONE!");
	ASSERT_STRING_EQ(args.list[1], "TWO!");
	ASSERT_STRING_EQ(args.list[2], "three=3");
	ASSERT_PTR_EQ(args.list[3], NULL);
	TEST_DONE();

	TEST_START("replacearg last");
	replacearg(&args, 2, "THREE=3!");
	ASSERT_U_INT_EQ(args.num, 3);
	ASSERT_STRING_EQ(args.list[0], "ONE!");
	ASSERT_STRING_EQ(args.list[1], "TWO!");
	ASSERT_STRING_EQ(args.list[2], "THREE=3!");
	ASSERT_PTR_EQ(args.list[3], NULL);
	TEST_DONE();

	TEST_START("replacearg with format");
	replacearg(&args, 1, "two=%d", 2);
	ASSERT_U_INT_EQ(args.num, 3);
	ASSERT_STRING_EQ(args.list[0], "ONE!");
	ASSERT_STRING_EQ(args.list[1], "two=2");
	ASSERT_STRING_EQ(args.list[2], "THREE=3!");
	ASSERT_PTR_EQ(args.list[3], NULL);
	TEST_DONE();

	TEST_START("addargs reallocation");
	for (i = args.num; i < 33; i++)
		addargs(&args, "pad-%d", i);
	ASSERT_U_INT_EQ(args.num, 33);
	ASSERT_U_INT_GE(args.nalloc, 33);
	ASSERT_STRING_EQ(args.list[32], "pad-32");
	ASSERT_PTR_EQ(args.list[33], NULL);
	TEST_DONE();

	TEST_START("freeargs");
	freeargs(&args);
	ASSERT_U_INT_EQ(args.num, 0);
	ASSERT_U_INT_EQ(args.nalloc, 0);
	ASSERT_PTR_EQ(args.list, NULL);
	TEST_DONE();

	TEST_START("freeargs on NULL");
	freeargs(NULL);
	TEST_DONE();

	TEST_START("freeargs on empty");
	memset(&args, 0, sizeof(args));
	freeargs(&args);
	ASSERT_U_INT_EQ(args.num, 0);
	ASSERT_U_INT_EQ(args.nalloc, 0);
	ASSERT_PTR_EQ(args.list, NULL);
	TEST_DONE();
}

static void
test_tohex(void)
{
	char *hex;

	TEST_START("tohex simple");
	hex = tohex("foo", 3);
	ASSERT_STRING_EQ(hex, "666f6f");
	free(hex);
	TEST_DONE();

	TEST_START("tohex with null");
	hex = tohex("a\0b", 3);
	ASSERT_STRING_EQ(hex, "610062");
	free(hex);
	TEST_DONE();

	TEST_START("tohex empty");
	hex = tohex("", 0);
	ASSERT_STRING_EQ(hex, "");
	free(hex);
	TEST_DONE();
}

static void
test_lowercase(void)
{
	char *s;

	TEST_START("lowercase mixed");
	s = xstrdup("HeLlO WoRlD 123");
	lowercase(s);
	ASSERT_STRING_EQ(s, "hello world 123");
	free(s);
	TEST_DONE();

	TEST_START("lowercase empty");
	s = xstrdup("");
	lowercase(s);
	ASSERT_STRING_EQ(s, "");
	free(s);
	TEST_DONE();
}

static void
test_path_absolute(void)
{
	TEST_START("path_absolute absolute");
	ASSERT_INT_EQ(path_absolute("/foo/bar"), 1);
	TEST_DONE();

	TEST_START("path_absolute relative");
	ASSERT_INT_EQ(path_absolute("foo/bar"), 0);
	TEST_DONE();

	TEST_START("path_absolute empty");
	ASSERT_INT_EQ(path_absolute(""), 0);
	TEST_DONE();
}

static void
test_stringlist(void)
{
	char **list = NULL;

	TEST_START("stringlist_append initial");
	stringlist_append(&list, "one");
	ASSERT_PTR_NE(list, NULL);
	ASSERT_STRING_EQ(list[0], "one");
	ASSERT_PTR_EQ(list[1], NULL);
	TEST_DONE();

	TEST_START("stringlist_append second");
	stringlist_append(&list, "two");
	ASSERT_PTR_NE(list, NULL);
	ASSERT_STRING_EQ(list[0], "one");
	ASSERT_STRING_EQ(list[1], "two");
	ASSERT_PTR_EQ(list[2], NULL);
	TEST_DONE();

	TEST_START("stringlist_append third");
	stringlist_append(&list, "three");
	ASSERT_PTR_NE(list, NULL);
	ASSERT_STRING_EQ(list[0], "one");
	ASSERT_STRING_EQ(list[1], "two");
	ASSERT_STRING_EQ(list[2], "three");
	ASSERT_PTR_EQ(list[3], NULL);
	TEST_DONE();

	TEST_START("stringlist_free");
	stringlist_free(list);
	TEST_DONE();

	TEST_START("stringlist_free NULL");
	stringlist_free(NULL);
	TEST_DONE();
}

static void
test_skip_space(void)
{
	char *s, *p;

	TEST_START("skip_space leading spaces");
	s = p = xstrdup("  hello");
	skip_space(&p);
	ASSERT_STRING_EQ(p, "hello");
	free(s);
	TEST_DONE();

	TEST_START("skip_space leading tabs");
	s = p = xstrdup("\t\thello");
	skip_space(&p);
	ASSERT_STRING_EQ(p, "hello");
	free(s);
	TEST_DONE();

	TEST_START("skip_space leading mixed whitespace");
	s = p = xstrdup(" \t hello");
	skip_space(&p);
	ASSERT_STRING_EQ(p, "hello");
	free(s);
	TEST_DONE();

	TEST_START("skip_space no leading whitespace");
	s = p = xstrdup("hello");
	skip_space(&p);
	ASSERT_STRING_EQ(p, "hello");
	free(s);
	TEST_DONE();

	TEST_START("skip_space empty string");
	s = p = xstrdup("");
	skip_space(&p);
	ASSERT_STRING_EQ(p, "");
	free(s);
	TEST_DONE();

	TEST_START("skip_space only whitespace");
	s = p = xstrdup(" \t ");
	skip_space(&p);
	ASSERT_STRING_EQ(p, "");
	free(s);
	TEST_DONE();
}

void
test_misc(void)
{
	test_chop();
	test_rtrim();
	test_strprefix();
	test_fmt_timeframe();
	test_arglist();
	test_tohex();
	test_lowercase();
	test_path_absolute();
	test_stringlist();
	test_skip_space();
}
