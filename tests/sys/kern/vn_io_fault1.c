/*-
 * Copyright (c) 2026 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <atf-c.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

ATF_TC(big_unmapped_read);
ATF_TC_HEAD(big_unmapped_read, tc)
{
	atf_tc_set_md_var(tc, "descr", "Read a large amount of data "
	    "into a partially unmapped buffer");
}
ATF_TC_BODY(big_unmapped_read, tc)
{
	char *buf;
	size_t len = 3LU << 30;
	ssize_t ret;
	int fd;

	/* allocate a large buffer */
	if ((buf = malloc(len)) == NULL)
		atf_tc_skip("failed to allocate buffer");

	/* fault in just a tad more than the first 2 GB */
	for (size_t i = 0; i <= 2LU << 30; i += 4096)
		buf[i] = 0;

	/* create a large file */
	fd = open("big", O_RDWR | O_CREAT | O_TRUNC, 0600);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE_EQ_MSG(0, ftruncate(fd, len),
	    "failed to extend file: %m");

	/* read the file into the buffer */
	ret = read(fd, buf, len);
	ATF_REQUIRE_MSG(ret >= 0,
	    "failed to read from file: %m");
	ATF_REQUIRE_EQ_MSG(len, (size_t)ret,
	    "requested %zu bytes, got %zd", len, ret);

	/* done */
	ATF_CHECK_EQ(0, close(fd));
	free(buf);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, big_unmapped_read);
	return (atf_no_error());
}
