#include "config.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "unity.h"
#include "ntp.h"
#include "ntp_stdlib.h"

/*
 * tests/libntp/data/ntp.keys has two keys for each algorithm, 50 keyids apart.
 * The first is 20 random ASCII chars, the 2nd 40 random hex values.
 */
#define HEX_KEYID_OFFSET	50

/* in generated srcdir.c */
extern const char srcdir[];

/* needed by authtrust() */
u_long			current_time;

static bool		setup;
static u_int32 *	pkt;
static size_t		pkt_sz;
static u_char *		mac;

/* helper routine */
void dump_mac(keyid_t keyid, u_char *pmac, size_t octets);


/* unity calls setUp before each test routine */
void setUp(void);
void
setUp(void)
{
	static bool	done_once;
	const char	msg_rel_fname[] =	"data/mills,david-03.jpg";
	const char	keys_rel_fname[] =	"data/ntp.keys";
	char		msg_fname[PATH_MAX];
	char		keys_fname[PATH_MAX];
	int		msgf;
	int		result;
	struct stat	msg_stat;
	u_char *	msg;
	size_t		msg_sz;
	size_t		pad_sz;
	ssize_t		octets;

	if (done_once) {
		return;
	}
	done_once = TRUE;

	init_auth();

	snprintf(keys_fname, sizeof(keys_fname), "%s/%s", srcdir,
		 keys_rel_fname);
	if (! authreadkeys(keys_fname)) {
		fprintf(stderr, "could not load keys %s\n", keys_fname);
		return;
	}

	snprintf(msg_fname, sizeof(msg_fname), "%s/%s", srcdir, msg_rel_fname);
	msgf = open(msg_fname, O_RDONLY);
	if (msgf < 0) {
		fprintf(stderr, "could not open msg file %s\n", msg_fname);
		return;
	}

	result = fstat(msgf, &msg_stat);
	if (result < 0) {
		fprintf(stderr, "could not get msg file %s size\n", msg_fname);
		return;
	}

	msg_sz = msg_stat.st_size;
	/* round up to next multiple of 4 as needed by MD5authencrypt() */
	pad_sz = sizeof(u_int32) - (msg_sz % sizeof(u_int32));
	if (sizeof(u_int32) == pad_sz) {
		pad_sz = 0;
	}
	/* allocate room for the message, key ID, and MAC */
	msg = emalloc_zero(msg_sz + pad_sz + MAX_MAC_LEN);
	octets = read(msgf, msg, msg_sz);
	if (octets != msg_sz) {
		fprintf(stderr, "could not read msg from file %s, %u != %u\n",
			msg_fname, (u_int)octets, (u_int)msg_sz);
		return;
	}
	zero_mem(msg + msg_sz, pad_sz);
	pkt_sz = msg_sz + pad_sz;
	mac = (void *)((u_char *)msg + pkt_sz);
	pkt = (void *)msg;

	setup = TRUE;
}

/* reduce code duplication with an ugly macro */
#define TEST_ONE_DIGEST(key, exp_sz, exp_mac)				\
do {									\
	size_t res_sz;							\
									\
	zero_mem(mac, MAX_MAC_LEN);					\
	if (!auth_findkey(key)) {					\
		TEST_IGNORE_MESSAGE("MAC unsupported on this system");	\
		return;							\
	}								\
	authtrust((key), 1);						\
									\
	res_sz = authencrypt((key), pkt, pkt_sz);			\
	if (KEY_MAC_LEN == res_sz) {					\
		TEST_IGNORE_MESSAGE("Likely OpenSSL 3 failed digest "	\
				    "init.");				\
		return;							\
	}								\
	TEST_ASSERT_EQUAL_UINT((u_int)((exp_sz) + KEY_MAC_LEN), res_sz);\
	dump_mac((key), mac, res_sz);					\
	TEST_ASSERT_EQUAL_HEX8_ARRAY((exp_mac), mac, MAX_MAC_LEN);	\
} while (FALSE)


#define AES128CMAC_KEYID	1
#undef KEYID_A
#define KEYID_A			AES128CMAC_KEYID
#undef DG_SZ
#define DG_SZ			16
#undef KEYID_B
#define KEYID_B			(KEYID_A + HEX_KEYID_OFFSET)
void test_Digest_AES128CMAC(void);
void test_Digest_AES128CMAC(void)
{
#if defined(OPENSSL) && defined(ENABLE_CMAC)
	u_char expectedA[MAX_MAC_LEN] =
		{ 
			0, 0, 0, KEYID_A,
			0x34, 0x5b, 0xcf, 0xa8,
			0x85, 0x6e, 0x9d, 0x01,
			0xeb, 0x81, 0x25, 0xc2,
			0xa4, 0xb8, 0x1b, 0xe0
		};
	u_char expectedB[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_B,
			0xd1, 0x04, 0x4e, 0xbf,
			0x79, 0x2d, 0x3a, 0x40,
			0xcd, 0xdc, 0x5a, 0x44,
			0xde, 0xe0, 0x0c, 0x84
		};

	TEST_ASSERT(setup);
	TEST_ONE_DIGEST(KEYID_A, DG_SZ, expectedA);
	TEST_ONE_DIGEST(KEYID_B, DG_SZ, expectedB);
#else	/* ! (OPENSSL && ENABLE_CMAC) follows  */
	TEST_IGNORE_MESSAGE("Skipping, no OPENSSL or not ENABLE_CMAC");
#endif
}


