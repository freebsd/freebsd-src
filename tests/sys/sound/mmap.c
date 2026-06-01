/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/soundcard.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define	FMT_ERR(s)	s ": %s", strerror(errno)

ATF_TC(mmap_offset_overflow);
ATF_TC_HEAD(mmap_offset_overflow, tc)
{
	atf_tc_set_md_var(tc, "descr", "mmap offset overflow test");
}

ATF_TC_BODY(mmap_offset_overflow, tc)
{
	uint8_t *buf;
	off_t off;
	size_t len;
	int fd;

	fd = open("/dev/dsp0", O_RDWR);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));

	/* off + len will overflow and wrap back to 0. */
	off = 0xfffffffffffff000;
	len = 0x1000;

	buf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, off);
	ATF_REQUIRE_MSG(buf == MAP_FAILED, FMT_ERR("mmap"));

	munmap(buf, len);

	close(fd);
}

/*
 * Verify that a MAP_SHARED mapping of a DSP device's software buffer remains
 * valid after the file descriptor is closed.
 */
ATF_TC(mmap_buffer_lifetime);
ATF_TC_HEAD(mmap_buffer_lifetime, tc)
{
	atf_tc_set_md_var(tc, "descr", "mmap data survives close()");
}
ATF_TC_BODY(mmap_buffer_lifetime, tc)
{
	audio_buf_info abi;
	uint8_t *buf;
	size_t len;
	int fd, arg;

	fd = open("/dev/dsp0", O_RDWR);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));

	arg = (2 << 16) | 14; /* 2*16KB */
	ATF_REQUIRE_MSG(ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &arg) == 0,
	    FMT_ERR("SNDCTL_DSP_SETFRAGMENT"));
	ATF_REQUIRE_MSG(ioctl(fd, SNDCTL_DSP_GETOSPACE, &abi) == 0,
	    FMT_ERR("SNDCTL_DSP_GETOSPACE"));

	len = abi.bytes;
	ATF_REQUIRE_MSG(len >= PAGE_SIZE, "buffer too small: %zu", len);

	buf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE_MSG(buf != MAP_FAILED, FMT_ERR("mmap"));

	for (size_t i = 0; i < len; i++) {
		ATF_REQUIRE_MSG(buf[i] == 0,
		    "mmap data corrupted at offset %zu: want 0 got 0x%02x",
		    i, buf[i]);
	}

	memset(buf, 0xa5, len);
	for (size_t i = 0; i < len; i++) {
		ATF_REQUIRE_MSG(buf[i] == 0xa5,
		    "mmap data corrupted at offset %zu: want 0xa5 got 0x%02x",
		    i, buf[i]);
	}

	ATF_REQUIRE(close(fd) == 0);

	/* Closing the device causes the buffer to be reset. */
	for (size_t i = 0; i < len; i++) {
		ATF_REQUIRE_MSG(buf[i] == 0 || buf[i] == 0xa5,
		    "mmap data corrupted at offset %zu: got 0x%02x", i, buf[i]);
	}
	memset(buf, 0xa5, len);

	ATF_REQUIRE(munmap(buf, len) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mmap_offset_overflow);
	ATF_TP_ADD_TC(tp, mmap_buffer_lifetime);

	return (atf_no_error());
}
