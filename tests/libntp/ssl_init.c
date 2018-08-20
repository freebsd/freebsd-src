#include "config.h"

#include "ntp.h"

#ifdef OPENSSL
# include "openssl/err.h"
# include "openssl/rand.h"
# include "openssl/evp.h"

#define CMAC "AES128CMAC"
#endif

#include "unity.h"


static const size_t TEST_MD5_DIGEST_LENGTH = 16;
static const size_t TEST_SHA1_DIGEST_LENGTH = 20;
static const size_t TEST_CMAC_DIGEST_LENGTH = 16;

void test_MD5KeyTypeWithoutDigestLength(void);
void test_MD5KeyTypeWithDigestLength(void);
void test_SHA1KeyTypeWithDigestLength(void);
void test_CMACKeyTypeWithDigestLength(void);
void test_MD5KeyName(void);
void test_SHA1KeyName(void);
void test_CMACKeyName(void);


// keytype_from_text()
void
test_MD5KeyTypeWithoutDigestLength(void) {
	TEST_ASSERT_EQUAL(KEY_TYPE_MD5, keytype_from_text("MD5", NULL));
}

void
test_MD5KeyTypeWithDigestLength(void) {
	size_t digestLength;
	size_t expected = TEST_MD5_DIGEST_LENGTH;

	TEST_ASSERT_EQUAL(KEY_TYPE_MD5, keytype_from_text("MD5", &digestLength));
	TEST_ASSERT_EQUAL(expected, digestLength);
}


void
test_SHA1KeyTypeWithDigestLength(void) {
#ifdef OPENSSL
	size_t digestLength;
	size_t expected = TEST_SHA1_DIGEST_LENGTH;

	TEST_ASSERT_EQUAL(NID_sha1, keytype_from_text("SHA1", &digestLength));
	TEST_ASSERT_EQUAL(expected, digestLength);
	/* OPENSSL */
#else 
	TEST_IGNORE_MESSAGE("Skipping because OPENSSL isn't defined");
#endif
}


void
test_CMACKeyTypeWithDigestLength(void) {
#if defined(OPENSSL) && defined(ENABLE_CMAC)
	size_t digestLength;
	size_t expected = TEST_CMAC_DIGEST_LENGTH;

	TEST_ASSERT_EQUAL(NID_cmac, keytype_from_text(CMAC, &digestLength));
	TEST_ASSERT_EQUAL(expected, digestLength);
	/* OPENSSL */
#else 
	TEST_IGNORE_MESSAGE("Skipping because OPENSSL/CMAC isn't defined");
#endif
}


// keytype_name()
void
test_MD5KeyName(void) {
	TEST_ASSERT_EQUAL_STRING("MD5", keytype_name(KEY_TYPE_MD5));
}


void
test_SHA1KeyName(void) {
#ifdef OPENSSL
	TEST_ASSERT_EQUAL_STRING("SHA1", keytype_name(NID_sha1));
#else
	TEST_IGNORE_MESSAGE("Skipping because OPENSSL isn't defined");
#endif	/* OPENSSL */
}


void
test_CMACKeyName(void) {
#if defined(OPENSSL)  && defined(ENABLE_CMAC)
	TEST_ASSERT_EQUAL_STRING(CMAC, keytype_name(NID_cmac));
#else
	TEST_IGNORE_MESSAGE("Skipping because OPENSSL/CMAC isn't defined");
#endif	/* OPENSSL */
}