#define MD4_KEYID		2
#undef KEYID_A
#define KEYID_A			MD4_KEYID
#undef DG_SZ
#define DG_SZ			16
#undef KEYID_B
#define KEYID_B			(KEYID_A + HEX_KEYID_OFFSET)
void test_Digest_MD4(void);
void test_Digest_MD4(void)
{
#ifdef OPENSSL
	u_char expectedA[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_A,
			0xf3, 0x39, 0x34, 0xca,
			0xe0, 0x48, 0x26, 0x0f,
			0x13, 0xca, 0x56, 0x9e,
			0xbc, 0x53, 0x9c, 0x66
		};
	u_char expectedB[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_B,
			0x5e, 0xe6, 0x81, 0xf2,
			0x57, 0x57, 0x8a, 0x2b,
			0xa8, 0x76, 0x8e, 0x7a,
			0xc4, 0xf4, 0x34, 0x7e
		};

	TEST_ASSERT(setup);
	TEST_ONE_DIGEST(KEYID_A, DG_SZ, expectedA);
	TEST_ONE_DIGEST(KEYID_B, DG_SZ, expectedB);
#else	/* ! OPENSSL follows  */
	TEST_IGNORE_MESSAGE("Skipping, no OPENSSL");
#endif
}


#define MD5_KEYID		3
#undef KEYID_A
#define KEYID_A			MD5_KEYID
#undef DG_SZ
#define DG_SZ			16
#undef KEYID_B
#define KEYID_B			(KEYID_A + HEX_KEYID_OFFSET)
void test_Digest_MD5(void);
void test_Digest_MD5(void)
{
	u_char expectedA[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_A,
			0xa6, 0x8d, 0x3a, 0xfe,
			0x52, 0xe5, 0xf7, 0xe9,
			0x4c, 0x97, 0x72, 0x16,
			0x7c, 0x28, 0x18, 0xaf
		};
	u_char expectedB[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_B,
			0xd4, 0x11, 0x2c, 0xc6,
			0x66, 0x74, 0x46, 0x8b,
			0x12, 0xb1, 0x8c, 0x49,
			0xb0, 0x06, 0xda, 0x34
		};

	TEST_ASSERT(setup);
	TEST_ONE_DIGEST(KEYID_A, DG_SZ, expectedA);
	TEST_ONE_DIGEST(KEYID_B, DG_SZ, expectedB);
}


#define MDC2_KEYID		4
#undef KEYID_A
#define KEYID_A			MDC2_KEYID
#undef DG_SZ
#define DG_SZ			16
#undef KEYID_B
#define KEYID_B			(KEYID_A + HEX_KEYID_OFFSET)
void test_Digest_MDC2(void);
void test_Digest_MDC2(void)
{
#ifdef OPENSSL
	u_char expectedA[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_A,
			0xa0, 0xfc, 0x18, 0xb6,
			0xea, 0xba, 0xa5, 0x27,
			0xc9, 0x64, 0x0e, 0x41,
			0x95, 0x90, 0x5d, 0xf5
		};
	u_char expectedB[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_B,
			0xe3, 0x2c, 0x1e, 0x64,
			0x7f, 0x85, 0x81, 0xe7,
			0x3b, 0xc3, 0x93, 0x5e,
			0xcd, 0x0e, 0x89, 0xeb
		};

	TEST_ASSERT(setup);
	TEST_ONE_DIGEST(KEYID_A, DG_SZ, expectedA);
	TEST_ONE_DIGEST(KEYID_B, DG_SZ, expectedB);
#else	/* ! OPENSSL follows  */
	TEST_IGNORE_MESSAGE("Skipping, no OPENSSL");
#endif
}


