#include "sntptest.h"
#include "fileHandlingTest.h"

extern "C" {
#include "main.h"
#include "utilities.h"
const char * Version = "stub unit test Version string";
};

using std::string;

class utilitiesTest : public sntptest {
protected:
	sockaddr_u CreateSockaddr4(const char* address) {
		sockaddr_u s;
		s.sa4.sin_family = AF_INET;
		s.sa4.sin_addr.s_addr = inet_addr(address);
		SET_PORT(&s, 123);

		return s;
	}

	addrinfo CreateAddrinfo(sockaddr_u* sock) {
		addrinfo a;
		a.ai_family = sock->sa.sa_family;
		a.ai_addrlen = SIZEOF_SOCKADDR(a.ai_family);
		a.ai_addr = &sock->sa;
		return a;
	}
};

class debugUtilitiesTest : public fileHandlingTest {
protected:
	bool outputFileOpened;
	FILE* outputFile;

	debugUtilitiesTest() : outputFileOpened(false) {}

	void InitDebugTest(const string& filename) {
		// Clear the contents of the current file.
		// Open the output file
		outputFile = fopen(filename.c_str(), "w+");
		ASSERT_TRUE(outputFile != NULL);
		outputFileOpened = true;
	}

	// Closes outputFile, and compare contents.
	void FinishDebugTest(const string& expected,
						 const string& actual) {
		if (outputFileOpened)
			fclose(outputFile);

		ifstream e(expected.c_str());
		ifstream a(actual.c_str());
		ASSERT_TRUE(e.good());
		ASSERT_TRUE(a.good());

		CompareFileContent(e, a);
	}
};

/* 
 * These tests are essentially a copy of the tests for socktoa()
 * in libntp. If sntp switches to using that functions, these
 * tests can be removed.
 */

TEST_F(utilitiesTest, IPv4Address) {
	const char* ADDR = "192.0.2.10";

	sockaddr_u input = CreateSockaddr4(ADDR);
	addrinfo inputA = CreateAddrinfo(&input);

	EXPECT_STREQ(ADDR, ss_to_str(&input));
	EXPECT_STREQ(ADDR, addrinfo_to_str(&inputA));
}

TEST_F(utilitiesTest, IPv6Address) {
	const struct in6_addr address = {
						0x20, 0x01, 0x0d, 0xb8,
						0x85, 0xa3, 0x08, 0xd3, 
						0x13, 0x19, 0x8a, 0x2e,
						0x03, 0x70, 0x73, 0x34
					};
	const char * expected = "2001:db8:85a3:8d3:1319:8a2e:370:7334";
	sockaddr_u	input;
	addrinfo	inputA;

	memset(&input, 0, sizeof(input));
	input.sa6.sin6_family = AF_INET6;
	input.sa6.sin6_addr = address;
	EXPECT_STREQ(expected, ss_to_str(&input));

	inputA = CreateAddrinfo(&input);
	EXPECT_STREQ(expected, addrinfo_to_str(&inputA));
}

TEST_F(utilitiesTest, SetLiVnMode1) {
	pkt expected;
	expected.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
					     NTP_VERSION,
					     MODE_SERVER);

	pkt actual;
	set_li_vn_mode(&actual, LEAP_NOWARNING, NTP_VERSION,
				   MODE_SERVER);

	EXPECT_EQ(expected.li_vn_mode, actual.li_vn_mode);
}

TEST_F(utilitiesTest, SetLiVnMode2) {
	pkt expected;
	expected.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOTINSYNC,
										 NTP_OLDVERSION,
										 MODE_BROADCAST);

	pkt actual;
	set_li_vn_mode(&actual, LEAP_NOTINSYNC, NTP_OLDVERSION,
				   MODE_BROADCAST);

	EXPECT_EQ(expected.li_vn_mode, actual.li_vn_mode);
}

/* Debug utilities tests */

TEST_F(debugUtilitiesTest, PktOutput) {
	string filename = CreatePath("debug-output-pkt", OUTPUT_DIR);
	InitDebugTest(filename);

	pkt testpkt;
	memset(&testpkt, 0, sizeof(pkt));
	testpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
										NTP_VERSION,
										MODE_SERVER);

	l_fp test;
	test.l_ui = 8;
	test.l_uf = 2147483647; // Lots of ones.
	HTONL_FP(&test, &testpkt.xmt);

	pkt_output(&testpkt, LEN_PKT_NOMAC, outputFile);

	FinishDebugTest(CreatePath("debug-input-pkt", INPUT_DIR), filename);
}

TEST_F(debugUtilitiesTest, LfpOutputBinaryFormat) {
	string filename = CreatePath("debug-output-lfp-bin", OUTPUT_DIR);
	InitDebugTest(filename);

	l_fp test;
	test.l_ui = 63;  // 00000000 00000000 00000000 00111111
	test.l_uf = 127; // 00000000 00000000 00000000 01111111

	l_fp network;
	HTONL_FP(&test, &network);

	l_fp_output_bin(&network, outputFile);

	FinishDebugTest(CreatePath("debug-input-lfp-bin", INPUT_DIR), filename);
}

TEST_F(debugUtilitiesTest, LfpOutputDecimalFormat) {
	string filename = CreatePath("debug-output-lfp-dec", OUTPUT_DIR);
	InitDebugTest(filename);

	l_fp test;
	test.l_ui = 6310; // 0x000018A6
	test.l_uf = 308502; // 0x00004B516

	l_fp network;
	HTONL_FP(&test, &network);

	l_fp_output_dec(&network, outputFile);

	FinishDebugTest(CreatePath("debug-input-lfp-dec", INPUT_DIR), filename);
}
