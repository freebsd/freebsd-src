/* This file contains test for both libntp/authkeys.c and libntp/authusekey.c */

#include "config.h"

#include "ntp.h"
#include "ntp_stdlib.h"
#include "ntp_calendar.h"

#include "unity.h"

#ifdef OPENSSL
# include "openssl/err.h"
# include "openssl/rand.h"
# include "openssl/evp.h"
#endif

u_long current_time = 4;
int counter = 0;

void setUp(void);
void tearDown(void);
void AddTrustedKey(keyid_t keyno);
void AddUntrustedKey(keyid_t keyno); 
void test_AddTrustedKeys(void);
void test_AddUntrustedKey(void);
void test_HaveKeyCorrect(void);
void test_HaveKeyIncorrect(void);
void test_AddWithAuthUseKey(void);
void test_EmptyKey(void);


void
setUp(void)
{ 
	if (counter == 0) {
		counter++;
		init_auth(); // causes segfault if called more than once
	}
	/*
	 * init_auth() is called by tests_main.cpp earlier.  It
	 * does not initialize global variables like
	 * authnumkeys, so let's reset them to zero here.
	 */
	authnumkeys = 0;

	/*
	 * Especially, empty the key cache!
	 */
	cache_keyid = 0;
	cache_type = 0;
	cache_flags = 0;
	cache_secret = NULL;
	cache_secretsize = 0;
}

void
tearDown(void)
{

}

static const int KEYTYPE = KEY_TYPE_MD5;

void
AddTrustedKey(keyid_t keyno) {
	/*
	 * We need to add a MD5-key in addition to setting the
	 * trust, because authhavekey() requires type != 0.
	 */
	MD5auth_setkey(keyno, KEYTYPE, NULL, 0);

	authtrust(keyno, TRUE);
}

void
AddUntrustedKey(keyid_t keyno) {
	authtrust(keyno, FALSE);
}

void
test_AddTrustedKeys(void) {
	const keyid_t KEYNO1 = 5;
	const keyid_t KEYNO2 = 8;

	AddTrustedKey(KEYNO1);
	AddTrustedKey(KEYNO2);

	TEST_ASSERT_TRUE(authistrusted(KEYNO1));
	TEST_ASSERT_TRUE(authistrusted(KEYNO2));
}

void
test_AddUntrustedKey(void) {
	const keyid_t KEYNO = 3;
   
	AddUntrustedKey(KEYNO);

	TEST_ASSERT_FALSE(authistrusted(KEYNO));
}

void
test_HaveKeyCorrect(void) {
	const keyid_t KEYNO = 3;

	AddTrustedKey(KEYNO);

	TEST_ASSERT_TRUE(auth_havekey(KEYNO));
	TEST_ASSERT_TRUE(authhavekey(KEYNO));
}

void
test_HaveKeyIncorrect(void) {
	const keyid_t KEYNO = 2;

	TEST_ASSERT_FALSE(auth_havekey(KEYNO));
	TEST_ASSERT_FALSE(authhavekey(KEYNO));
}

void
test_AddWithAuthUseKey(void) {
	const keyid_t KEYNO = 5;
	const char* KEY = "52a";

	TEST_ASSERT_TRUE(authusekey(KEYNO, KEYTYPE, (u_char*)KEY));	
}

void
test_EmptyKey(void) {
	const keyid_t KEYNO = 3;
	const char* KEY = "";


	TEST_ASSERT_FALSE(authusekey(KEYNO, KEYTYPE, (u_char*)KEY));
}