#define RIPEMD160_KEYID		5
#undef KEYID_A
#define KEYID_A			RIPEMD160_KEYID
#undef DG_SZ
#define DG_SZ			20
#undef KEYID_B
#define KEYID_B			(KEYID_A + HEX_KEYID_OFFSET)
void test_Digest_RIPEMD160(void);
void test_Digest_RIPEMD160(void)
{
#ifdef OPENSSL
	u_char expectedA[MAX_MAC_LEN] =
		{ 
			0, 0, 0, KEYID_A,
			0x8c, 0x3e, 0x55, 0xbb,
			0xec, 0x7c, 0xf6, 0x30,
			0xef, 0xd1, 0x45, 0x8c,
			0xdd, 0x29, 0x32, 0x7e,
			0x04, 0x87, 0x6c, 0xd7
		};
	u_char expectedB[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_B,
			0x2d, 0x4a, 0x48, 0xdd,
			0x28, 0x02, 0xb4, 0x9d,
			0xe3, 0x6d, 0x1b, 0x90,
			0x2b, 0xc4, 0x3f, 0xe5,
			0x19, 0x60, 0x12, 0xbc
		};

	TEST_ASSERT(setup);
	TEST_ONE_DIGEST(KEYID_A, DG_SZ, expectedA);
	TEST_ONE_DIGEST(KEYID_B, DG_SZ, expectedB);
#else	/* ! OPENSSL follows  */
	TEST_IGNORE_MESSAGE("Skipping, no OPENSSL");
#endif
}


#define SHA1_KEYID		6
#undef KEYID_A
#define KEYID_A			SHA1_KEYID
#undef DG_SZ
#define DG_SZ			20
#undef KEYID_B
#define KEYID_B			(KEYID_A + HEX_KEYID_OFFSET)
void test_Digest_SHA1(void);
void test_Digest_SHA1(void)
{
#ifdef OPENSSL
	u_char expectedA[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_A,
			0xe2, 0xc6, 0x17, 0x71,
			0x03, 0xc1, 0x85, 0x56,
			0x35, 0xc7, 0x4e, 0x75,
			0x79, 0x82, 0x9d, 0xcb,
			0x2d, 0x06, 0x0e, 0xfa
		};
	u_char expectedB[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_B,
			0x01, 0x16, 0x37, 0xb4,
			0xf5, 0x2d, 0xe0, 0x97,
			0xaf, 0xd8, 0x58, 0xf7,
			0xad, 0xb3, 0x7e, 0x38,
			0x86, 0x85, 0x78, 0x44
		};

	TEST_ASSERT(setup);
	TEST_ONE_DIGEST(KEYID_A, DG_SZ, expectedA);
	TEST_ONE_DIGEST(KEYID_B, DG_SZ, expectedB);
#else	/* ! OPENSSL follows  */
	TEST_IGNORE_MESSAGE("Skipping, no OPENSSL");
#endif
}


#define SHAKE128_KEYID		7
#undef KEYID_A
#define KEYID_A			SHAKE128_KEYID
#undef DG_SZ
#define DG_SZ			16
#undef KEYID_B
#define KEYID_B			(KEYID_A + HEX_KEYID_OFFSET)
void test_Digest_SHAKE128(void);
void test_Digest_SHAKE128(void)
{
#ifdef OPENSSL
	u_char expectedA[MAX_MAC_LEN] =
		{
			0, 0, 0, KEYID_A,
			0x5c, 0x0c, 0x1a, 0x85,
			0xad, 0x03, 0xb2, 0x9a,
			0xe4, 0x75, 0x37, 0x93,
			0xaa, 0xa6, 0xcd, 0x76
		};
	u_char expectedB[MAX_MAC_LEN] =
		{ 
			0, 0, 0, KEYID_B,
			0x07, 0x04, 0x63, 0xcc,
			0x46, 0xaf, 0xca, 0x00,
			0x7d, 0xd1, 0x5a, 0x39,
			0xfd, 0x34, 0xca, 0x10
		};

	TEST_ASSERT(setup);
	TEST_ONE_DIGEST(KEYID_A, DG_SZ, expectedA);
	TEST_ONE_DIGEST(KEYID_B, DG_SZ, expectedB);
#else	/* ! OPENSSL follows  */
	TEST_IGNORE_MESSAGE("Skipping, no OPENSSL");
#endif
}


/*
 * Dump a MAC in a form easy to cut and paste into the expected declaration.
 */
void dump_mac(
	keyid_t		keyid,
	u_char *	pmac,
	size_t		octets
	)
{
	char	dump[128];
	size_t	dc = 0;
	size_t	idx;

	dc += snprintf(dump + dc, sizeof(dump) - dc, "digest with key %u { ", keyid);

	for (idx = 0; idx < octets; idx++) {
		if (10 == idx) {
			msyslog(LOG_DEBUG, "%s", dump);
			dc = 0;
		}
		if (dc < sizeof(dump)) {
			dc += snprintf(dump + dc, sizeof(dump) - dc,
				       "0x%02x, ", pmac[idx]);
		}
	}

	if (dc < sizeof(dump)) {
		dc += snprintf(dump + dc, sizeof(dump) - dc, "}");
	}

	msyslog(LOG_DEBUG, "%s", dump);
}

