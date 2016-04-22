#include "config.h"

/* need autokey for some of the tests, or the will create buffer overruns. */
#ifndef AUTOKEY
# define AUTOKEY 1
#endif

#include "sntptest.h"
#include "networking.h"
#include "ntp_stdlib.h"
#include "unity.h"


const char * Version = "stub unit test Version string";

// Hacks into the key database.
extern struct key* key_ptr;
extern int key_cnt;


void PrepareAuthenticationTest(int key_id,int key_len,const char* type,const void* key_seq);
void PrepareAuthenticationTestMD5(int key_id,int key_len,const void* key_seq);
void setUp(void);
void tearDown(void);
void test_TooShortLength(void);
void test_LengthNotMultipleOfFour(void);
void test_TooShortExtensionFieldLength(void);
void test_UnauthenticatedPacketReject(void);
void test_CryptoNAKPacketReject(void);
void test_AuthenticatedPacketInvalid(void);
void test_AuthenticatedPacketUnknownKey(void);
void test_ServerVersionTooOld(void);
void test_ServerVersionTooNew(void);
void test_NonWantedMode(void);
void test_KoDRate(void);
void test_KoDDeny(void);
void test_RejectUnsyncedServer(void);
void test_RejectWrongResponseServerMode(void);
void test_AcceptNoSentPacketBroadcastMode(void);
void test_CorrectUnauthenticatedPacket(void);
void test_CorrectAuthenticatedPacketMD5(void);
void test_CorrectAuthenticatedPacketSHA1(void);


static struct pkt testpkt;
static struct pkt testspkt;
static sockaddr_u testsock;
bool restoreKeyDb;


void
PrepareAuthenticationTest(
	int		key_id,
	int		key_len,
	const char *	type,
	const void *	key_seq
	)
{
	char str[25];
	snprintf(str, 25, "%d", key_id);
	ActivateOption("-a", str);

	key_cnt = 1;
	key_ptr = emalloc(sizeof(struct key));
	key_ptr->next = NULL;
	key_ptr->key_id = key_id;
	key_ptr->key_len = key_len;
	memcpy(key_ptr->type, "MD5", 3);

	TEST_ASSERT_TRUE(key_len < sizeof(key_ptr->key_seq));

	memcpy(key_ptr->key_seq, key_seq, key_ptr->key_len);
	restoreKeyDb = true;
}


void
PrepareAuthenticationTestMD5(
	int 		key_id,
	int 		key_len,
	const void *	key_seq
	)
{
	PrepareAuthenticationTest(key_id, key_len, "MD5", key_seq);
}


void
setUp(void)
{

	sntptest();
	restoreKeyDb = false;

	/* Initialize the test packet and socket,
	 * so they contain at least some valid data.
	 */
	testpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING, NTP_VERSION,
										MODE_SERVER);
	testpkt.stratum = STRATUM_REFCLOCK;
	memcpy(&testpkt.refid, "GPS\0", 4);

	/* Set the origin timestamp of the received packet to the
	 * same value as the transmit timestamp of the sent packet.
	 */
	l_fp tmp;
	tmp.l_ui = 1000UL;
	tmp.l_uf = 0UL;

	HTONL_FP(&tmp, &testpkt.org);
	HTONL_FP(&tmp, &testspkt.xmt);
}


void
tearDown(void)
{	
	if (restoreKeyDb) {
		key_cnt = 0;
		free(key_ptr);
		key_ptr = NULL;
	}

	sntptest_destroy(); /* only on the final test!! if counter == 0 etc... */
}


void
test_TooShortLength(void)
{
	TEST_ASSERT_EQUAL(PACKET_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC - 1,
				      MODE_SERVER, &testspkt, "UnitTest"));
	TEST_ASSERT_EQUAL(PACKET_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC - 1,
				      MODE_BROADCAST, &testspkt, "UnitTest"));
}


void
test_LengthNotMultipleOfFour(void)
{
	TEST_ASSERT_EQUAL(PACKET_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC + 6,
				      MODE_SERVER, &testspkt, "UnitTest"));
	TEST_ASSERT_EQUAL(PACKET_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC + 3,
				      MODE_BROADCAST, &testspkt, "UnitTest"));
}


