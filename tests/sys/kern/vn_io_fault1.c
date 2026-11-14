/*-
 * Copyright (c) 2026 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/mman.h>

#include <atf-c.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

ATF_TC(big_unmapped_read);
ATF_TC_HEAD(big_unmapped_read, tc)
{
	atf_tc_set_md_var(tc, "descr", "Read a large amount of data "
	    "into a partially unmapped buffer");
	atf_tc_set_md_var(tc, "require.memory", "4G");
}
ATF_TC_BODY(big_unmapped_read, tc)
{
	char *buf;
	size_t len = 3LU << 30;
	ssize_t ret;
	int fd;

	/* allocate a large buffer */
	buf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	if (buf == MAP_FAILED)
		atf_tc_skip("failed to allocate buffer");

	/* create a large file */
	fd = open("big", O_RDWR | O_CREAT | O_TRUNC, 0600);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE_EQ_MSG(0, ftruncate(fd, len),
	    "failed to extend file: %m");

	/* read part of the file into the buffer */
	ATF_REQUIRE_EQ(0, lseek(fd, 0, SEEK_SET));
	ret = read(fd, buf, INT_MAX);
	ATF_REQUIRE_MSG(ret >= 0,
	    "failed to read from file: %m");
	ATF_REQUIRE_EQ_MSG(INT_MAX, (size_t)ret,
	    "requested %zu bytes, got %zd", (size_t)INT_MAX, ret);

	/* read the entire file into the buffer */
	ATF_REQUIRE_EQ(0, lseek(fd, 0, SEEK_SET));
	ret = read(fd, buf, len);
	ATF_REQUIRE_MSG(ret >= 0,
	    "failed to read from file: %m");
	ATF_REQUIRE_EQ_MSG(len, (size_t)ret,
	    "requested %zu bytes, got %zd", len, ret);

	/* done */
	ATF_CHECK_EQ(0, close(fd));
	ATF_CHECK_EQ(0, munmap(buf, len));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, big_unmapped_read);
	return (atf_no_error());
}
