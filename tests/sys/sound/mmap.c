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

	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Verify that a MAP_SHARED mapping of a DSP device's software buffer remains
 * valid after the file descriptor is closed.
 */
ATF_TC(mmap_buffer_lifetime);
ATF_TC_HEAD(mmap_buffer_lifetime, tc)
{
	atf_tc_set_md_var(tc, "descr", "mmap data survives close()");
	atf_tc_set_md_var(tc, "require.kmods", "snd_dummy");
}
ATF_TC_BODY(mmap_buffer_lifetime, tc)
{
	audio_buf_info abi;
	uint8_t *buf;
	size_t len;
	int fd, arg;

	fd = open("/dev/dsp.dummy", O_RDWR);
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

ATF_TC(mmap_reject_prot_exec);
ATF_TC_HEAD(mmap_reject_prot_exec, tc)
{
	atf_tc_set_md_var(tc, "descr", "mmap PROT_EXEC rejection test");
	atf_tc_set_md_var(tc, "require.kmods", "snd_dummy");
}

ATF_TC_BODY(mmap_reject_prot_exec, tc)
{
	uint8_t *buf;
	size_t len;
	int fd;

	fd = open("/dev/dsp.dummy", O_RDWR);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));

	len = 8;

	buf = mmap(NULL, len, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED,
	    fd, 0);
	ATF_REQUIRE_MSG(buf == MAP_FAILED, FMT_ERR("mmap"));

	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Verify that an attempt to resize a channel's buffer while it is mmapped
 * fails.
 */
ATF_TC(mmap_resize_fails);
ATF_TC_HEAD(mmap_resize_fails, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Resizing a mmapped channel buffer fails");
	atf_tc_set_md_var(tc, "require.kmods", "snd_dummy");
}
ATF_TC_BODY(mmap_resize_fails, tc)
{
	audio_buf_info abi;
	uint8_t *buf;
	size_t len;
	int fd, arg, ret;

	fd = open("/dev/dsp.dummy", O_RDWR);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));

	arg = (2 << 16) | 14; /* 2 * 16KB */
	ATF_REQUIRE_MSG(ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &arg) == 0,
	    FMT_ERR("SNDCTL_DSP_SETFRAGMENT"));
	ATF_REQUIRE_MSG(ioctl(fd, SNDCTL_DSP_GETOSPACE, &abi) == 0,
	    FMT_ERR("SNDCTL_DSP_GETOSPACE"));

	len = abi.fragstotal * abi.fragsize;
	ATF_REQUIRE_MSG(len >= PAGE_SIZE, "buffer too small: %zu", len);

	buf = mmap(NULL, len, PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE_MSG(buf != MAP_FAILED, FMT_ERR("mmap"));

	/* Try to resize the buffer; this should fail. */
	arg = (4 << 16) | 12; /* 4 * 4KB */
	ret = ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &arg);
	ATF_REQUIRE_MSG(ret != 0, "resize of mmapped buffer should fail");
	ATF_REQUIRE_MSG(errno == EINVAL,
	    "expected EINVAL, got %s", strerror(errno));

	ATF_REQUIRE(munmap(buf, len) == 0);

	/*
	 * CHN_F_MMAP is only cleared when the fd is closed, so resizing still
	 * fails after munmap.  This is probably a bug, but we can at least
	 * verify that the resize succeeds after the fd is closed and reopened.
	 */
	ATF_REQUIRE(close(fd) == 0);
	fd = open("/dev/dsp.dummy", O_RDWR);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));
	arg = (4 << 16) | 12; /* 4 * 4KB */
	ATF_REQUIRE_MSG(ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &arg) == 0,
	    FMT_ERR("SNDCTL_DSP_SETFRAGMENT after reopen"));

	ATF_REQUIRE(close(fd) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mmap_offset_overflow);
	ATF_TP_ADD_TC(tp, mmap_buffer_lifetime);
	ATF_TP_ADD_TC(tp, mmap_reject_prot_exec);
	ATF_TP_ADD_TC(tp, mmap_resize_fails);

	return (atf_no_error());
}
