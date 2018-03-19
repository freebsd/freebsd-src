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

#ifndef PTUNIT_H
#define PTUNIT_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


/* A source location for reporting unit test fails. */
struct ptunit_srcloc {
	/* The source file. */
	const char *file;

	/* The source line. */
	uint32_t line;
};

/* A unit test result type.
 *
 * This distinguishes the various potential results of a unit test.
 */
enum ptunit_result_type {
	/* The test has passed. */
	ptur_passed,

	/* The test has been skipped. */
	ptur_skipped,

	/* The test failed a signed/unsigned integer comparison. */
	ptur_failed_signed_int,
	ptur_failed_unsigned_int,

	/* The test failed a pointer comparison. */
	ptur_failed_pointer,

	/* The test failed a string comparison. */
	ptur_failed_str
};

/* A unit test result.
 *
 * We separate test execution and result reporting. A unit test function
 * returns a structured result that can later be used for reporting.
 */
struct ptunit_failed_signed_int {
	/* The expression that failed. */
	const char *expr;

	/* A string representation of the comparison operation. */
	const char *cmp;

	/* The expected value. */
	int64_t expected;

	/* The actual value. */
	int64_t actual;
};

struct ptunit_failed_unsigned_int {
	/* The expression that failed. */
	const char *expr;

	/* A string representation of the comparison operation. */
	const char *cmp;

	/* The expected value. */
	uint64_t expected;

	/* The actual value. */
	uint64_t actual;
};

struct ptunit_failed_pointer {
	/* The expression that failed. */
	const char *expr;

	/* A string representation of the comparison operation. */
	const char *cmp;

	/* The expected value. */
	const void *expected;

	/* The actual value. */
	const void *actual;
};

struct ptunit_failed_str {
	/* The expression that failed. */
	const char *expr;

	/* A string representation of the comparison operation. */
	const char *cmp;

	/* The expected value. */
	char *expected;

	/* The actual value. */
	char *actual;
};

struct ptunit_result {
	/* The test result type. */
	enum ptunit_result_type type;

	/* Test result details depending on the result type. */
	struct {
		/* The source location of the fail. */
		struct ptunit_srcloc where;

		union {
			struct ptunit_failed_signed_int signed_int;
			struct ptunit_failed_unsigned_int unsigned_int;
			struct ptunit_failed_pointer pointer;
			struct ptunit_failed_str str;
		} variant;
	} failed;
};

/* A unit test function. */
typedef struct ptunit_result (*ptunit_tfun_t)(void);

/* A unit test.
 *
 * This is used for logging and reporting.
 *
 * It is not used for running tests or even for storing tests to be run at a
 * later time.
 */
struct ptunit_test {
	/* The test name. */
	const char *name;

	/* The optional test arguments. */
	const char *args;

	/* The test result. */
	struct ptunit_result result;
};

/* A unit test suite.
 *
 * This is a simple summary of all tests that have been run.
 */
struct ptunit_suite {
	/* An optional suite name. */
	const char *name;

	/* The number of total tests. */
	uint32_t nr_tests;

	/* The number of tests that have been skipped. */
	uint32_t nr_skips;

	/* The number of tests that have failed. */
	uint32_t nr_fails;
};

/* Create a unit test source location. */
extern struct ptunit_srcloc ptunit_mk_srcloc(const char *file, uint32_t line);

#define ptu_here() ptunit_mk_srcloc(__FILE__, __LINE__)


/* Create unit test passed and not run results. */
extern struct ptunit_result ptunit_mk_passed(void);
extern struct ptunit_result ptunit_mk_skipped(void);

/* Create a unit test failed signed int result. */
extern struct ptunit_result ptunit_mk_failed_signed_int(const char *expr,
							const char *cmp,
							struct ptunit_srcloc,
							int64_t actual,
							int64_t expected);

#define ptunit_int_cmp(A, E, C)						\
	do {								\
		int64_t a = (A), e = (E);				\
									\
		if (!(a C e))						\
			return ptunit_mk_failed_signed_int(#A #C #E, #C, \
							   ptu_here(),	\
							   a, e);	\
	} while (0)


