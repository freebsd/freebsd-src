#include "g_libntptest.h"

extern "C" {
#include "vint64ops.h"
}

class vi64Test : public libntptest {
public:
	::testing::AssertionResult IsEqual(const vint64 &expected, const vint64 &actual) {
		if (0 == memcmp(&expected, &actual, sizeof(vint64))) {
			return ::testing::AssertionSuccess();
		} else {
			return ::testing::AssertionFailure()
			    << "expected: "
			    << std::hex << expected.D_s.hi << '.'
			    << std::hex << expected.D_s.lo
			    << " but was "
			    << std::hex << actual.D_s.hi << '.'
			    << std::hex << actual.D_s.lo;
		}
	}
};

// ----------------------------------------------------------------------
// test number parser
TEST_F(vi64Test, ParseVUI64_pos) {
	vint64 act, exp;
	const char *sp;
	char       *ep;

	sp         = "1234x";
	exp.D_s.hi = 0;
	exp.D_s.lo = 1234;
	act        = strtouv64(sp, &ep, 0);
	EXPECT_TRUE(IsEqual(exp, act));
	EXPECT_EQ(*ep, 'x');
}

TEST_F(vi64Test, ParseVUI64_neg) {
	vint64 act, exp;
	const char *sp;
	char       *ep;

	sp         = "-1234x";
	exp.D_s.hi = ~0;
	exp.D_s.lo = -1234;
	act        = strtouv64(sp, &ep, 0);
	EXPECT_TRUE(IsEqual(exp, act));
	EXPECT_EQ(*ep, 'x');
}

TEST_F(vi64Test, ParseVUI64_case) {
	vint64 act, exp;
	const char *sp;
	char       *ep;

	sp         = "0123456789AbCdEf";
	exp.D_s.hi = 0x01234567;
	exp.D_s.lo = 0x89ABCDEF;
	act        = strtouv64(sp, &ep, 16);
	EXPECT_TRUE(IsEqual(exp, act));
	EXPECT_EQ(*ep, '\0');
}

