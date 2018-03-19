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


static struct ptunit_result cmp_pass(void)
{
	int zero = 0, one = 1, neg = -1;
	const char *szero = "zero", *sone = "one", *null = NULL;

	ptu_int_eq(zero, 0);
	ptu_int_ne(zero, one);
	ptu_int_lt(neg, 0);
	ptu_int_gt(zero, neg);

	ptu_uint_eq(zero, 0);
	ptu_uint_ne(zero, one);
	ptu_uint_lt(zero, one);
	ptu_uint_gt(neg, one);

	ptu_ptr_eq(szero, szero);
	ptu_ptr_ne(szero, sone);
	ptu_null(null);
	ptu_ptr(szero);

	ptu_str_eq(szero, szero);
	ptu_str_ne(szero, sone);

	return ptu_passed();
}

static struct ptunit_result int_eq_fail(void)
{
	int zero = 0, one = 1;

	ptu_int_eq(zero, one);

	return ptu_skipped();
}

static struct ptunit_result int_fail(void)
{
	struct ptunit_result result;

	result = int_eq_fail();

	ptu_uint_eq(result.type, ptur_failed_signed_int);
	ptu_str_eq(result.failed.where.file, __FILE__);
	ptu_uint_lt(result.failed.where.line, __LINE__);
	ptu_str_eq(result.failed.variant.signed_int.expr, "zero==one");
	ptu_str_eq(result.failed.variant.signed_int.cmp, "==");
	ptu_int_eq(result.failed.variant.signed_int.expected, 1);
	ptu_int_eq(result.failed.variant.signed_int.actual, 0);

	return ptu_passed();
}

static struct ptunit_result uint_eq_fail(void)
{
	uint16_t zero = 0, one = 1;

	ptu_uint_eq(zero, one);

	return ptu_skipped();
}

static struct ptunit_result uint_fail(void)
{
	struct ptunit_result result;

	result = uint_eq_fail();

	ptu_uint_eq(result.type, ptur_failed_unsigned_int);
	ptu_str_eq(result.failed.where.file, __FILE__);
	ptu_uint_lt(result.failed.where.line, __LINE__);
	ptu_str_eq(result.failed.variant.unsigned_int.expr, "zero==one");
	ptu_str_eq(result.failed.variant.unsigned_int.cmp, "==");
	ptu_int_eq(result.failed.variant.unsigned_int.expected, 1);
	ptu_int_eq(result.failed.variant.unsigned_int.actual, 0);

	return ptu_passed();
}

static int i, j, *pi = &i, *null;

static struct ptunit_result ptr_eq_fail(void)
{
	ptu_ptr_eq(pi, &j);

	return ptu_skipped();
}

static struct ptunit_result ptr_fail(void)
{
	struct ptunit_result result;

	result = ptr_eq_fail();

	ptu_uint_eq(result.type, ptur_failed_pointer);
	ptu_str_eq(result.failed.where.file, __FILE__);
	ptu_uint_lt(result.failed.where.line, __LINE__);
	ptu_str_eq(result.failed.variant.pointer.expr, "pi==&j");
	ptu_str_eq(result.failed.variant.pointer.cmp, "==");
	ptu_ptr_eq(result.failed.variant.pointer.expected, &j);
	ptu_ptr_eq(result.failed.variant.pointer.actual, &i);

	return ptu_passed();
}

static struct ptunit_result ptr_null_fail(void)
{
	ptu_null(pi);

	return ptu_skipped();
}

static struct ptunit_result null_fail(void)
{
	struct ptunit_result result;

	result = ptr_null_fail();

	ptu_uint_eq(result.type, ptur_failed_pointer);
	ptu_str_eq(result.failed.where.file, __FILE__);
	ptu_uint_lt(result.failed.where.line, __LINE__);
	ptu_str_eq(result.failed.variant.pointer.expr, "pi==NULL");
	ptu_str_eq(result.failed.variant.pointer.cmp, "==");
	ptu_ptr_eq(result.failed.variant.pointer.expected, NULL);
	ptu_ptr_eq(result.failed.variant.pointer.actual, &i);

	return ptu_passed();
}

static struct ptunit_result ptr_check_fail(void)
{
	ptu_ptr(null);

	return ptu_skipped();
}

static struct ptunit_result check_fail(void)
{
	struct ptunit_result result;

	result = ptr_check_fail();

