#include "config.h"

#include "recvbuff.h"

#include "unity.h"

void setUp(void);
void test_Initialization(void);
void test_GetAndFree(void);
void test_GetAndFill(void);

void
setUp(void)
{
	init_recvbuff(RECV_INIT);
}

void
test_Initialization(void) {
	TEST_ASSERT_EQUAL_UINT(RECV_INIT, free_recvbuffs());
	TEST_ASSERT_EQUAL_UINT(0, full_recvbuffs());
	TEST_ASSERT_FALSE(has_full_recv_buffer());
	TEST_ASSERT_NULL(get_full_recv_buffer());
}

void
test_GetAndFree(void) {
	u_long initial = free_recvbuffs();
	recvbuf_t* buf = get_free_recv_buffer();

	TEST_ASSERT_EQUAL_UINT(initial-1, free_recvbuffs());
	freerecvbuf(buf);
	TEST_ASSERT_EQUAL_UINT(initial, free_recvbuffs());
}


void
test_GetAndFill(void) {
	int initial = free_recvbuffs();
	recvbuf_t* buf = get_free_recv_buffer();

	add_full_recv_buffer(buf);
	TEST_ASSERT_EQUAL_UINT(1, full_recvbuffs());
	TEST_ASSERT_TRUE(has_full_recv_buffer());
	TEST_ASSERT_EQUAL_PTR(buf, get_full_recv_buffer());
}
