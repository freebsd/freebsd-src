#include "libntptest.h"

extern "C" {
#ifdef OPENSSL
# include "openssl/err.h"
# include "openssl/rand.h"
# include "openssl/evp.h"
#endif
#include "ntp.h"
};

class ssl_initTest : public libntptest {
protected:
	static const size_t TEST_MD5_DIGEST_LENGTH = 16;
	static const size_t TEST_SHA1_DIGEST_LENGTH = 20;
};

// keytype_from_text()
TEST_F(ssl_initTest, MD5KeyTypeWithoutDigestLength) {
	ASSERT_EQ(KEY_TYPE_MD5, keytype_from_text("MD5", NULL));
}

TEST_F(ssl_initTest, MD5KeyTypeWithDigestLength) {
	size_t digestLength;
	size_t expected = TEST_MD5_DIGEST_LENGTH;

	EXPECT_EQ(KEY_TYPE_MD5, keytype_from_text("MD5", &digestLength));
	EXPECT_EQ(expected, digestLength);
}

#ifdef OPENSSL
TEST_F(ssl_initTest, SHA1KeyTypeWithDigestLength) {
	size_t digestLength;
	size_t expected = TEST_SHA1_DIGEST_LENGTH;

	EXPECT_EQ(NID_sha, keytype_from_text("SHA", &digestLength));
	EXPECT_EQ(expected, digestLength);
}
#endif	/* OPENSSL */

// keytype_name()
TEST_F(ssl_initTest, MD5KeyName) {
	EXPECT_STREQ("MD5", keytype_name(KEY_TYPE_MD5));
}

#ifdef OPENSSL
TEST_F(ssl_initTest, SHA1KeyName) {
	EXPECT_STREQ("SHA", keytype_name(NID_sha));
}
#endif	/* OPENSSL */
