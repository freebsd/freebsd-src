/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * This software was developed by Olivier Certner <olce@FreeBSD.org> at
 * Kumacom SARL under sponsorship from the FreeBSD Foundation.
 */

#include <sys/types.h>
#define QUEUE_MACRO_DEBUG_ASSERTIONS
#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

/*
 * General utilities.
 */
#define DIAG(fmt, ...)	do {						\
	fprintf(stderr, "%s(): " fmt "\n", __func__, ##__VA_ARGS__);	\
} while (0)

/*
 * Common definitions and utilities.
 *
 * 'type' should be tailq, stailq, list or slist.  'TYPE' is 'type' in
 * uppercase.
 */

#define QUEUE_TESTS_COMMON(type, TYPE)					\
/*									\
 * Definitions and utilities.						\
 */									\
									\
struct type ## _id_elem {						\
	TYPE ## _ENTRY(type ## _id_elem) ie_entry;			\
	u_int ie_id;							\
};									\
									\
TYPE ## _HEAD(type ## _ids, type ## _id_elem);				\
									\
static void								\
type ## _check(const struct type ## _ids *const type,			\
    const u_int nb, const u_int id_shift);				\
									\
/*									\
 * Creates a tailq/list with 'nb' elements with contiguous IDs		\
 * in ascending order starting at 'id_shift'.				\
 */									\
static struct type ## _ids *						\
type ## _create(const u_int nb, const u_int id_shift)			\
{									\
	struct type ## _ids *const type =				\
	    malloc(sizeof(*type));					\
									\
	ATF_REQUIRE_MSG(type != NULL,					\
	    "Cannot malloc " #type " head");				\
									\
	TYPE ## _INIT(type);						\
	for (u_int i = 0; i < nb; ++i) {				\
		struct type ## _id_elem *const e =			\
		    malloc(sizeof(*e));					\
									\
		ATF_REQUIRE_MSG(e != NULL,				\
		    "Cannot malloc " #type " element %u", i);		\
		e->ie_id = nb - 1 - i + id_shift;			\
		TYPE ## _INSERT_HEAD(type, e, ie_entry);		\
	}								\
									\
	DIAG("Created " #type " %p with %u elements",			\
	    type, nb);							\
	type ## _check(type, nb, id_shift);				\
	return (type);							\
}									\
									\
/* Performs no check. */						\
static void								\
type ## _destroy(struct type ## _ids *const type)			\
{									\
	struct type ## _id_elem *e, *tmp_e;				\
									\
	DIAG("Destroying " #type" %p", type);				\
	TYPE ## _FOREACH_SAFE(e, type, ie_entry,			\
	    tmp_e) {							\
		free(e);						\
	}								\
	free(type);							\
}									\
									\
									\
/* Checks that some tailq/list is as produced by *_create(). */		\
static void								\
type ## _check(const struct type ## _ids *const type,			\
    const u_int nb, const u_int id_shift)				\
{									\
	struct type ## _id_elem *e;					\
	u_int i = 0;							\
									\
	TYPE ## _FOREACH(e, type, ie_entry) {				\
		ATF_REQUIRE_MSG(i + 1 <= nb,				\
		    #type " %p has more than %u elements",		\
		    type, nb);						\
		ATF_REQUIRE_MSG(e->ie_id == i + id_shift,		\
		    #type " %p element %p: Found ID %u, "		\
		    "expected %u",					\
		    type, e, e->ie_id, i + id_shift);			\
		++i;							\
	}								\
	ATF_REQUIRE_MSG(i == nb,					\
	    #type " %p has only %u elements, expected %u",		\
	    type, i, nb);						\
}									\
									\
/* Returns NULL if not enough elements. */				\
static struct type ## _id_elem *					\
type ## _nth(const struct type ## _ids *const type,			\
    const u_int idx)							\
{									\
	struct type ## _id_elem *e;					\
	u_int i = 0;							\
									\
	TYPE ## _FOREACH(e, type, ie_entry) {				\
		if (i == idx) {						\
			DIAG(#type " %p has element %p "		\
			    "(ID %u) at index %u",			\
			    type, e, e->ie_id, idx);			\
			return (e);					\
		}							\
		++i;							\
	}								\
	DIAG(#type " %p: Only %u elements, no index %u",		\
	    type, i, idx);						\
	return (NULL);							\
}									\
									\
/*									\
 * Tests.								\
 */									\
									\