	ptu_uint_eq(result.type, ptur_failed_pointer);
	ptu_str_eq(result.failed.where.file, __FILE__);
	ptu_uint_lt(result.failed.where.line, __LINE__);
	ptu_str_eq(result.failed.variant.pointer.expr, "null!=NULL");
	ptu_str_eq(result.failed.variant.pointer.cmp, "!=");
	ptu_ptr_eq(result.failed.variant.pointer.expected, NULL);
	ptu_ptr_eq(result.failed.variant.pointer.actual, null);

	return ptu_passed();
}

/* A unit test fixture providing a unit test struct and cleaning it up. */
struct test_fixture {
	/* A unit test. */
	struct ptunit_test test;

	/* Standard initialization and finalization functions. */
	struct ptunit_result (*init)(struct test_fixture *);
	struct ptunit_result (*fini)(struct test_fixture *);
};

static struct ptunit_result init_test_fixture(struct test_fixture *tfix)
{
	tfix->test = ptunit_mk_test(NULL, NULL);

	return ptu_passed();
}

static struct ptunit_result fini_test_fixture(struct test_fixture *tfix)
{
	ptunit_fini_test(&tfix->test);

	return ptu_passed();
}

static const char *sfoo = "foo", *sbar = "bar", *snull;

static struct ptunit_result str_eq_fail(void)
{
	ptu_str_eq(sfoo, sbar);

	return ptu_skipped();
}

static struct ptunit_result str_fail(struct test_fixture *tfix)
{
	struct ptunit_result *result = &tfix->test.result;

	*result = str_eq_fail();

	ptu_uint_eq(result->type, ptur_failed_str);
	ptu_str_eq(result->failed.where.file, __FILE__);
	ptu_uint_lt(result->failed.where.line, __LINE__);
	ptu_str_eq(result->failed.variant.str.expr, "sfoo~==sbar");
	ptu_str_eq(result->failed.variant.str.cmp, "~==");
	ptu_str_eq(result->failed.variant.str.expected, "bar");
	ptu_str_eq(result->failed.variant.str.actual, "foo");

	return ptu_passed();
}

static struct ptunit_result str_eq_null(void)
{
	ptu_str_eq(snull, sbar);

	return ptu_skipped();
}

static struct ptunit_result str_null(struct test_fixture *tfix)
{
	struct ptunit_result *result = &tfix->test.result;

	*result = str_eq_null();

	ptu_uint_eq(result->type, ptur_failed_str);
	ptu_str_eq(result->failed.where.file, __FILE__);
	ptu_uint_lt(result->failed.where.line, __LINE__);
	ptu_str_eq(result->failed.variant.str.expr, "snull~==sbar");
	ptu_str_eq(result->failed.variant.str.cmp, "~==");
	ptu_str_eq(result->failed.variant.str.expected, "bar");
	ptu_str_eq(result->failed.variant.str.actual, "(null)");

	return ptu_passed();
}

static struct ptunit_result param(int arg_i, int *arg_pi)
{
	ptu_int_eq(arg_i, i);
	ptu_ptr_eq(arg_pi, pi);

	return ptu_passed();
}

struct fixture {
	struct ptunit_result (*fini)(struct fixture *);
	uint8_t *pointer;
	struct ptunit_result (*init)(struct fixture *);
};

static struct ptunit_result init_fixture(struct fixture *pfix)
{
	pfix->pointer = malloc(42);

	return ptu_passed();
}

static struct ptunit_result fini_fixture(struct fixture *pfix)
{
	free(pfix->pointer);

	return ptu_passed();
}

static struct ptunit_result fixture(struct fixture *pfix)
{
	ptu_ptr(pfix);
	ptu_ptr(pfix->pointer);

	return ptu_passed();
}

static struct ptunit_result fixture_param(struct fixture *pfix, uint8_t *rep)
{
	ptu_ptr(pfix);
	ptu_ptr(pfix->pointer);

	free(pfix->pointer);
	pfix->pointer = rep;

	return ptu_passed();
}

static struct ptunit_result frun_pass(struct fixture *pfix)
{
	(void) pfix;

	return ptu_passed();
}

static struct ptunit_result frun_skip(struct fixture *pfix)
{
	(void) pfix;

	return ptu_skipped();
}

static struct ptunit_result frun_fail(struct fixture *pfix)
{
	ptu_null(pfix);

	return ptu_passed();
}

