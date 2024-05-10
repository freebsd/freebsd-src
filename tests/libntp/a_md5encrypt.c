#include "config.h"
#include "unity.h"

#ifdef OPENSSL
# include "openssl/err.h"
# include "openssl/rand.h"
# include "openssl/evp.h"
#endif
#include "ntp.h"
#include "ntp_stdlib.h"


/*
 * Example packet with SHA1 hash calculated manually:
 * echo -n abcdefghijklmnopqrstuvwx | openssl sha1 -
 */
#ifdef OPENSSL
const keyid_t keyId = 42;
const int keytype = NID_sha1;
const u_char key[] = "abcdefgh";
const size_t keyLength = sizeof(key) - 1;
const u_char payload[] = "ijklmnopqrstuvwx";
#define payloadLength (sizeof(payload) - 1)
#define keyIdLength  (sizeof(keyid_t))
#define digestLength SHA1_LENGTH
#define packetLength (payloadLength + keyIdLength + digestLength)
union {
	u_char		u8 [packetLength];
	uint32_t	u32[1];
} expectedPacket =
{
    {
	'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
	'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
	0x00, 0x00, 0x00, 0x00,
	0xd7, 0x17, 0xe2, 0x2e,
	0x16, 0x59, 0x30, 0x5f,
	0xad, 0x6e, 0xf0, 0x88,
	0x64, 0x92, 0x3d, 0xb6,
	0x4a, 0xba, 0x9c, 0x08
    }
};
union {
	u_char		u8 [packetLength];
	uint32_t	u32[1];
} invalidPacket =
{
    {
	'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
	'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
	0x00, 0x00, 0x00, 0x00,
	0xd7, 0x17, 0xe2, 0x2e,
	0x16, 0x59, 0x30, 0x5f,
	0xad, 0x6e, 0xf0, 0x88,
	0x64, 0x92, 0x3d, 0xb6,
	0x4a, 0xba, 0x9c, 0xff
    }
}; /* same as expectedPacket but with last octet modified */
#endif	/* OPENSSL */

u_long current_time = 4;

void test_Encrypt(void);
void test_DecryptValid(void);
void test_DecryptInvalid(void);
void test_IPv4AddressToRefId(void);
void test_IPv6AddressToRefId(void);


void
test_Encrypt(void)
{
#ifndef OPENSSL
	TEST_IGNORE_MESSAGE("non-SSL build");
#else
	u_int32 *packetPtr;
	size_t length;

	packetPtr = emalloc_zero(packetLength);
	memcpy(packetPtr, payload, payloadLength);

	length = MD5authencrypt(keytype, key, keyLength,
				packetPtr, payloadLength);

	TEST_ASSERT_EQUAL(MAX_SHA1_LEN, length);

	TEST_ASSERT_TRUE(MD5authdecrypt(keytype, key, keyLength, packetPtr,
				        payloadLength, MAX_SHA1_LEN, keyId));
	TEST_ASSERT_EQUAL_MEMORY(expectedPacket.u8, packetPtr, packetLength);

	free(packetPtr);
#endif	/* OPENSSL */
}

void
test_DecryptValid(void)
{
#ifndef OPENSSL
	TEST_IGNORE_MESSAGE("non-SSL build");
#else
	TEST_ASSERT_TRUE(MD5authdecrypt(keytype, key, keyLength,
					expectedPacket.u32, payloadLength,
					MAX_SHA1_LEN, keyId));
#endif	/* OPENSSL */
}

void
test_DecryptInvalid(void)
{
#ifndef OPENSSL
	TEST_IGNORE_MESSAGE("non-SSL build");
#else
	TEST_ASSERT_FALSE(MD5authdecrypt(keytype, key, keyLength,
					 invalidPacket.u32, payloadLength,
					 MAX_SHA1_LEN, keyId));
#endif	/* OPENSSL */
}

void
test_IPv4AddressToRefId(void)
{
	sockaddr_u	addr;
	u_int32		addr4n;

	AF(&addr) = AF_INET;
	SET_PORT(&addr, htons(80));
	addr4n = inet_addr("192.0.2.1");
	NSRCADR(&addr) = addr4n;

	TEST_ASSERT_EQUAL_UINT32(addr4n, addr2refid(&addr));
}

void
test_IPv6AddressToRefId(void) {
	const int expected = 0x75cffd52;
	const struct in6_addr address = { { {
		0x20, 0x01, 0x0d, 0xb8,
		0x85, 0xa3, 0x08, 0xd3,
		0x13, 0x19, 0x8a, 0x2e,
		0x03, 0x70, 0x73, 0x34
	} } };
	sockaddr_u addr;

	AF(&addr) = AF_INET6;
	SOCK_ADDR6(&addr) = address;

	TEST_ASSERT_EQUAL(expected, addr2refid(&addr));
}
