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
	/*NOP*/
}


static const int KEYTYPE = KEY_TYPE_MD5;
static char      msgbuf[128];

void
AddTrustedKey(keyid_t keyno)
{
	/*
	 * We need to add a MD5-key in addition to setting the
	 * trust, because authhavekey() requires type != 0.
	 */
	MD5auth_setkey(keyno, KEYTYPE, NULL, 0, NULL);

	authtrust(keyno, TRUE);
}

void
AddUntrustedKey(keyid_t keyno)
{
	authtrust(keyno, FALSE);
}

void test_AddTrustedKeys(void);
void test_AddTrustedKeys(void)
{
	const keyid_t KEYNO1 = 5;
	const keyid_t KEYNO2 = 8;

	AddTrustedKey(KEYNO1);
	AddTrustedKey(KEYNO2);

	TEST_ASSERT_TRUE(authistrusted(KEYNO1));
	TEST_ASSERT_TRUE(authistrusted(KEYNO2));
}

void test_AddUntrustedKey(void);
void test_AddUntrustedKey(void)
{
	const keyid_t KEYNO = 3;

	AddUntrustedKey(KEYNO);

	TEST_ASSERT_FALSE(authistrusted(KEYNO));
}

void test_HaveKeyCorrect(void);
void test_HaveKeyCorrect(void)
{
	const keyid_t KEYNO = 3;

	AddTrustedKey(KEYNO);

	TEST_ASSERT_TRUE(auth_havekey(KEYNO));
	TEST_ASSERT_TRUE(authhavekey(KEYNO));
}

void test_HaveKeyIncorrect(void);
void test_HaveKeyIncorrect(void)
{
	const keyid_t KEYNO = 2;

	TEST_ASSERT_FALSE(auth_havekey(KEYNO));
	TEST_ASSERT_FALSE(authhavekey(KEYNO));
}

void test_AddWithAuthUseKey(void);
void test_AddWithAuthUseKey(void)
{
	const keyid_t KEYNO = 5;
	const char* KEY = "52a";

	TEST_ASSERT_TRUE(authusekey(KEYNO, KEYTYPE, (const u_char*)KEY));
}

