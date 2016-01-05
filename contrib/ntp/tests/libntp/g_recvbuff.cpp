#include "g_libntptest.h"

extern "C" {
#include "recvbuff.h"
};

class recvbuffTest : public libntptest {
protected:
	virtual void SetUp() {
		init_recvbuff(RECV_INIT);
	}
};

TEST_F(recvbuffTest, Initialization) {
	EXPECT_EQ(RECV_INIT, free_recvbuffs());
	EXPECT_EQ(0, full_recvbuffs());
	EXPECT_FALSE(has_full_recv_buffer());
	EXPECT_TRUE(get_full_recv_buffer() == NULL);
}

TEST_F(recvbuffTest, GetAndFree) {
	int initial = free_recvbuffs();
	recvbuf_t* buf = get_free_recv_buffer();

	EXPECT_EQ(initial-1, free_recvbuffs());
	freerecvbuf(buf);
	EXPECT_EQ(initial, free_recvbuffs());
}

TEST_F(recvbuffTest, GetAndFill) {
	int initial = free_recvbuffs();
	recvbuf_t* buf = get_free_recv_buffer();

	add_full_recv_buffer(buf);
	EXPECT_EQ(1, full_recvbuffs());
	EXPECT_TRUE(has_full_recv_buffer());
	EXPECT_EQ(buf, get_full_recv_buffer());
}