/* Create a unit test failed unsigned int result. */
extern struct ptunit_result ptunit_mk_failed_unsigned_int(const char *expr,
							  const char *cmp,
							  struct ptunit_srcloc,
							  uint64_t actual,
							  uint64_t expected);

#define ptunit_uint_cmp(A, E, C)					\
	do {								\
		uint64_t a = (A), e = (E);				\
									\
		if (!(a C e))						\
			return ptunit_mk_failed_unsigned_int(#A #C #E, #C, \
							     ptu_here(), \
							     a, e);	\
	} while (0)


/* Create a unit test failed pointer result. */
extern struct ptunit_result ptunit_mk_failed_pointer(const char *expr,
						     const char *cmp,
						     struct ptunit_srcloc,
						     const void *actual,
						     const void *expected);

#define ptunit_ptr_cmp(A, E, C)						\
	do {								\
		const void *a = (A), *e = (E);				\
									\
		if (!(a C e))						\
			return ptunit_mk_failed_pointer(#A #C #E, #C,	\
							ptu_here(),	\
							a, e);		\
	} while (0)


/* Create a unit test failed string result. */
extern struct ptunit_result ptunit_mk_failed_str(const char *expr,
						 const char *cmp,
						 struct ptunit_srcloc,
						 const char *actual,
						 const char *expected);

#define ptunit_str_cmp(A, E, C)						\
	do {								\
		const char *a = (A), *e = (E);				\
									\
		if (!a || !e || !(strcmp(a, e) C 0))			\
			return ptunit_mk_failed_str(#A "~"#C #E, "~"#C,	\
						    ptu_here(),		\
						    a, e);		\
	} while (0)


/* Run a sub-unit test; return on fail. */

#define ptunit_subtest(T, ...)				\
	do {						\
		struct ptunit_result result;		\
							\
		result = (T)(__VA_ARGS__);		\
		if (result.type != ptur_passed)		\
			return result;			\
	} while (0)


/* Run a sub-unit test; return on fail from here. */

#define ptunit_check(T, ...)					\
	do {							\
		struct ptunit_result result;			\
								\
		result = (T)(__VA_ARGS__);			\
		if (result.type != ptur_passed) {		\
			result.failed.where = ptu_here();	\
			return result;				\
		}						\
	} while (0)


/* Create a unit test. */
extern struct ptunit_test ptunit_mk_test(const char *name, const char *args);

/* Destroy a unit test. */
extern void ptunit_fini_test(struct ptunit_test *);

/* Create a unit test suite. */
extern struct ptunit_suite ptunit_mk_suite(int argc, char **argv);

/* Log a unit test result.
 *
 * This may also report test fails depending on the configuration.
 */
extern void ptunit_log_test(struct ptunit_suite *, const struct ptunit_test *);

/* Print a summary report for a unit test suite.
 *
 * Returns the number of failed tests (capped to fit into an int) on success.
 * Returns -1 if @suite is NULL.
 */
extern int ptunit_report(const struct ptunit_suite *suite);

/* Run a single simple unit test and log its result. */

