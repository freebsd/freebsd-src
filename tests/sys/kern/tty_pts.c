/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <termios.h>

#include <atf-c.h>
#include <libutil.h>

/* Just a little more concise. */
#define	newpty(masterp, slavep)	openpty((masterp), (slavep), NULL, NULL, NULL)

ATF_TC_WITHOUT_HEAD(fionread);
ATF_TC_BODY(fionread, tc)
{
	char rbuf[32];
	char buf[] = "Hello";
	int master, slave;
	int bytes;

	ATF_REQUIRE_EQ(0, newpty(&master, &slave));

	/* Should be empty to begin with. */
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(0, bytes);

	ATF_REQUIRE_EQ(sizeof(buf) - 1, write(slave, buf, sizeof(buf) - 1));
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(sizeof(buf) - 1, bytes);

	/* Drain what we have available, should result in 0 bytes again. */
	ATF_REQUIRE_EQ(sizeof(buf) - 1, read(master, rbuf, sizeof(rbuf)));
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(0, bytes);

	/*
	 * Write once more, then close the slave side with data still in the
	 * buffer.
	 */
	ATF_REQUIRE_EQ(sizeof(buf) - 1, write(slave, buf, sizeof(buf) - 1));
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(sizeof(buf) - 1, bytes);

	ATF_REQUIRE_EQ(0, close(slave));

	/*
	 * The tty's output queue is discarded upon close, so we shouldn't have
	 * anything else to read().
	 */
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(0, bytes);
	ATF_REQUIRE_EQ(0, read(master, rbuf, sizeof(rbuf)));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, fionread);
	return (atf_no_error());
}
