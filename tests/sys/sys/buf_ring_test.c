/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
 */

#include <sys/types.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>

#include <errno.h>
#include <stdint.h>

#include <atf-c.h>

static void critical_enter(void);
static void critical_exit(void);

#include <sys/buf_ring.h>

static void
critical_enter(void)
{
}

static void
critical_exit(void)
{
}

static void *
buf_ring_dequeue_peek(struct buf_ring *br)
{
	void *val;

	val = buf_ring_peek(br);
	if (val != NULL)
		buf_ring_advance_sc(br);
	return (val);
}

static void *
buf_ring_dequeue_peek_clear_sc(struct buf_ring *br)
{
	void *val;

	val = buf_ring_peek_clear_sc(br);
	if (val != NULL)
		buf_ring_advance_sc(br);
	return (val);
}

#define	MC_SC_TEST(dequeue_func)					\
ATF_TC_WITHOUT_HEAD(dequeue_func);					\
ATF_TC_BODY(dequeue_func, tc)						\
{									\
	struct buf_ring *br;						\
									\
	br = buf_ring_alloc(4);						\
	ATF_REQUIRE_MSG(br != NULL, "buf_ring_alloc returned NULL");	\
									\
	ATF_REQUIRE(dequeue_func(br) == NULL);				\
	ATF_REQUIRE(buf_ring_count(br) == 0);				\
	ATF_REQUIRE(!buf_ring_full(br));				\
	ATF_REQUIRE(buf_ring_empty(br));				\
									\
	/* Try filling the buf_ring */					\
	ATF_REQUIRE(buf_ring_enqueue(br, (void *)1) == 0);		\
	ATF_REQUIRE(buf_ring_enqueue(br, (void *)2) == 0);		\
	ATF_REQUIRE(buf_ring_enqueue(br, (void *)3) == 0);		\
	ATF_REQUIRE(buf_ring_enqueue(br, (void *)4) == ENOBUFS);	\
									\
	ATF_REQUIRE(buf_ring_count(br) == 3);				\
	ATF_REQUIRE(buf_ring_full(br));					\
	ATF_REQUIRE(!buf_ring_empty(br));				\
									\
	/* Partially empty it */					\
	ATF_REQUIRE(dequeue_func(br) == (void *)1);			\
	ATF_REQUIRE(dequeue_func(br) == (void *)2);			\
									\
	ATF_REQUIRE(buf_ring_count(br) == 1);				\
	ATF_REQUIRE(!buf_ring_full(br));				\
	ATF_REQUIRE(!buf_ring_empty(br));				\
									\
	/* Add more items */						\
	ATF_REQUIRE(buf_ring_enqueue(br, (void *)5) == 0);		\
	ATF_REQUIRE(buf_ring_count(br) == 2);				\
									\
	/* Finish emptying it */					\
	ATF_REQUIRE(dequeue_func(br) == (void *)3);			\
	ATF_REQUIRE(dequeue_func(br) == (void *)5);			\
	ATF_REQUIRE(dequeue_func(br) == NULL);				\
									\
	ATF_REQUIRE(buf_ring_count(br) == 0);				\
	ATF_REQUIRE(!buf_ring_full(br));				\
	ATF_REQUIRE(buf_ring_empty(br));				\
									\
	for (uintptr_t i = 0; i < 8; i++) {				\
		ATF_REQUIRE(buf_ring_enqueue(br, (void *)(i + 100)) == 0);  \
		ATF_REQUIRE(buf_ring_enqueue(br, (void *)(i + 200)) == 0);  \
		ATF_REQUIRE(buf_ring_enqueue(br, (void *)(i + 300)) == 0);  \
		ATF_REQUIRE(buf_ring_count(br) == 3);			\
		ATF_REQUIRE(dequeue_func(br) == (void *)(i + 100));	\
		ATF_REQUIRE(dequeue_func(br) == (void *)(i + 200));	\
		ATF_REQUIRE(dequeue_func(br) == (void *)(i + 300));	\
									\
		ATF_REQUIRE(!buf_ring_full(br));			\
		ATF_REQUIRE(buf_ring_empty(br));			\
	}								\
									\
	buf_ring_free(br);						\
}

MC_SC_TEST(buf_ring_dequeue_sc)
MC_SC_TEST(buf_ring_dequeue_mc)
MC_SC_TEST(buf_ring_dequeue_peek)
MC_SC_TEST(buf_ring_dequeue_peek_clear_sc)

ATF_TC_WITHOUT_HEAD(overflow);
ATF_TC_BODY(overflow, tc)
{
	struct buf_ring *br;

	br = buf_ring_alloc(4);
	ATF_REQUIRE_MSG(br != NULL, "buf_ring_alloc returned NULL");

	br->br_prod_head = br->br_cons_head = br->br_prod_tail =
	    br->br_cons_tail = UINT32_MAX - 1;
	ATF_REQUIRE(buf_ring_count(br) == 0);
	ATF_REQUIRE(!buf_ring_full(br));
	ATF_REQUIRE(buf_ring_empty(br));

	ATF_REQUIRE(buf_ring_enqueue(br, (void *)1) == 0);
	ATF_REQUIRE(buf_ring_count(br) == 1);
	ATF_REQUIRE(!buf_ring_full(br));
	ATF_REQUIRE(!buf_ring_empty(br));

	ATF_REQUIRE(buf_ring_enqueue(br, (void *)2) == 0);
	ATF_REQUIRE(buf_ring_count(br) == 2);
	ATF_REQUIRE(!buf_ring_full(br));
	ATF_REQUIRE(!buf_ring_empty(br));

	ATF_REQUIRE(buf_ring_enqueue(br, (void *)3) == 0);
	ATF_REQUIRE(buf_ring_count(br) == 3);
	ATF_REQUIRE(buf_ring_full(br));
	ATF_REQUIRE(!buf_ring_empty(br));

	ATF_REQUIRE(br->br_prod_head == 1);
	ATF_REQUIRE(br->br_prod_tail == 1);
	ATF_REQUIRE(br->br_cons_head == UINT32_MAX - 1);
	ATF_REQUIRE(br->br_cons_tail == UINT32_MAX - 1);

	ATF_REQUIRE(buf_ring_dequeue_sc(br) == (void *)1);
	ATF_REQUIRE(buf_ring_count(br) == 2);
	ATF_REQUIRE(!buf_ring_full(br));
	ATF_REQUIRE(!buf_ring_empty(br));

	ATF_REQUIRE(buf_ring_dequeue_sc(br) == (void *)2);
	ATF_REQUIRE(buf_ring_count(br) == 1);
	ATF_REQUIRE(!buf_ring_full(br));
	ATF_REQUIRE(!buf_ring_empty(br));

	ATF_REQUIRE(buf_ring_dequeue_sc(br) == (void *)3);
	ATF_REQUIRE(buf_ring_count(br) == 0);
	ATF_REQUIRE(!buf_ring_full(br));
	ATF_REQUIRE(buf_ring_empty(br));

	buf_ring_free(br);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, buf_ring_dequeue_sc);
	ATF_TP_ADD_TC(tp, buf_ring_dequeue_mc);
	ATF_TP_ADD_TC(tp, buf_ring_dequeue_peek);
	ATF_TP_ADD_TC(tp, buf_ring_dequeue_peek_clear_sc);
	ATF_TP_ADD_TC(tp, overflow);
	return (atf_no_error());
}
