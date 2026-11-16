/*-
 * Copyright (c) 2026 Max Freedom Pollard
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

/*
 * The return value of the client close callback must be reported
 * by archive_write_close(), as documented in archive_write_open(3).
 */

struct close_data {
	int close_return;
	int close_called;
};

static ssize_t
write_cb(struct archive *a, void *client_data, const void *buff,
    size_t length)
{
	(void)a; /* UNUSED */
	(void)client_data; /* UNUSED */
	(void)buff; /* UNUSED */
	return ((ssize_t)length);
}

static int
close_cb(struct archive *a, void *client_data)
{
	struct close_data *data = (struct close_data *)client_data;

	++data->close_called;
	if (data->close_return != ARCHIVE_OK)
		archive_set_error(a, EIO, "Close callback failed");
	return (data->close_return);
}

static struct archive *
open_archive(struct close_data *data, int close_return)
{
	struct archive *a;

	memset(data, 0, sizeof(*data));
	data->close_return = close_return;
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open(a, data, NULL, write_cb, close_cb));
	return (a);
}

DEFINE_TEST(test_archive_write_close_callback)
{
	struct archive *a;
	struct close_data data;

	/* A successful close callback. */
	a = open_archive(&data, ARCHIVE_OK);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(1, data.close_called);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	assertEqualInt(1, data.close_called);

	/* A failing close callback is reported by archive_write_close(). */
	a = open_archive(&data, ARCHIVE_FATAL);
	assertEqualIntA(a, ARCHIVE_FATAL, archive_write_close(a));
	assertEqualInt(EIO, archive_errno(a));
	assertEqualString("Close callback failed", archive_error_string(a));
	assertEqualInt(1, data.close_called);
	/* The close callback is not invoked again. */
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	assertEqualInt(1, data.close_called);

	/* Also when archive_write_free() closes the archive. */
	a = open_archive(&data, ARCHIVE_FATAL);
	assertEqualInt(ARCHIVE_FATAL, archive_write_free(a));
	assertEqualInt(1, data.close_called);

	/* A warning from the close callback is reported, too. */
	a = open_archive(&data, ARCHIVE_WARN);
	assertEqualIntA(a, ARCHIVE_WARN, archive_write_close(a));
	assertEqualInt(EIO, archive_errno(a));
	assertEqualInt(1, data.close_called);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	assertEqualInt(1, data.close_called);
}
