#include "sntptest.h"

extern "C" {
#include "kod_management.h"
};

class kodDatabaseTest : public sntptest {
protected:
	virtual void SetUp() {
		kod_init_kod_db("/dev/null", TRUE);
	}
};

TEST_F(kodDatabaseTest, SingleEntryHandling) {
	char HOST[] = "192.0.2.5";
	char REASON[] = "DENY";

	add_entry(HOST, REASON);

	kod_entry* result;

	EXPECT_EQ(1, search_entry(HOST, &result));
	EXPECT_STREQ(HOST, result->hostname);
	EXPECT_STREQ(REASON, result->type);
}

TEST_F(kodDatabaseTest, MultipleEntryHandling) {
	char HOST1[] = "192.0.2.3";
	char REASON1[] = "DENY";

	char HOST2[] = "192.0.5.5";
	char REASON2[] = "RATE";

	char HOST3[] = "192.0.10.1";
	char REASON3[] = "DENY";

	add_entry(HOST1, REASON1);
	add_entry(HOST2, REASON2);
	add_entry(HOST3, REASON3);

	kod_entry* result;

	EXPECT_EQ(1, search_entry(HOST1, &result));
	EXPECT_STREQ(HOST1, result->hostname);
	EXPECT_STREQ(REASON1, result->type);

	EXPECT_EQ(1, search_entry(HOST2, &result));
	EXPECT_STREQ(HOST2, result->hostname);
	EXPECT_STREQ(REASON2, result->type);

	EXPECT_EQ(1, search_entry(HOST3, &result));
	EXPECT_STREQ(HOST3, result->hostname);
	EXPECT_STREQ(REASON3, result->type);

	free(result);
}

TEST_F(kodDatabaseTest, NoMatchInSearch) {
	char HOST_ADD[] = "192.0.2.6";
	char HOST_NOTADD[] = "192.0.6.1";
	char REASON[] = "DENY";

	add_entry(HOST_ADD, REASON);

	kod_entry* result;

	EXPECT_EQ(0, search_entry(HOST_NOTADD, &result));
	EXPECT_TRUE(result == NULL);
}

TEST_F(kodDatabaseTest, AddDuplicate) {
	char HOST[] = "192.0.2.3";
	char REASON1[] = "RATE";
	char REASON2[] = "DENY";

	add_entry(HOST, REASON1);
	kod_entry* result1;
	ASSERT_EQ(1, search_entry(HOST, &result1));

	/* 
	 * Sleeps for two seconds since we want to ensure that
	 * the timestamp is updated to a new value.
	 */
	sleep(2);

	add_entry(HOST, REASON2);
	kod_entry* result2;
	ASSERT_EQ(1, search_entry(HOST, &result2));

	EXPECT_NE(result1->timestamp, result2->timestamp);

	free(result1);
	free(result2);
}

TEST_F(kodDatabaseTest, DeleteEntry) {
	char HOST1[] = "192.0.2.1";
	char HOST2[] = "192.0.2.2";
	char HOST3[] = "192.0.2.3";
	char REASON[] = "DENY";

	add_entry(HOST1, REASON);
	add_entry(HOST2, REASON);
	add_entry(HOST3, REASON);

	kod_entry* result;
	
	ASSERT_EQ(1, search_entry(HOST2, &result));
	free(result);

	delete_entry(HOST2, REASON);

	EXPECT_EQ(0, search_entry(HOST2, &result));

	// Ensure that the other entry is still there.
	EXPECT_EQ(1, search_entry(HOST1, &result));
	free(result);
}