#define ptunit_run(S, T)				\
	do {						\
		struct ptunit_test test;		\
							\
		test = ptunit_mk_test(#T, NULL);	\
		test.result = (T)();			\
							\
		ptunit_log_test(S, &test);		\
		ptunit_fini_test(&test);		\
	} while (0)


/* Run a single parameterized unit test and log its result. */

#define ptunit_run_p(S, T, ...)					\
	do {							\
		struct ptunit_test test;			\
								\
		test = ptunit_mk_test(#T, #__VA_ARGS__);	\
		test.result = (T)(__VA_ARGS__);			\
								\
		ptunit_log_test(S, &test);			\
		ptunit_fini_test(&test);			\
	} while (0)


/* Run a single unit test with fixture and an explict argument list.
 *
 * The first argument in the argument list is typically the fixture.
 */

#define ptunit_frun(R, T, F, ...)				\
	do {							\
		struct ptunit_result *pr = &(R);		\
								\
		pr->type = ptur_passed;				\
		if ((F)->init)					\
			*pr = (F)->init(F);			\
								\
		if (pr->type == ptur_passed) {			\
			*pr = (T)(__VA_ARGS__);			\
								\
			if ((F)->fini) {			\
				if (pr->type == ptur_passed)	\
					*pr = (F)->fini(F);	\
				else				\
					(void) (F)->fini(F);	\
			}					\
		}						\
	} while (0)


/* Run a single unit test with fixture and log its result. */

#define ptunit_run_f(S, T, F)					\
	do {							\
		struct ptunit_test test;			\
								\
		test = ptunit_mk_test(#T, #F);			\
								\
		ptunit_frun(test.result, T, &(F), &(F));	\
								\
		ptunit_log_test(S, &test);			\
		ptunit_fini_test(&test);			\
	} while (0)


/* Run a single parameterized unit test with fixture and log its result. */

#define ptunit_run_fp(S, T, F, ...)					\
	do {								\
		struct ptunit_test test;				\
									\
		test = ptunit_mk_test(#T, #F ", " #__VA_ARGS__);	\
									\
		ptunit_frun(test.result, T, &(F), &(F), __VA_ARGS__);	\
									\
		ptunit_log_test(S, &test);				\
		ptunit_fini_test(&test);				\
	} while (0)



/* The actual macros to be used in unit tests.
 *
 * Do not use the above ptunit_ macros directly.
 */

#define ptu_int_eq(A, E) ptunit_int_cmp(A, E, ==)
#define ptu_int_ne(A, E) ptunit_int_cmp(A, E, !=)
#define ptu_int_gt(A, E) ptunit_int_cmp(A, E, >)
#define ptu_int_ge(A, E) ptunit_int_cmp(A, E, >=)
#define ptu_int_lt(A, E) ptunit_int_cmp(A, E, <)
#define ptu_int_le(A, E) ptunit_int_cmp(A, E, <=)

#define ptu_uint_eq(A, E) ptunit_uint_cmp(A, E, ==)
#define ptu_uint_ne(A, E) ptunit_uint_cmp(A, E, !=)
#define ptu_uint_gt(A, E) ptunit_uint_cmp(A, E, >)
#define ptu_uint_ge(A, E) ptunit_uint_cmp(A, E, >=)
#define ptu_uint_lt(A, E) ptunit_uint_cmp(A, E, <)
#define ptu_uint_le(A, E) ptunit_uint_cmp(A, E, <=)

#define ptu_ptr_eq(A, E) ptunit_ptr_cmp(A, E, ==)
#define ptu_ptr_ne(A, E) ptunit_ptr_cmp(A, E, !=)
#define ptu_ptr_gt(A, E) ptunit_ptr_cmp(A, E, >)
#define ptu_ptr_ge(A, E) ptunit_ptr_cmp(A, E, >=)
#define ptu_ptr_lt(A, E) ptunit_ptr_cmp(A, E, <)
#define ptu_ptr_le(A, E) ptunit_ptr_cmp(A, E, <=)
#define ptu_null(A) ptunit_ptr_cmp(A, NULL, ==)
#define ptu_ptr(A) ptunit_ptr_cmp(A, NULL, !=)

#define ptu_str_eq(A, E) ptunit_str_cmp(A, E, ==)
#define ptu_str_ne(A, E) ptunit_str_cmp(A, E, !=)

/* Indicate that a unit test passed. */
#define ptu_passed() ptunit_mk_passed()

/* Skip a unit test. */
#define ptu_skipped() ptunit_mk_skipped()

/* Run a sub-unit test; return on fail. */
#define ptu_test(T, ...) ptunit_subtest(T, __VA_ARGS__)

/* Run a sub-unit test; return on fail from here. */
#define ptu_check(T, ...) ptunit_check(T, __VA_ARGS__)

/* Run a single unit test. */
#define ptu_run(S, T) ptunit_run(&(S), T)

/* Run a single parameterized unit test. */
#define ptu_run_p(S, T, ...) ptunit_run_p(&(S), T, __VA_ARGS__)

/* Run a single unit test with fixture. */
#define ptu_run_f(S, T, F) ptunit_run_f(&(S), T, F)

/* Run a single parameterized unit test with fixture. */
#define ptu_run_fp(S, T, F, ...) ptunit_run_fp(&(S), T, F, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* PTUNIT_H */
