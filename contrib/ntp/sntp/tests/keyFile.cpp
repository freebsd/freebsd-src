#include "fileHandlingTest.h"

extern "C" {
#include "crypto.h"
};

class keyFileTest : public fileHandlingTest {
protected:
	::testing::AssertionResult CompareKeys(key& expected, key& actual) {
		if (expected.key_id != actual.key_id)
			return ::testing::AssertionFailure()
				<< "Expected key_id: " << expected.key_id
				<< " but was: " << actual.key_id;
		if (expected.key_len != actual.key_len)
			return ::testing::AssertionFailure()
				<< "Expected key_len: " << expected.key_len
				<< " but was: " << actual.key_len;
		if (strcmp(expected.type, actual.type) != 0)
			return ::testing::AssertionFailure()
				<< "Expected key_type: " << expected.type
				<< " but was: " << actual.type;
		if (memcmp(expected.key_seq, actual.key_seq, expected.key_len) != 0)
			return ::testing::AssertionFailure()
				<< "Key mismatch!";
		return ::testing::AssertionSuccess();
	}

	::testing::AssertionResult CompareKeys(int key_id,
					       int key_len,
					       const char* type,
					       const char* key_seq,
					       key& actual) {
		key temp;

		temp.key_id = key_id;
		temp.key_len = key_len;
		strlcpy(temp.type, type, sizeof(temp.type));
		memcpy(temp.key_seq, key_seq, key_len);

		return CompareKeys(temp, actual);
	}
};

TEST_F(keyFileTest, ReadEmptyKeyFile) {
	key* keys = NULL;

	ASSERT_EQ(0, auth_init(CreatePath("key-test-empty", INPUT_DIR).c_str(), &keys));

	EXPECT_TRUE(keys == NULL);
}

TEST_F(keyFileTest, ReadASCIIKeys) {
	key* keys = NULL;

	ASSERT_EQ(2, auth_init(CreatePath("key-test-ascii", INPUT_DIR).c_str(), &keys));

	ASSERT_TRUE(keys != NULL);

	key* result = NULL;
	get_key(40, &result);
	ASSERT_TRUE(result != NULL);
	EXPECT_TRUE(CompareKeys(40, 11, "MD5", "asciikeyTwo", *result));

	result = NULL;
	get_key(50, &result);
	ASSERT_TRUE(result != NULL);
	EXPECT_TRUE(CompareKeys(50, 11, "MD5", "asciikeyOne", *result));
}

TEST_F(keyFileTest, ReadHexKeys) {
	key* keys = NULL;

	ASSERT_EQ(3, auth_init(CreatePath("key-test-hex", INPUT_DIR).c_str(), &keys));

	ASSERT_TRUE(keys != NULL);

	key* result = NULL;
	get_key(10, &result);
	ASSERT_TRUE(result != NULL);
	EXPECT_TRUE(CompareKeys(10, 13, "MD5",
		 "\x01\x23\x45\x67\x89\xab\xcd\xef\x01\x23\x45\x67\x89", *result));

	result = NULL;
	get_key(20, &result);
	ASSERT_TRUE(result != NULL);
	char data1[15]; memset(data1, 0x11, 15);
	EXPECT_TRUE(CompareKeys(20, 15, "MD5", data1, *result));

	result = NULL;
	get_key(30, &result);
	ASSERT_TRUE(result != NULL);
	char data2[13]; memset(data2, 0x01, 13);
	EXPECT_TRUE(CompareKeys(30, 13, "MD5", data2, *result));
}

TEST_F(keyFileTest, ReadKeyFileWithComments) {
	key* keys = NULL;

	ASSERT_EQ(2, auth_init(CreatePath("key-test-comments", INPUT_DIR).c_str(), &keys));
	
	ASSERT_TRUE(keys != NULL);

	key* result = NULL;
	get_key(10, &result);
	ASSERT_TRUE(result != NULL);
	char data[15]; memset(data, 0x01, 15);
	EXPECT_TRUE(CompareKeys(10, 15, "MD5", data, *result));

	result = NULL;
	get_key(34, &result);
	ASSERT_TRUE(result != NULL);
	EXPECT_TRUE(CompareKeys(34, 3, "MD5", "xyz", *result));
}

TEST_F(keyFileTest, ReadKeyFileWithInvalidHex) {
	key* keys = NULL;

	ASSERT_EQ(1, auth_init(CreatePath("key-test-invalid-hex", INPUT_DIR).c_str(), &keys));

	ASSERT_TRUE(keys != NULL);

	key* result = NULL;
	get_key(10, &result);
	ASSERT_TRUE(result != NULL);
	char data[15]; memset(data, 0x01, 15);
	EXPECT_TRUE(CompareKeys(10, 15, "MD5", data, *result));

	result = NULL;
	get_key(30, &result); // Should not exist, and result should remain NULL.
	ASSERT_TRUE(result == NULL);
}
