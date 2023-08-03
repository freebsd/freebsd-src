/*-
 * Copyright (c) 2023 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <stdio.h>

#include <atf-c.h>

#define BUFSIZE 16

static const char seq[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

struct stream {
	char buf[BUFSIZE];
	unsigned int len;
	unsigned int pos;
};

static int writefn(void *cookie, const char *buf, int len)
{
	struct stream *s = cookie;
	int written = 0;

	if (len <= 0)
		return 0;
	while (len > 0 && s->pos < s->len) {
		s->buf[s->pos++] = *buf++;
		written++;
		len--;
	}
	if (written > 0)
		return written;
	errno = EAGAIN;
	return -1;
}

ATF_TC_WITHOUT_HEAD(flushlbuf_partial);
ATF_TC_BODY(flushlbuf_partial, tc)
{
	static struct stream s;
	static char buf[BUFSIZE + 1];
	FILE *f;
	unsigned int i = 0;
	int ret = 0;

	/*
	 * Create the stream and its buffer, print just enough characters
	 * to the stream to fill the buffer without triggering a flush,
	 * then check the state.
	 */
	s.len = BUFSIZE / 2; // write will fail after this amount
	ATF_REQUIRE((f = fwopen(&s, writefn)) != NULL);
	ATF_REQUIRE(setvbuf(f, buf, _IOLBF, BUFSIZE) == 0);
	while (i < BUFSIZE)
		if ((ret = fprintf(f, "%c", seq[i++])) < 0)
			break;
	ATF_CHECK_EQ(BUFSIZE, i);
	ATF_CHECK_EQ(seq[i - 1], buf[BUFSIZE - 1]);
	ATF_CHECK_EQ(1, ret);
	ATF_CHECK_EQ(0, s.pos);

	/*
	 * At this point, the buffer is full but writefn() has not yet
	 * been called.  The next fprintf() call will trigger a preemptive
	 * fflush(), and writefn() will consume s.len characters before
	 * returning EAGAIN, causing fprintf() to fail without having
	 * written anything (which is why we don't increment i here).
	 */
	ret = fprintf(f, "%c", seq[i]);
	ATF_CHECK_ERRNO(EAGAIN, ret < 0);
	ATF_CHECK_EQ(s.len, s.pos);

	/*
	 * We have consumed s.len characters from the buffer, so continue
	 * printing until it is full again and check that no overflow has
	 * occurred yet.
	 */
	while (i < BUFSIZE + s.len)
		fprintf(f, "%c", seq[i++]);
	ATF_CHECK_EQ(BUFSIZE + s.len, i);
	ATF_CHECK_EQ(seq[i - 1], buf[BUFSIZE - 1]);
	ATF_CHECK_EQ(0, buf[BUFSIZE]);

	/*
	 * The straw that breaks the camel's back: libc fails to recognize
	 * that the buffer is full and continues to write beyond its end.
	 */
	fprintf(f, "%c", seq[i++]);
	ATF_CHECK_EQ(0, buf[BUFSIZE]);
}

ATF_TC_WITHOUT_HEAD(flushlbuf_full);
ATF_TC_BODY(flushlbuf_full, tc)
{
	static struct stream s;
	static char buf[BUFSIZE];
	FILE *f;
	unsigned int i = 0;
	int ret = 0;

	/*
	 * Create the stream and its buffer, print just enough characters
	 * to the stream to fill the buffer without triggering a flush,
	 * then check the state.
	 */
	s.len = 0; // any attempt to write will fail
	ATF_REQUIRE((f = fwopen(&s, writefn)) != NULL);
	ATF_REQUIRE(setvbuf(f, buf, _IOLBF, BUFSIZE) == 0);
	while (i < BUFSIZE)
		if ((ret = fprintf(f, "%c", seq[i++])) < 0)
			break;
	ATF_CHECK_EQ(BUFSIZE, i);
	ATF_CHECK_EQ(seq[i - 1], buf[BUFSIZE - 1]);
	ATF_CHECK_EQ(1, ret);
	ATF_CHECK_EQ(0, s.pos);

	/*
	 * At this point, the buffer is full but writefn() has not yet
	 * been called.  The next fprintf() call will trigger a preemptive
	 * fflush(), and writefn() will immediately return EAGAIN, causing
	 * fprintf() to fail without having written anything (which is why
	 * we don't increment i here).
	 */
	ret = fprintf(f, "%c", seq[i]);
	ATF_CHECK_ERRNO(EAGAIN, ret < 0);
	ATF_CHECK_EQ(s.len, s.pos);

	/*
	 * Now make our stream writeable.
	 */
	s.len = sizeof(s.buf);

	/*
	 * Flush the stream again.  The data we failed to write previously
	 * should still be in the buffer and will now be written to the
	 * stream.
	 */
	ATF_CHECK_EQ(0, fflush(f));
	ATF_CHECK_EQ(seq[0], s.buf[0]);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, flushlbuf_partial);
	ATF_TP_ADD_TC(tp, flushlbuf_full);

	return (atf_no_error());
}
