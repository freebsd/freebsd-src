#include "sntptest.h"

extern "C" {
#include "crypto.h"
};

class cryptoTest : public sntptest {
};

#define MD5_LENGTH 16
#define SHA1_LENGTH 20

TEST_F(cryptoTest, MakeMd5Mac) {

	const char* PKT_DATA = "abcdefgh0123";
	const int PKT_LEN = strlen(PKT_DATA);
	const char* EXPECTED_DIGEST =
		"\x52\x6c\xb8\x38\xaf\x06\x5a\xfb\x6c\x98\xbb\xc0\x9b\x0a\x7a\x1b";
	char actual[MD5_LENGTH];

	key md5;
	md5.next = NULL;
	md5.key_id = 10;
	md5.key_len = 6;
	memcpy(&md5.key_seq, "md5seq", md5.key_len);
	memcpy(&md5.type, "MD5", 4);

	EXPECT_EQ(MD5_LENGTH,
			  make_mac((char*)PKT_DATA, PKT_LEN, MD5_LENGTH, &md5, actual));

	EXPECT_TRUE(memcmp(EXPECTED_DIGEST, actual, MD5_LENGTH) == 0);
}

#ifdef OPENSSL
TEST_F(cryptoTest, MakeSHA1Mac) {
	const char* PKT_DATA = "abcdefgh0123";
	const int PKT_LEN = strlen(PKT_DATA);
	const char* EXPECTED_DIGEST =
		"\x17\xaa\x82\x97\xc7\x17\x13\x6a\x9b\xa9"
		"\x63\x85\xb4\xce\xbe\x94\xa0\x97\x16\x1d";
	char actual[SHA1_LENGTH];

	key sha1;
	sha1.next = NULL;
	sha1.key_id = 20;
	sha1.key_len = 7;
	memcpy(&sha1.key_seq, "sha1seq", sha1.key_len);
	memcpy(&sha1.type, "SHA1", 5);

	EXPECT_EQ(SHA1_LENGTH,
			  make_mac((char*)PKT_DATA, PKT_LEN, SHA1_LENGTH, &sha1, actual));

	EXPECT_TRUE(memcmp(EXPECTED_DIGEST, actual, SHA1_LENGTH) == 0);
}
#endif	/* OPENSSL */

TEST_F(cryptoTest, VerifyCorrectMD5) {
	const char* PKT_DATA =
		"sometestdata"		// Data
		"\0\0\0\0"			// Key-ID (unused)
		"\xc7\x58\x99\xdd\x99\x32\x0f\x71" // MAC
		"\x2b\x7b\xfe\x4f\xa2\x32\xcf\xac";
	const int PKT_LEN = 12;

	key md5;
	md5.next = NULL;
	md5.key_id = 0;
	md5.key_len = 6;
	memcpy(&md5.key_seq, "md5key", md5.key_len);
	memcpy(&md5.type, "MD5", 4);

	EXPECT_TRUE(auth_md5((char*)PKT_DATA, PKT_LEN, MD5_LENGTH, &md5));
}

#ifdef OPENSSL
TEST_F(cryptoTest, VerifySHA1) {
	const char* PKT_DATA =
		"sometestdata"		// Data
		"\0\0\0\0"			// Key-ID (unused)
		"\xad\x07\xde\x36\x39\xa6\x77\xfa\x5b\xce" // MAC
		"\x2d\x8a\x7d\x06\x96\xe6\x0c\xbc\xed\xe1";
	const int PKT_LEN = 12;

	key sha1;
	sha1.next = NULL;
	sha1.key_id = 0;
	sha1.key_len = 7;
	memcpy(&sha1.key_seq, "sha1key", sha1.key_len);
	memcpy(&sha1.type, "SHA1", 5);

	EXPECT_TRUE(auth_md5((char*)PKT_DATA, PKT_LEN, SHA1_LENGTH, &sha1));
}
#endif	/* OPENSSL */

TEST_F(cryptoTest, VerifyFailure) {
	/* We use a copy of the MD5 verification code, but modify
	 * the last bit to make sure verification fails. */
	const char* PKT_DATA =
		"sometestdata"		// Data
		"\0\0\0\0"			// Key-ID (unused)
		"\xc7\x58\x99\xdd\x99\x32\x0f\x71"	// MAC
		"\x2b\x7b\xfe\x4f\xa2\x32\xcf\x00"; // Last byte is wrong!
	const int PKT_LEN = 12;

	key md5;
	md5.next = NULL;
	md5.key_id = 0;
	md5.key_len = 6;
	memcpy(&md5.key_seq, "md5key", md5.key_len);
	memcpy(&md5.type, "MD5", 4);

	EXPECT_FALSE(auth_md5((char*)PKT_DATA, PKT_LEN, MD5_LENGTH, &md5));
}

TEST_F(cryptoTest, PacketSizeNotMultipleOfFourBytes) {
	const char* PKT_DATA = "123456";
	const int PKT_LEN = 6;
	char actual[MD5_LENGTH];

	key md5;
	md5.next = NULL;
	md5.key_id = 10;
	md5.key_len = 6;
	memcpy(&md5.key_seq, "md5seq", md5.key_len);
	memcpy(&md5.type, "MD5", 4);

	EXPECT_EQ(0, make_mac((char*)PKT_DATA, PKT_LEN, MD5_LENGTH, &md5, actual));
}