ATF_TC(type ## _split_after_and_concat);				\
ATF_TC_HEAD(type ## _split_after_and_concat, tc)			\
{									\
	atf_tc_set_md_var(tc, "descr",					\
	    "Test " #TYPE "_SPLIT_AFTER() followed by "			\
	    #TYPE "_CONCAT()");						\
}									\
ATF_TC_BODY(type ## _split_after_and_concat, tc)			\
{									\
	struct type ## _ids *const type =				\
	    type ## _create(100, 0);					\
	struct type ## _ids rest;					\
	struct type ## _id_elem *e;					\
									\
	e = type ## _nth(type, 49);					\
	TYPE ## _SPLIT_AFTER(type, e, &rest, ie_entry);			\
	type ## _check(type, 50, 0);					\
	type ## _check(&rest, 50, 50);					\
	QUEUE_TESTS_ ## TYPE ## _CONCAT(type, &rest);			\
	ATF_REQUIRE_MSG(TYPE ## _EMPTY(&rest),				\
	    "'rest' not empty after concat");				\
	type ## _check(type, 100, 0);					\
	type ## _destroy(type);						\
}

#define QUEUE_TESTS_CHECK_REVERSED(type, TYPE)				\
/*									\
 * Checks that some tailq/list is reversed.				\
 */									\
static void								\
type ## _check_reversed(const struct type ## _ids *const type,		\
    const u_int nb, const u_int id_shift)				\
{									\
	struct type ## _id_elem *e;					\
	u_int i = 0;							\
									\
	TYPE ## _FOREACH(e, type, ie_entry) {				\
		const u_int expected_id = nb - 1 - i + id_shift;	\
									\
		ATF_REQUIRE_MSG(i < nb,					\
		    #type " %p has more than %u elements",		\
		    type, nb);						\
		ATF_REQUIRE_MSG(e->ie_id == expected_id,		\
		    #type " %p element %p, idx %u: Found ID %u, "	\
		    "expected %u",					\
		    type, e, i, e->ie_id, expected_id);			\
		++i;							\
	}								\
	ATF_REQUIRE_MSG(i == nb,					\
	    #type " %p has only %u elements, expected %u",		\
	    type, i, nb);						\
}

/*
 * Paper over the *_CONCAT() signature differences.
 */

#define QUEUE_TESTS_TAILQ_CONCAT(first, second)				\
	TAILQ_CONCAT(first, second, ie_entry)

#define QUEUE_TESTS_LIST_CONCAT(first, second)				\
	LIST_CONCAT(first, second, list_id_elem, ie_entry)

#define QUEUE_TESTS_STAILQ_CONCAT(first, second)			\
	STAILQ_CONCAT(first, second)

#define QUEUE_TESTS_SLIST_CONCAT(first, second)				\
	SLIST_CONCAT(first, second, slist_id_elem, ie_entry)

/*
 * ATF test registration.
 */

#define QUEUE_TESTS_REGISTRATION(tp, type)				\
	ATF_TP_ADD_TC(tp, type ## _split_after_and_concat)

/*
 * Macros defining print functions.
 *
 * They are currently not used in the tests above, but are useful for debugging.
 */

#define QUEUE_TESTS_TQ_PRINT(type, hfp)					\
	static void							\
	type ## _print(const struct type ## _ids *const type)		\
	{								\
		printf(#type " %p: " __STRING(hfp ## _first)		\
		    " = %p, " __STRING(hfp ## _last) " = %p\n",		\
		    type, type->hfp ## _first, type->hfp ## _last);	\
	}

#define QUEUE_TESTS_L_PRINT(type, hfp)					\
	static void							\
	type ## _print(const struct type ## _ids *const type)		\
	{								\
		printf(#type " %p: " __STRING(hfp ## _first) " = %p\n",	\
		    type, type->hfp ## _first);				\
	}


/*
 * Meat.
 */

/* Common tests. */
QUEUE_TESTS_COMMON(tailq, TAILQ);
QUEUE_TESTS_COMMON(list, LIST);
QUEUE_TESTS_COMMON(stailq, STAILQ);
QUEUE_TESTS_COMMON(slist, SLIST);

/* STAILQ_REVERSE(). */
QUEUE_TESTS_CHECK_REVERSED(stailq, STAILQ);
ATF_TC(stailq_reverse);
ATF_TC_HEAD(stailq_reverse, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test STAILQ_REVERSE");
}
ATF_TC_BODY(stailq_reverse, tc)
{
	const u_int size = 100;
	struct stailq_ids *const stailq = stailq_create(size, 0);
	struct stailq_ids *const empty_stailq = stailq_create(0, 0);
	const struct stailq_id_elem *last;

	stailq_check(stailq, size, 0);
	STAILQ_REVERSE(stailq, stailq_id_elem, ie_entry);
	stailq_check_reversed(stailq, size, 0);
	last = STAILQ_LAST(stailq, stailq_id_elem, ie_entry);
	ATF_REQUIRE_MSG(last->ie_id == 0,
	    "Last element of stailq %p has id %u, expected 0",
	    stailq, last->ie_id);
	stailq_destroy(stailq);

	STAILQ_REVERSE(empty_stailq, stailq_id_elem, ie_entry);
	stailq_check(empty_stailq, 0, 0);
	stailq_destroy(empty_stailq);
}

/*
 * Main.
 */
ATF_TP_ADD_TCS(tp)
{
	QUEUE_TESTS_REGISTRATION(tp, tailq);
	QUEUE_TESTS_REGISTRATION(tp, list);
	QUEUE_TESTS_REGISTRATION(tp, stailq);
	QUEUE_TESTS_REGISTRATION(tp, slist);
	ATF_TP_ADD_TC(tp, stailq_reverse);

	return (atf_no_error());
}
