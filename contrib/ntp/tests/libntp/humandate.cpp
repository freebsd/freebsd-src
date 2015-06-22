#include "libntptest.h"

#include <sstream>
#include <string>

class humandateTest : public libntptest {
};

TEST_F(humandateTest, RegularTime) {
	time_t sample = 1276601278;
	std::ostringstream expected;

	tm* time;
	time = localtime(&sample);
	ASSERT_TRUE(time != NULL);

	expected << std::setfill('0')
			 << std::setw(2) << time->tm_hour << ":"
			 << std::setw(2) << time->tm_min << ":"
			 << std::setw(2) << time->tm_sec;

	EXPECT_STREQ(expected.str().c_str(), humantime(sample));
}

TEST_F(humandateTest, CurrentTime) {
	time_t sample;
	std::ostringstream expected;

	time(&sample);

	tm* time;
	time = localtime(&sample);
	ASSERT_TRUE(time != NULL);

	expected << std::setfill('0')
			 << std::setw(2) << time->tm_hour << ":"
			 << std::setw(2) << time->tm_min << ":"
			 << std::setw(2) << time->tm_sec;

	EXPECT_STREQ(expected.str().c_str(), humantime(sample));
}
