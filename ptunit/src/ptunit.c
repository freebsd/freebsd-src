/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ptunit.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>


struct ptunit_srcloc ptunit_mk_srcloc(const char *file, uint32_t line)
{
	struct ptunit_srcloc srcloc;

	srcloc.file = file;
	srcloc.line = line;

	return srcloc;
}

struct ptunit_result ptunit_mk_failed_signed_int(const char *expr,
						 const char *cmp,
						 struct ptunit_srcloc where,
						 int64_t actual,
						 int64_t expected)
{
	struct ptunit_result result;

	result.type = ptur_failed_signed_int;
	result.failed.where = where;
	result.failed.variant.signed_int.expr = expr;
	result.failed.variant.signed_int.cmp = cmp;
	result.failed.variant.signed_int.expected = expected;
	result.failed.variant.signed_int.actual = actual;

	return result;
}

struct ptunit_result ptunit_mk_failed_unsigned_int(const char *expr,
						   const char *cmp,
						   struct ptunit_srcloc where,
						   uint64_t actual,
						   uint64_t expected)
{
	struct ptunit_result result;

	result.type = ptur_failed_unsigned_int;
	result.failed.where = where;
	result.failed.variant.unsigned_int.expr = expr;
	result.failed.variant.unsigned_int.cmp = cmp;
	result.failed.variant.unsigned_int.expected = expected;
	result.failed.variant.unsigned_int.actual = actual;

	return result;
}

struct ptunit_result ptunit_mk_failed_pointer(const char *expr,
					      const char *cmp,
					      struct ptunit_srcloc where,
					      const void *actual,
					      const void *expected)
{
	struct ptunit_result result;

	result.type = ptur_failed_pointer;
	result.failed.where = where;
	result.failed.variant.pointer.expr = expr;
	result.failed.variant.pointer.cmp = cmp;
	result.failed.variant.pointer.expected = expected;
	result.failed.variant.pointer.actual = actual;

	return result;
}

static char *dupstr(const char *str)
{
	char *dup;
	size_t len;

	if (!str)
		str = "(null)";

	len = strlen(str);
	dup = malloc(len + 1);
	if (!dup)
		return NULL;

	strncpy(dup, str, len);
	dup[len] = 0;

	return dup;
}

struct ptunit_result ptunit_mk_failed_str(const char *expr,
					  const char *cmp,
					  struct ptunit_srcloc where,
					  const char *actual,
					  const char *expected)
{
	struct ptunit_result result;

	result.type = ptur_failed_str;
	result.failed.where = where;
	result.failed.variant.str.expr = expr;
	result.failed.variant.str.cmp = cmp;
	result.failed.variant.str.expected = dupstr(expected);
	result.failed.variant.str.actual = dupstr(actual);

	return result;
}

struct ptunit_result ptunit_mk_passed(void)
{
	struct ptunit_result result;

	memset(&result, 0, sizeof(result));
	result.type = ptur_passed;

	return result;
}

struct ptunit_result ptunit_mk_skipped(void)
{
	struct ptunit_result result;

	memset(&result, 0, sizeof(result));
	result.type = ptur_skipped;

	return result;
}

struct ptunit_test ptunit_mk_test(const char *name, const char *args)
{
	struct ptunit_test test;

	test.name = name;
	test.args = args;
	test.result = ptunit_mk_skipped();

	return test;
}

void ptunit_fini_test(struct ptunit_test *test)
{
	if (!test)
		return;

	switch (test->result.type) {
	case ptur_skipped:
	case ptur_passed:
	case ptur_failed_signed_int:
	case ptur_failed_unsigned_int:
	case ptur_failed_pointer:
		break;

	case ptur_failed_str:
		free(test->result.failed.variant.str.expected);
		free(test->result.failed.variant.str.actual);
		break;
	}
}

struct ptunit_suite ptunit_mk_suite(int argc, char **argv)
{
	struct ptunit_suite suite;

	memset(&suite, 0, sizeof(suite));

	if (argc && argv)
		suite.name = argv[0];
	return suite;
}

