/* 	$OpenBSD: test_sshbuf.c,v 1.3 2025/12/30 00:12:58 djm Exp $ */
/*
 * Regress test for sshbuf.h buffer API
 *
 * Placed in the public domain
 */

#define SSHBUF_INTERNAL 1	/* access internals for testing */
#include "includes.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "ssherr.h"
#include "sshbuf.h"

void sshbuf_tests(void);

#ifndef roundup
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

void
sshbuf_tests(void)
{
	struct sshbuf *p1, *p2, *p3;
	u_int v32;
	const u_char *cdp;
	u_char *dp;
	size_t sz;
	int r;

	TEST_START("allocate sshbuf");
	p1 = sshbuf_new();
	ASSERT_PTR_NE(p1, NULL);
	TEST_DONE();

	TEST_START("max size on fresh buffer");
	ASSERT_SIZE_T_GT(sshbuf_max_size(p1), 0);
	TEST_DONE();

	TEST_START("available on fresh buffer");
	ASSERT_SIZE_T_GT(sshbuf_avail(p1), 0);
	TEST_DONE();

	TEST_START("len = 0 on empty buffer");
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 0);
	TEST_DONE();

	TEST_START("set valid max size");
	ASSERT_INT_EQ(sshbuf_set_max_size(p1, 65536), 0);
	ASSERT_SIZE_T_EQ(sshbuf_max_size(p1), 65536);
	TEST_DONE();

	TEST_START("available on limited buffer");
	ASSERT_SIZE_T_EQ(sshbuf_avail(p1), 65536);
	TEST_DONE();

	TEST_START("free");
	sshbuf_free(p1);
	TEST_DONE();

	TEST_START("consume on empty buffer");
	p1 = sshbuf_new();
	ASSERT_PTR_NE(p1, NULL);
	ASSERT_INT_EQ(sshbuf_consume(p1, 0), 0);
	ASSERT_INT_EQ(sshbuf_consume(p1, 1), SSH_ERR_MESSAGE_INCOMPLETE);
	sshbuf_free(p1);
	TEST_DONE();

	TEST_START("consume_end on empty buffer");
	p1 = sshbuf_new();
	ASSERT_PTR_NE(p1, NULL);
	ASSERT_INT_EQ(sshbuf_consume_end(p1, 0), 0);
	ASSERT_INT_EQ(sshbuf_consume_end(p1, 1), SSH_ERR_MESSAGE_INCOMPLETE);
	sshbuf_free(p1);
	TEST_DONE();

	TEST_START("reserve space");
	p1 = sshbuf_new();
	ASSERT_PTR_NE(p1, NULL);
	r = sshbuf_reserve(p1, 1, &dp);
	ASSERT_INT_EQ(r, 0);
	ASSERT_PTR_NE(dp, NULL);
	*dp = 0x11;
	r = sshbuf_reserve(p1, 3, &dp);
	ASSERT_INT_EQ(r, 0);
	ASSERT_PTR_NE(dp, NULL);
	*dp++ = 0x22;
	*dp++ = 0x33;
	*dp++ = 0x44;
	TEST_DONE();

	TEST_START("sshbuf_len on filled buffer");
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 4);
	TEST_DONE();

	TEST_START("sshbuf_ptr on filled buffer");
	cdp = sshbuf_ptr(p1);
	ASSERT_PTR_NE(cdp, NULL);
	ASSERT_U8_EQ(cdp[0], 0x11);
	ASSERT_U8_EQ(cdp[1], 0x22);
	ASSERT_U8_EQ(cdp[2], 0x33);
	ASSERT_U8_EQ(cdp[3], 0x44);
	TEST_DONE();

	TEST_START("consume on filled buffer");
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 4);
	ASSERT_INT_EQ(sshbuf_consume(p1, 0), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 4);
	r = sshbuf_consume(p1, 64);
	ASSERT_INT_EQ(r, SSH_ERR_MESSAGE_INCOMPLETE);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 4);
	ASSERT_INT_EQ(sshbuf_consume(p1, 1), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 3);
	cdp = sshbuf_ptr(p1);
	ASSERT_PTR_NE(p1, NULL);
	ASSERT_U8_EQ(cdp[0], 0x22);
	ASSERT_INT_EQ(sshbuf_consume(p1, 2), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 1);
	cdp = sshbuf_ptr(p1);
	ASSERT_PTR_NE(p1, NULL);
	ASSERT_U8_EQ(cdp[0], 0x44);
	r = sshbuf_consume(p1, 2);
	ASSERT_INT_EQ(r, SSH_ERR_MESSAGE_INCOMPLETE);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 1);
	ASSERT_INT_EQ(sshbuf_consume(p1, 1), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 0);
	r = sshbuf_consume(p1, 1);
	ASSERT_INT_EQ(r, SSH_ERR_MESSAGE_INCOMPLETE);
	sshbuf_free(p1);
	TEST_DONE();

	TEST_START("consume_end on filled buffer");
	p1 = sshbuf_new();
	ASSERT_PTR_NE(p1, NULL);
	r = sshbuf_reserve(p1, 4, &dp);
	ASSERT_INT_EQ(r, 0);
	ASSERT_PTR_NE(dp, NULL);
	*dp++ = 0x11;
	*dp++ = 0x22;
	*dp++ = 0x33;
	*dp++ = 0x44;
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 4);
	r = sshbuf_consume_end(p1, 5);
	ASSERT_INT_EQ(r, SSH_ERR_MESSAGE_INCOMPLETE);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 4);
	ASSERT_INT_EQ(sshbuf_consume_end(p1, 3), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 1);
	cdp = sshbuf_ptr(p1);
	ASSERT_PTR_NE(cdp, NULL);
	ASSERT_U8_EQ(*cdp, 0x11);
	r = sshbuf_consume_end(p1, 2);
	ASSERT_INT_EQ(r, SSH_ERR_MESSAGE_INCOMPLETE);
	ASSERT_INT_EQ(sshbuf_consume_end(p1, 1), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 0);
	sshbuf_free(p1);
	TEST_DONE();

	TEST_START("fill limited buffer");
	p1 = sshbuf_new();
	ASSERT_PTR_NE(p1, NULL);
	ASSERT_INT_EQ(sshbuf_set_max_size(p1, 1223), 0);
	ASSERT_SIZE_T_EQ(sshbuf_max_size(p1), 1223);
	ASSERT_SIZE_T_EQ(sshbuf_avail(p1), 1223);
	r = sshbuf_reserve(p1, 1223, &dp);
	ASSERT_INT_EQ(r, 0);
	ASSERT_PTR_NE(dp, NULL);
	memset(dp, 0xd7, 1223);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 1223);
	ASSERT_SIZE_T_EQ(sshbuf_avail(p1), 0);
	r = sshbuf_reserve(p1, 1, &dp);
	ASSERT_INT_EQ(r, SSH_ERR_NO_BUFFER_SPACE);
	ASSERT_PTR_EQ(dp, NULL);
	TEST_DONE();

	TEST_START("consume and force compaction");
	ASSERT_INT_EQ(sshbuf_consume(p1, 223), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 1000);
	ASSERT_SIZE_T_EQ(sshbuf_avail(p1), 223);
	r = sshbuf_reserve(p1, 224, &dp);
	ASSERT_INT_EQ(r, SSH_ERR_NO_BUFFER_SPACE);
	ASSERT_PTR_EQ(dp, NULL);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 1000);
	ASSERT_SIZE_T_EQ(sshbuf_avail(p1), 223);
	r = sshbuf_reserve(p1, 223, &dp);
	ASSERT_INT_EQ(r, 0);
	ASSERT_PTR_NE(dp, NULL);
	memset(dp, 0x7d, 223);
	cdp = sshbuf_ptr(p1);
	ASSERT_PTR_NE(cdp, NULL);
	ASSERT_MEM_FILLED_EQ(cdp, 0xd7, 1000);
	ASSERT_MEM_FILLED_EQ(cdp + 1000, 0x7d, 223);
	TEST_DONE();

	TEST_START("resize full buffer");
	r = sshbuf_set_max_size(p1, 1000);
	ASSERT_INT_EQ(r, SSH_ERR_NO_BUFFER_SPACE);
	sz = roundup(1223 + SSHBUF_SIZE_INC * 3, SSHBUF_SIZE_INC);
	ASSERT_INT_EQ(sshbuf_set_max_size(p1, sz), 0);
	ASSERT_SIZE_T_EQ(sshbuf_max_size(p1), sz);
	ASSERT_SIZE_T_EQ(sshbuf_avail(p1), sz - 1223);
	ASSERT_INT_EQ(sshbuf_len(p1), 1223);
	TEST_DONE();

	/* NB. uses sshbuf internals */
	TEST_START("alloc chunking");
	r = sshbuf_reserve(p1, 1, &dp);
	ASSERT_INT_EQ(r, 0);
	ASSERT_PTR_NE(dp, NULL);
	*dp = 0xff;
	cdp = sshbuf_ptr(p1);
	ASSERT_PTR_NE(cdp, NULL);
	ASSERT_MEM_FILLED_EQ(cdp, 0xd7, 1000);
	ASSERT_MEM_FILLED_EQ(cdp + 1000, 0x7d, 223);
	ASSERT_MEM_FILLED_EQ(cdp + 1223, 0xff, 1);
	ASSERT_SIZE_T_EQ(sshbuf_alloc(p1) % SSHBUF_SIZE_INC, 0);
	sshbuf_free(p1);
	TEST_DONE();

	TEST_START("reset buffer");
	p1 = sshbuf_new();
	ASSERT_PTR_NE(p1, NULL);
	ASSERT_INT_EQ(sshbuf_set_max_size(p1, 1223), 0);
	ASSERT_SIZE_T_EQ(sshbuf_max_size(p1), 1223);
	r = sshbuf_reserve(p1, 1223, &dp);
	ASSERT_INT_EQ(r, 0);
	ASSERT_PTR_NE(dp, NULL);
	memset(dp, 0xd7, 1223);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 1223);
	sshbuf_reset(p1);
	ASSERT_SIZE_T_EQ(sshbuf_max_size(p1), 1223);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 0);
	ASSERT_SIZE_T_EQ(sshbuf_avail(p1), 1223);
	sshbuf_free(p1);
	TEST_DONE();

	TEST_START("sshbuf_consume_upto_child");
	p1 = sshbuf_new();
	ASSERT_PTR_NE(p1, NULL);
	p2 = sshbuf_new();
	ASSERT_PTR_NE(p2, NULL);
	/* Unrelated buffers */
	ASSERT_INT_EQ(sshbuf_consume_upto_child(p1, p2),
	    SSH_ERR_INVALID_ARGUMENT);
	/* Simple success case */
	ASSERT_INT_EQ(sshbuf_put_u32(p1, 0xdeadbeef), 0);
	ASSERT_INT_EQ(sshbuf_put_u32(p1, 0x01020304), 0);
	ASSERT_INT_EQ(sshbuf_put_u32(p1, 0xfeedface), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), 12);
	p3 = sshbuf_fromb(p1);
	ASSERT_PTR_NE(p3, NULL);
	ASSERT_INT_EQ(sshbuf_get_u32(p3, &v32), 0);
	ASSERT_U32_EQ(v32, 0xdeadbeef);
	ASSERT_SIZE_T_EQ(sshbuf_len(p3), 8);
	ASSERT_INT_EQ(sshbuf_consume_upto_child(p1, p3), 0);
	ASSERT_SIZE_T_EQ(sshbuf_len(p1), sshbuf_len(p3));
	ASSERT_PTR_EQ(sshbuf_ptr(p1), sshbuf_ptr(p3));
	sshbuf_free(p3);
	/* Parent already consumed past child */
	p3 = sshbuf_fromb(p1);
	ASSERT_PTR_NE(p3, NULL);
	ASSERT_INT_EQ(sshbuf_get_u32(p1, &v32), 0);
	ASSERT_U32_EQ(v32, 0x01020304);
	ASSERT_INT_EQ(sshbuf_consume_upto_child(p1, p3),
	    SSH_ERR_INVALID_ARGUMENT);
	sshbuf_free(p1);
	sshbuf_free(p2);
	sshbuf_free(p3);
	TEST_DONE();
}