static struct ptunit_result frun_die(struct fixture *pfix)
{
	(void) pfix;

	*((volatile int *) NULL) = 0;

	return ptu_skipped();
}

static struct ptunit_result frun_empty_pass(void)
{
	struct fixture pfix;
	struct ptunit_result result;

	pfix.init = NULL;
	pfix.fini = NULL;
	ptunit_frun(result, frun_pass, &pfix, &pfix);

	ptu_uint_eq(result.type, ptur_passed);

	return ptu_passed();
}

static struct ptunit_result frun_init_fail(struct fixture *pfix)
{
	struct ptunit_result result;

	pfix->init = frun_fail;
	pfix->fini = frun_skip;
	ptunit_frun(result, frun_die, pfix, pfix);

	ptu_uint_eq(result.type, ptur_failed_pointer);
	ptu_str_eq(result.failed.where.file, __FILE__);
	ptu_uint_lt(result.failed.where.line, __LINE__);
	ptu_str_eq(result.failed.variant.pointer.expr, "pfix==NULL");
	ptu_str_eq(result.failed.variant.pointer.cmp, "==");
	ptu_ptr_eq(result.failed.variant.pointer.expected, NULL);
	ptu_ptr_eq(result.failed.variant.pointer.actual, pfix);

	return ptu_passed();
}

static struct ptunit_result frun_init_skip(void)
{
	struct fixture pfix;
	struct ptunit_result result;

	pfix.init = frun_skip;
	pfix.fini = frun_fail;
	ptunit_frun(result, frun_die, &pfix, &pfix);

	ptu_uint_eq(result.type, ptur_skipped);

	return ptu_passed();
}

static struct ptunit_result frun_fini_fail(struct fixture *pfix)
{
	struct ptunit_result result;

	pfix->init = NULL;
	pfix->fini = frun_fail;
	ptunit_frun(result, frun_pass, pfix, pfix);

	ptu_uint_eq(result.type, ptur_failed_pointer);
	ptu_str_eq(result.failed.where.file, __FILE__);
	ptu_uint_lt(result.failed.where.line, __LINE__);
	ptu_str_eq(result.failed.variant.pointer.expr, "pfix==NULL");
	ptu_str_eq(result.failed.variant.pointer.cmp, "==");
	ptu_ptr_eq(result.failed.variant.pointer.expected, NULL);
	ptu_ptr_eq(result.failed.variant.pointer.actual, pfix);

	return ptu_passed();
}

static struct ptunit_result frun_fini_skip(void)
{
	struct fixture pfix;
	struct ptunit_result result;

	pfix.init = NULL;
	pfix.fini = frun_skip;
	ptunit_frun(result, frun_pass, &pfix, &pfix);

	ptu_uint_eq(result.type, ptur_skipped);

	return ptu_passed();
}

static struct ptunit_result frun_fini_preserve(void)
{
	struct fixture pfix;
	struct ptunit_result result;

	pfix.init = NULL;
	pfix.fini = frun_fail;
	ptunit_frun(result, frun_skip, &pfix, &pfix);

	ptu_uint_eq(result.type, ptur_skipped);

	return ptu_passed();
}

int main(int argc, char **argv)
{
	struct ptunit_suite suite;
	struct test_fixture tfix;
	struct fixture pfix;

	suite = ptunit_mk_suite(argc, argv);

	ptu_run(suite, cmp_pass);
	ptu_run(suite, int_fail);
	ptu_run(suite, uint_fail);
	ptu_run(suite, ptr_fail);
	ptu_run(suite, null_fail);
	ptu_run(suite, check_fail);

	tfix.init = init_test_fixture;
	tfix.fini = fini_test_fixture;

	ptu_run_f(suite, str_fail, tfix);
	ptu_run_f(suite, str_null, tfix);

	pfix.pointer = NULL;
	pfix.init = init_fixture;
	pfix.fini = fini_fixture;

	ptu_run_p(suite, param, i, pi);
	ptu_run_f(suite, fixture, pfix);
	ptu_run_fp(suite, fixture_param, pfix, NULL);

	ptu_run(suite, frun_empty_pass);
	ptu_run(suite, frun_init_skip);
	ptu_run(suite, frun_fini_skip);
	ptu_run(suite, frun_fini_preserve);

	ptu_run_p(suite, frun_init_fail, &pfix);
	ptu_run_p(suite, frun_fini_fail, &pfix);

	return ptunit_report(&suite);
}