static void ptunit_print_test(const struct ptunit_test *test)
{
	fprintf(stderr, "%s", test->name);

	if (test->args)
		fprintf(stderr, "(%s)", test->args);

	fprintf(stderr, ": ");
}

static const char *basename(const char *file)
{
	const char *base;

	if (!file)
		return NULL;

	for (base = file + strlen(file); base != file; base -= 1) {
		char ch;

		ch = base[-1];
		if ((ch == '/') || (ch == '\\'))
			break;
	}

	return base;
}

static void ptunit_print_srcloc(const struct ptunit_test *test)
{
	const char *file;

	switch (test->result.type) {
	case ptur_passed:
	case ptur_skipped:
		fprintf(stderr, "n/a: ");
		break;

	case ptur_failed_signed_int:
	case ptur_failed_unsigned_int:
	case ptur_failed_pointer:
	case ptur_failed_str:
		file = basename(test->result.failed.where.file);
		if (!file)
			file = "<unknown>";

		fprintf(stderr, "%s:%" PRIu32 ": ", file,
			test->result.failed.where.line);
		break;
	}
}

static void ptunit_report_test(const struct ptunit_test *test)
{
	switch (test->result.type) {
	case ptur_skipped:
	case ptur_passed:
		return;

	case ptur_failed_signed_int:
		ptunit_print_test(test);
		ptunit_print_srcloc(test);
		fprintf(stderr, "%s [%" PRId64 "%s%" PRId64 "] failed.\n",
			test->result.failed.variant.signed_int.expr,
			test->result.failed.variant.signed_int.actual,
			test->result.failed.variant.signed_int.cmp,
			test->result.failed.variant.signed_int.expected);
		return;

	case ptur_failed_unsigned_int:
		ptunit_print_test(test);
		ptunit_print_srcloc(test);
		fprintf(stderr, "%s [0x%" PRIx64 "%s0x%" PRIx64 "] failed.\n",
			test->result.failed.variant.unsigned_int.expr,
			test->result.failed.variant.unsigned_int.actual,
			test->result.failed.variant.unsigned_int.cmp,
			test->result.failed.variant.unsigned_int.expected);
		return;

	case ptur_failed_pointer:
		ptunit_print_test(test);
		ptunit_print_srcloc(test);
		fprintf(stderr, "%s [%p%s%p] failed.\n",
			test->result.failed.variant.pointer.expr,
			test->result.failed.variant.pointer.actual,
			test->result.failed.variant.pointer.cmp,
			test->result.failed.variant.pointer.expected);
		return;

	case ptur_failed_str:
		ptunit_print_test(test);
		ptunit_print_srcloc(test);
		fprintf(stderr, "%s [%s%s%s] failed.\n",
			test->result.failed.variant.str.expr,
			test->result.failed.variant.str.actual,
			test->result.failed.variant.str.cmp,
			test->result.failed.variant.str.expected);
		return;
	}

	ptunit_print_test(test);
	fprintf(stderr, "bad result type: 0x%" PRIx32 ".\n", test->result.type);
}

void ptunit_log_test(struct ptunit_suite *suite,
		     const struct ptunit_test *test)
{
	if (!test)
		return;

	if (suite) {
		suite->nr_tests += 1;

		if (test->result.type == ptur_skipped)
			suite->nr_skips += 1;
		else if (test->result.type != ptur_passed)
			suite->nr_fails += 1;
	}

	ptunit_report_test(test);
}

int ptunit_report(const struct ptunit_suite *suite)
{
	if (!suite)
		return -1;

	if (suite->name)
		fprintf(stdout, "%s: ", suite->name);

	fprintf(stdout,
		"tests: %" PRIu32 ", passes: %" PRIu32 ", fails: %" PRIu32,
		suite->nr_tests,
		suite->nr_tests - (suite->nr_fails + suite->nr_skips),
		suite->nr_fails);

	if (suite->nr_skips)
		fprintf(stdout, " (skipped: %" PRIu32 ")", suite->nr_skips);

	fprintf(stdout, "\n");

	if (INT32_MAX < suite->nr_fails)
		return INT32_MAX;

	return (int) suite->nr_fails;
}
