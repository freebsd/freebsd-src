/*-
 * Copyright (C) 2021 Axcient, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

typedef bool(*ses_cb)(const char *devname, int fd);

// Run a test function on every available ses device
static void
for_each_ses_dev(ses_cb cb, int oflags)
{
	glob_t g;
	int r;
	unsigned i;

	g.gl_pathc = 0;
	g.gl_pathv = NULL;
	g.gl_offs = 0;

	r = glob("/dev/ses*", GLOB_NOCHECK | GLOB_NOSORT, NULL, &g);
	ATF_REQUIRE_EQ(r, 0);
	if (g.gl_matchc == 0)
		return;

	for(i = 0; i < g.gl_matchc; i++) {
		int fd;

		fd = open(g.gl_pathv[i], oflags);
		ATF_REQUIRE(fd >= 0);
		cb(g.gl_pathv[i], fd);
		close(fd);
	}

	globfree(&g);
}

static bool
has_ses()
{
	glob_t g;
	int r;

	r = glob("/dev/ses*", GLOB_NOCHECK | GLOB_NOSORT, NULL, &g);
	ATF_REQUIRE_EQ(r, 0);

	return (g.gl_matchc != 0);
}