void test_EmptyKey(void);
void test_EmptyKey(void)
{
	const keyid_t KEYNO = 3;
	const char* KEY = "";

	TEST_ASSERT_FALSE(authusekey(KEYNO, KEYTYPE, (const u_char*)KEY));
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

void test_auth_log2(void);
void test_auth_log2(void)
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

/* Converting a string to a host address. Here we use 'getaddrinfo()' in
 * an independent implementation to avoid cross-reactions with the
 * object under test. 'inet_pton' is too dangerous to handle it
 * properly, and ultimate performance is *not* the goal here.
 */
static int/*BOOL*/
getaddr(
	int af,
	const char *astr,
	sockaddr_u * addr)
{
	struct addrinfo  hint;
	struct addrinfo *ares;

	memset(&hint, 0, sizeof(hint));
	hint.ai_flags = AI_NUMERICHOST;
	hint.ai_family = af;
	if (getaddrinfo(astr, NULL, &hint, &ares))
		return FALSE;
	if (ares->ai_addrlen > sizeof(*addr))
		memcpy(addr, ares->ai_addr, sizeof(*addr));
	else
		memcpy(addr, ares->ai_addr, ares->ai_addrlen);
	freeaddrinfo(ares);
	return TRUE;
}

void test_AddrMatch_anull(void);
void test_AddrMatch_anull(void)
{
	/* Check the not-an-address logic with a prefix/check length of
	 * zero bits. Any compare with a NULL or AF_UNSPEC address
	 * returns inequality (aka FALSE).
	 */
	sockaddr_u   ip4, ip6, ipn;

	memset(&ipn, 0, sizeof(ipn));
	AF(&ipn) = AF_UNSPEC;

	TEST_ASSERT_TRUE(getaddr(AF_INET , "192.128.1.1", &ip4));
	TEST_ASSERT_TRUE(getaddr(AF_INET6, "::1"        , &ip6));

	TEST_ASSERT_FALSE(keyacc_amatch(NULL, NULL, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(NULL, &ipn, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(NULL, &ip4, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(NULL, &ip6, 0));

	TEST_ASSERT_FALSE(keyacc_amatch(&ipn, NULL, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(&ipn, &ipn, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(&ipn, &ip4, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(&ipn, &ip6, 0));

	TEST_ASSERT_FALSE(keyacc_amatch(&ip4, NULL, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(&ip4, &ipn, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(&ip6, NULL, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(&ip6, &ipn, 0));
}

void test_AddrMatch_self4(void);
void test_AddrMatch_self4(void)
{
	sockaddr_u   ip4;
	unsigned int bits;
	
	TEST_ASSERT_TRUE(getaddr(AF_INET, "192.128.1.1", &ip4));
	for (bits = 0; bits < 40; ++bits)
		TEST_ASSERT_TRUE(keyacc_amatch(&ip4, &ip4, bits));
}

void test_AddrMatch_self6(void);
void test_AddrMatch_self6(void)
{
	sockaddr_u   ip6;
	unsigned int bits;
	
	TEST_ASSERT_TRUE(getaddr(AF_INET6, "::1" , &ip6));
	for (bits = 0; bits < 136; ++bits)
		TEST_ASSERT_TRUE(keyacc_amatch(&ip6, &ip6, bits));
}

void test_AddrMatch_afmix(void);
void test_AddrMatch_afmix(void)
{
	sockaddr_u ip6, ip4;
	
	TEST_ASSERT_TRUE(getaddr(AF_INET , "192.128.1.1", &ip4));
	TEST_ASSERT_TRUE(getaddr(AF_INET6, "::1"        , &ip6));

	TEST_ASSERT_FALSE(keyacc_amatch(&ip4, &ip6, 0));
	TEST_ASSERT_FALSE(keyacc_amatch(&ip6, &ip4, 0));
}

void test_AddrMatch_ipv4(void);
void test_AddrMatch_ipv4(void)
{
	sockaddr_u   a1, a2;
	unsigned int bits;
	int          want;

	TEST_ASSERT_TRUE(getaddr(AF_INET, "192.128.2.1", &a1));
	TEST_ASSERT_TRUE(getaddr(AF_INET, "192.128.3.1", &a2));

	/* the first 23 bits are equal, so any prefix <= 23 should match */
	for (bits = 0; bits < 40; ++bits) {
		snprintf(msgbuf, sizeof(msgbuf),
			 "keyacc_amatch(*,*,%u) wrong", bits);
		want = (bits <= 23);
		TEST_ASSERT_EQUAL_MESSAGE(want, keyacc_amatch(&a1, &a2, bits), msgbuf);
	}

	TEST_ASSERT_TRUE(getaddr(AF_INET, "192.128.2.127", &a1));
	TEST_ASSERT_TRUE(getaddr(AF_INET, "192.128.2.128", &a2));

	/* the first 24 bits are equal, so any prefix <= 24 should match */
	for (bits = 0; bits < 40; ++bits) {
		snprintf(msgbuf, sizeof(msgbuf),
			 "keyacc_amatch(*,*,%u) wrong", bits);
		want = (bits <= 24);
		TEST_ASSERT_EQUAL_MESSAGE(want, keyacc_amatch(&a1, &a2, bits), msgbuf);
	}
}

void test_AddrMatch_ipv6(void);
void test_AddrMatch_ipv6(void)
{
	sockaddr_u   a1, a2;
	unsigned int bits;
	int          want;
	
	TEST_ASSERT_TRUE(getaddr(AF_INET6, "FEDC:BA98:7654:3210::2:FFFF", &a1));
	TEST_ASSERT_TRUE(getaddr(AF_INET6, "FEDC:BA98:7654:3210::3:FFFF", &a2));

	/* the first 111 bits are equal, so any prefix <= 111 should match */
	for (bits = 0; bits < 136; ++bits) {
		snprintf(msgbuf, sizeof(msgbuf),
			 "keyacc_amatch(*,*,%u) wrong", bits);
		want = (bits <= 111);
		TEST_ASSERT_EQUAL_MESSAGE(want, keyacc_amatch(&a1, &a2, bits), msgbuf);
	}

	TEST_ASSERT_TRUE(getaddr(AF_INET6, "FEDC:BA98:7654:3210::2:7FFF", &a1));
	TEST_ASSERT_TRUE(getaddr(AF_INET6, "FEDC:BA98:7654:3210::2:8000", &a2));

	/* the first 112 bits are equal, so any prefix <= 112 should match */
	for (bits = 0; bits < 136; ++bits) {
		snprintf(msgbuf, sizeof(msgbuf),
			 "keyacc_amatch(*,*,%u) wrong", bits);
		want = (bits <= 112);
		TEST_ASSERT_EQUAL_MESSAGE(want, keyacc_amatch(&a1, &a2, bits), msgbuf);
	}
}
