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
#include <limits.h>

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
void test_auth_log2(void);


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

	return;
}

void
tearDown(void)
{
	return;
}

static const int KEYTYPE = KEY_TYPE_MD5;

void
AddTrustedKey(keyid_t keyno)
{
	/*
	 * We need to add a MD5-key in addition to setting the
	 * trust, because authhavekey() requires type != 0.
	 */
	MD5auth_setkey(keyno, KEYTYPE, NULL, 0, NULL);

	authtrust(keyno, TRUE);

	return;
}

void
AddUntrustedKey(keyid_t keyno)
{
	authtrust(keyno, FALSE);

	return;
}

void
test_AddTrustedKeys(void)
{
	const keyid_t KEYNO1 = 5;
	const keyid_t KEYNO2 = 8;

	AddTrustedKey(KEYNO1);
	AddTrustedKey(KEYNO2);

	TEST_ASSERT_TRUE(authistrusted(KEYNO1));
	TEST_ASSERT_TRUE(authistrusted(KEYNO2));

	return;
}

void
test_AddUntrustedKey(void)
{
	const keyid_t KEYNO = 3;
   
	AddUntrustedKey(KEYNO);

	TEST_ASSERT_FALSE(authistrusted(KEYNO));

	return;
}

void
test_HaveKeyCorrect(void)
{
	const keyid_t KEYNO = 3;

	AddTrustedKey(KEYNO);

	TEST_ASSERT_TRUE(auth_havekey(KEYNO));
	TEST_ASSERT_TRUE(authhavekey(KEYNO));

	return;
}

void
test_HaveKeyIncorrect(void)
{
	const keyid_t KEYNO = 2;

	TEST_ASSERT_FALSE(auth_havekey(KEYNO));
	TEST_ASSERT_FALSE(authhavekey(KEYNO));

	return;
}

void
test_AddWithAuthUseKey(void)
{
	const keyid_t KEYNO = 5;
	const char* KEY = "52a";

	TEST_ASSERT_TRUE(authusekey(KEYNO, KEYTYPE, (const u_char*)KEY));

	return;
}

void
test_EmptyKey(void)
{
	const keyid_t KEYNO = 3;
	const char* KEY = "";


	TEST_ASSERT_FALSE(authusekey(KEYNO, KEYTYPE, (const u_char*)KEY));

	return;
}

/* test the implementation of 'auth_log2' -- use a local copy of the code */

static u_short
auth_log2(
	size_t x)
{
	int	s;
	int	r = 0;
	size_t  m = ~(size_t)0;

	for (s = sizeof(size_t) / 2 * CHAR_BIT; s != 0; s >>= 1) {
		m <<= s;
		if (x & m)
			r += s;
		else
			x <<= s;
	}
	return (u_short)r;
}

void
test_auth_log2(void)
{
	int	l2;
	size_t	tv;
	
	TEST_ASSERT_EQUAL_INT(0, auth_log2(0));
	TEST_ASSERT_EQUAL_INT(0, auth_log2(1));
	for (l2 = 1; l2 < sizeof(size_t)*CHAR_BIT; ++l2) {
		tv = (size_t)1 << l2;
		TEST_ASSERT_EQUAL_INT(l2, auth_log2(   tv   ));
		TEST_ASSERT_EQUAL_INT(l2, auth_log2( tv + 1 ));
		TEST_ASSERT_EQUAL_INT(l2, auth_log2(2*tv - 1));
	}
}
