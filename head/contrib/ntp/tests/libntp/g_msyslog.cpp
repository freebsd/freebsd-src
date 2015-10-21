#include "g_libntptest.h"

extern "C" {
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef VSNPRINTF_PERCENT_M
// format_errmsg() is normally private to msyslog.c
void	format_errmsg	(char *, size_t, const char *, int);
#endif
};

class msyslogTest : public libntptest {
};

// msnprintf()
TEST_F(msyslogTest, msnprintf)
{
#define FMT_PREFIX "msyslog.cpp ENOENT: "
	char	exp_buf[512];
	char	act_buf[512];
	int	exp_cnt;
	int	act_cnt;

	exp_cnt = snprintf(exp_buf, sizeof(exp_buf), FMT_PREFIX "%s",
			   strerror(ENOENT));
	errno = ENOENT;
	act_cnt = msnprintf(act_buf, sizeof(act_buf), FMT_PREFIX "%m");
	EXPECT_EQ(exp_cnt, act_cnt);
	EXPECT_STREQ(exp_buf, act_buf);
}

TEST_F(msyslogTest, msnprintfLiteralPercentm)
{
	char	exp_buf[32];
	char	act_buf[32];
	int	exp_cnt;
	int	act_cnt;

	exp_cnt = snprintf(exp_buf, sizeof(exp_buf), "%%m");
	errno = ENOENT;
	act_cnt = msnprintf(act_buf, sizeof(act_buf), "%%m");
	EXPECT_EQ(exp_cnt, act_cnt);
	EXPECT_STREQ(exp_buf, act_buf);
}

TEST_F(msyslogTest, msnprintfBackslashLiteralPercentm)
{
	char	exp_buf[32];
	char	act_buf[32];
	int	exp_cnt;
	int	act_cnt;

	exp_cnt = snprintf(exp_buf, sizeof(exp_buf), "\%%m");
	errno = ENOENT;
	act_cnt = msnprintf(act_buf, sizeof(act_buf), "\%%m");
	EXPECT_EQ(exp_cnt, act_cnt);
	EXPECT_STREQ(exp_buf, act_buf);
}

TEST_F(msyslogTest, msnprintfBackslashPercent)
{
	char	exp_buf[32];
	char	act_buf[32];
	int	exp_cnt;
	int	act_cnt;

	exp_cnt = snprintf(exp_buf, sizeof(exp_buf), "\%s",
			   strerror(ENOENT));
	errno = ENOENT;
	act_cnt = msnprintf(act_buf, sizeof(act_buf), "\%m");
	EXPECT_EQ(exp_cnt, act_cnt);
	EXPECT_STREQ(exp_buf, act_buf);
}

TEST_F(msyslogTest, msnprintfHangingPercent)
{
	static char fmt[] = "percent then nul term then non-nul %\0oops!";
	char exp_buf[64];
	char act_buf[64];
	int	exp_cnt;
	int	act_cnt;

	ZERO(exp_buf);
	ZERO(act_buf);
	exp_cnt = snprintf(exp_buf, sizeof(exp_buf), fmt);
	act_cnt = msnprintf(act_buf, sizeof(act_buf), fmt);
	EXPECT_EQ(exp_cnt, act_cnt);
	EXPECT_STREQ(exp_buf, act_buf);
	EXPECT_STREQ("", act_buf + 1 + strlen(act_buf));
}

#ifndef VSNPRINTF_PERCENT_M
TEST_F(msyslogTest, format_errmsgHangingPercent)
{
	static char fmt[] = "percent then nul term then non-nul %\0oops!";
	char act_buf[64];

	ZERO(act_buf);
	format_errmsg(act_buf, sizeof(act_buf), fmt, ENOENT);
	EXPECT_STREQ(fmt, act_buf);
	EXPECT_STREQ("", act_buf + 1 + strlen(act_buf));
}
#endif

TEST_F(msyslogTest, msnprintfNullTarget)
{
	int	exp_cnt;
	int	act_cnt;

	exp_cnt = snprintf(NULL, 0, "%d", 123);
	errno = ENOENT;
	act_cnt = msnprintf(NULL, 0, "%d", 123);
	EXPECT_EQ(exp_cnt, act_cnt);
}

TEST_F(msyslogTest, msnprintfTruncate)
{
	char	undist[] = "undisturbed";
	char	exp_buf[512];
	char	act_buf[512];
	int	exp_cnt;
	int	act_cnt;

	memcpy(exp_buf + 3, undist, sizeof(undist));
	memcpy(act_buf + 3, undist, sizeof(undist));
	exp_cnt = snprintf(exp_buf, 3, "%s", strerror(ENOENT));
	errno = ENOENT;
	act_cnt = msnprintf(act_buf, 3, "%m");
	EXPECT_EQ('\0', exp_buf[2]);
	EXPECT_EQ('\0', act_buf[2]);
	EXPECT_TRUE(act_cnt > 0);
	EXPECT_EQ(exp_cnt, act_cnt);
	EXPECT_STREQ(exp_buf, act_buf);
	EXPECT_STREQ(exp_buf + 3, undist);
	EXPECT_STREQ(act_buf + 3, undist);
}
