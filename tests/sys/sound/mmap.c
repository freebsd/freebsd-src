/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#include <sys/mman.h>
#include <sys/soundcard.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define	FMT_ERR(s)	s ": %s", strerror(errno)

ATF_TC(mmap_offset_overflow);
ATF_TC_HEAD(mmap_offset_overflow, tc)
{
	atf_tc_set_md_var(tc, "descr", "mmap offset overflow test");
	atf_tc_set_md_var(tc, "require.kmods", "snd_dummy");
}

ATF_TC_BODY(mmap_offset_overflow, tc)
{
	uint8_t *buf;
	off_t off;
	size_t len;
	int fd;

	fd = open("/dev/dsp.dummy", O_RDWR);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));

	/* off + len will overflow and wrap back to 0. */
	off = 0xfffffffffffff000;
	len = 0x1000;

	buf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, off);
	ATF_REQUIRE_MSG(buf == MAP_FAILED, FMT_ERR("mmap"));

	munmap(buf, len);

	close(fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mmap_offset_overflow);

	return (atf_no_error());
}