void
test_TooShortExtensionFieldLength(void)
{
	/* The lower 16-bits are the length of the extension field.
	 * This lengths must be multiples of 4 bytes, which gives
	 * a minimum of 4 byte extension field length.
	 */
	testpkt.exten[7] = htonl(3); /* 3 bytes is too short. */

	/* We send in a pkt_len of header size + 4 byte extension
	 * header + 24 byte MAC, this prevents the length error to
	 * be caught at an earlier stage
	 */
	int pkt_len = LEN_PKT_NOMAC + 4 + 24;

	TEST_ASSERT_EQUAL(PACKET_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_UnauthenticatedPacketReject(void)
{
	/* Activate authentication option */
	ActivateOption("-a", "123");
	TEST_ASSERT_TRUE(ENABLED_OPT(AUTHENTICATION));

	int pkt_len = LEN_PKT_NOMAC;

	/* We demand authentication, but no MAC header is present. */
	TEST_ASSERT_EQUAL(SERVER_AUTH_FAIL,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_CryptoNAKPacketReject(void)
{
	/* Activate authentication option */
	ActivateOption("-a", "123");
	TEST_ASSERT_TRUE(ENABLED_OPT(AUTHENTICATION));

	int pkt_len = LEN_PKT_NOMAC + 4; /* + 4 byte MAC = Crypto-NAK */

	TEST_ASSERT_EQUAL(SERVER_AUTH_FAIL,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_AuthenticatedPacketInvalid(void)
{
	/* Activate authentication option */
	PrepareAuthenticationTestMD5(50, 9, "123456789");
	TEST_ASSERT_TRUE(ENABLED_OPT(AUTHENTICATION));
	
	/* Prepare the packet. */
	int pkt_len = LEN_PKT_NOMAC;

	testpkt.exten[0] = htonl(50);
	int mac_len = make_mac(&testpkt, pkt_len,
			       MAX_MD5_LEN, key_ptr,
			       &testpkt.exten[1]);

	pkt_len += 4 + mac_len;

	/* Now, alter the MAC so it becomes invalid. */
	testpkt.exten[1] += 1;

	TEST_ASSERT_EQUAL(SERVER_AUTH_FAIL,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_AuthenticatedPacketUnknownKey(void)
{
	/* Activate authentication option */
	PrepareAuthenticationTestMD5(30, 9, "123456789");
	TEST_ASSERT_TRUE(ENABLED_OPT(AUTHENTICATION));
	
	/* Prepare the packet. Note that the Key-ID expected is 30, but
	 * the packet has a key id of 50.
	 */
	int pkt_len = LEN_PKT_NOMAC;

	testpkt.exten[0] = htonl(50);
	int mac_len = make_mac(&testpkt, pkt_len,
			       MAX_MD5_LEN, key_ptr,
			       &testpkt.exten[1]);
	pkt_len += 4 + mac_len;

	TEST_ASSERT_EQUAL(SERVER_AUTH_FAIL,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_ServerVersionTooOld(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	testpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
					    NTP_OLDVERSION - 1,
					    MODE_CLIENT);
	TEST_ASSERT_TRUE(PKT_VERSION(testpkt.li_vn_mode) < NTP_OLDVERSION);

	int pkt_len = LEN_PKT_NOMAC;
	
	TEST_ASSERT_EQUAL(SERVER_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_ServerVersionTooNew(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	testpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
					    NTP_VERSION + 1,
					    MODE_CLIENT);
	TEST_ASSERT_TRUE(PKT_VERSION(testpkt.li_vn_mode) > NTP_VERSION);

	int pkt_len = LEN_PKT_NOMAC;

	TEST_ASSERT_EQUAL(SERVER_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_NonWantedMode(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	testpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
					    NTP_VERSION,
					    MODE_CLIENT);

	/* The packet has a mode of MODE_CLIENT, but process_pkt expects
	 * MODE_SERVER
	 */
	TEST_ASSERT_EQUAL(SERVER_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


/* Tests bug 1597 */
void
test_KoDRate(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	testpkt.stratum = STRATUM_PKT_UNSPEC;
	memcpy(&testpkt.refid, "RATE", 4);

	TEST_ASSERT_EQUAL(KOD_RATE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_KoDDeny(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	testpkt.stratum = STRATUM_PKT_UNSPEC;
	memcpy(&testpkt.refid, "DENY", 4);

	TEST_ASSERT_EQUAL(KOD_DEMOBILIZE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_RejectUnsyncedServer(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	testpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOTINSYNC,
					    NTP_VERSION,
					    MODE_SERVER);

	TEST_ASSERT_EQUAL(SERVER_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_RejectWrongResponseServerMode(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	l_fp tmp;
	tmp.l_ui = 1000UL;
	tmp.l_uf = 0UL;
	HTONL_FP(&tmp, &testpkt.org);

	tmp.l_ui = 2000UL;
	tmp.l_uf = 0UL;
	HTONL_FP(&tmp, &testspkt.xmt);

	TEST_ASSERT_EQUAL(PACKET_UNUSEABLE,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_AcceptNoSentPacketBroadcastMode(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	testpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
					    NTP_VERSION,
					    MODE_BROADCAST);

	TEST_ASSERT_EQUAL(LEN_PKT_NOMAC,
		  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC,
			      MODE_BROADCAST, NULL, "UnitTest"));
}


void
test_CorrectUnauthenticatedPacket(void)
{
	TEST_ASSERT_FALSE(ENABLED_OPT(AUTHENTICATION));

	TEST_ASSERT_EQUAL(LEN_PKT_NOMAC,
			  process_pkt(&testpkt, &testsock, LEN_PKT_NOMAC,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_CorrectAuthenticatedPacketMD5(void)
{
	PrepareAuthenticationTestMD5(10, 15, "123456789abcdef");
	TEST_ASSERT_TRUE(ENABLED_OPT(AUTHENTICATION));

	int pkt_len = LEN_PKT_NOMAC;

	/* Prepare the packet. */
	testpkt.exten[0] = htonl(10);
	int mac_len = make_mac(&testpkt, pkt_len,
			       MAX_MD5_LEN, key_ptr,
			       &testpkt.exten[1]);

	pkt_len += 4 + mac_len;

	TEST_ASSERT_EQUAL(pkt_len,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}


void
test_CorrectAuthenticatedPacketSHA1(void)
{
	PrepareAuthenticationTest(20, 15, "SHA1", "abcdefghijklmno");
	TEST_ASSERT_TRUE(ENABLED_OPT(AUTHENTICATION));

	int pkt_len = LEN_PKT_NOMAC;

	/* Prepare the packet. */
	testpkt.exten[0] = htonl(20);
	int mac_len = make_mac(&testpkt, pkt_len,
			       MAX_MAC_LEN, key_ptr,
			       &testpkt.exten[1]);

	pkt_len += 4 + mac_len;

	TEST_ASSERT_EQUAL(pkt_len,
			  process_pkt(&testpkt, &testsock, pkt_len,
				      MODE_SERVER, &testspkt, "UnitTest"));
}
