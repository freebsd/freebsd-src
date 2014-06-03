/*-
Copyright (C) 2013 Pietro Cerutti <gahr@FreeBSD.org>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

/*
 * Test basic FILE * functions (fread, fwrite, fseek, fclose) against
 * a FILE * retrieved using fmemopen()
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

void
test_preexisting()
{
	/* 
	 * Use a pre-existing buffer.
	 */

	char buf[512];
	char buf2[512];
	char str[]  = "Test writing some stuff";
	char str2[] = "AAAAAAAAA";
	char str3[] = "AAAA writing some stuff";
	FILE *fp;
	size_t nofw, nofr;
	int rc;

	/* Open a FILE * using fmemopen. */
	fp = fmemopen(buf, sizeof(buf), "w");
	assert(fp != NULL);

	/* Write to the buffer. */
	nofw = fwrite(str, 1, sizeof(str), fp);
	assert(nofw == sizeof(str));

	/* Close the FILE *. */
	rc = fclose(fp);
	assert(rc == 0);

	/* Re-open the FILE * to read back the data. */
	fp = fmemopen(buf, sizeof(buf), "r");
	assert(fp != NULL);

	/* Read from the buffer. */
	bzero(buf2, sizeof(buf2));
	nofr = fread(buf2, 1, sizeof(buf2), fp);
	assert(nofr == sizeof(buf2));

	/* 
	 * Since a write on a FILE * retrieved by fmemopen
	 * will add a '\0' (if there's space), we can check
	 * the strings for equality.
	 */
	assert(strcmp(str, buf2) == 0);

	/* Close the FILE *. */
	rc = fclose(fp);
	assert(rc == 0);

	/* Now open a FILE * on the first 4 bytes of the string. */
	fp = fmemopen(str, 4, "w");
	assert(fp != NULL);

	/*
	 * Try to write more bytes than we shoud, we'll get a short count (4).
	 */
	nofw = fwrite(str2, 1, sizeof(str2), fp);
	assert(nofw == 4);

	/* Close the FILE *. */
	rc = fclose(fp);

	/* Check that the string was not modified after the first 4 bytes. */
	assert(strcmp(str, str3) == 0);
}

void
test_autoalloc()
{
	/* 
	 * Let fmemopen allocate the buffer.
	 */

	char str[] = "A quick test";
	FILE *fp;
	long pos;
	size_t nofw, nofr, i;
	int rc;

	/* Open a FILE * using fmemopen. */
	fp = fmemopen(NULL, 512, "w+");
	assert(fp != NULL);

	/* fill the buffer */
	for (i = 0; i < 512; i++) {
		nofw = fwrite("a", 1, 1, fp);
		assert(nofw == 1);
	}

	/* Get the current position into the stream. */
	pos = ftell(fp);
	assert(pos == 512);

	/* 
	 * Try to write past the end, we should get a short object count (0)
	 */
	nofw = fwrite("a", 1, 1, fp);
	assert(nofw == 0);

	/* Close the FILE *. */
	rc = fclose(fp);
	assert(rc == 0);

	/* Open a FILE * using a wrong mode */
	fp = fmemopen(NULL, 512, "r");
	assert(fp == NULL);

	fp = fmemopen(NULL, 512, "w");
	assert(fp == NULL);
}

void
test_data_length()
{
	/*
	 * Here we test that a read operation doesn't go past the end of the
	 * data actually written, and that a SEEK_END seeks from the end of the
	 * data, not of the whole buffer.
	 */
	FILE *fp;
	char buf[512] = {'\0'};
	char str[]  = "Test data length. ";
	char str2[] = "Do we have two sentences?";
	char str3[sizeof(str) + sizeof(str2) -1];
	long pos;
	size_t nofw, nofr;
	int rc;

	/* Open a FILE * for updating our buffer. */
	fp = fmemopen(buf, sizeof(buf), "w+");
	assert(fp != NULL);

	/* Write our string into the buffer. */
	nofw = fwrite(str, 1, sizeof(str), fp);
	assert(nofw == sizeof(str));

	/* 
	 * Now seek to the end and check that ftell
	 * gives us sizeof(str).
	 */
	rc = fseek(fp, 0, SEEK_END);
	assert(rc == 0);
	pos = ftell(fp);
	assert(pos == sizeof(str));

	/* Close the FILE *. */
	rc = fclose(fp);
	assert(rc == 0);

	/* Reopen the buffer for appending. */
	fp = fmemopen(buf, sizeof(buf), "a+");
	assert(fp != NULL);

	/* We should now be writing after the first string. */
	nofw = fwrite(str2, 1, sizeof(str2), fp);
	assert(nofw == sizeof(str2));

	/* Rewind the FILE *. */
	rc = fseek(fp, 0, SEEK_SET);
	assert(rc == 0);

	/* Make sure we're at the beginning. */
	pos = ftell(fp);
	assert(pos == 0);

	/* Read the whole buffer. */
	nofr = fread(str3, 1, sizeof(buf), fp);
	assert(nofr == sizeof(str3));

	/* Make sure the two strings are there. */
	assert(strncmp(str3, str, sizeof(str) - 1) == 0);
	assert(strncmp(str3 + sizeof(str) - 1, str2, sizeof(str2)) == 0);

	/* Close the FILE *. */
	rc = fclose(fp);
	assert(rc == 0);
}

void
test_binary()
{
	/*
	 * Make sure that NULL bytes are never appended when opening a buffer
	 * in binary mode.
	 */

	FILE *fp;
	char buf[20];
	char str[] = "Test";
	size_t nofw;
	int rc, i;

	/* Pre-fill the buffer. */
	memset(buf, 'A', sizeof(buf));

	/* Open a FILE * in binary mode. */
	fp = fmemopen(buf, sizeof(buf), "w+b");
	assert(fp != NULL);

	/* Write some data into it. */
	nofw = fwrite(str, 1, strlen(str), fp);
	assert(nofw == strlen(str));

	/* Make sure that the buffer doesn't contain any NULL bytes. */
	for (i = 0; i < sizeof(buf); i++)
		assert(buf[i] != '\0');

	/* Close the FILE *. */
	rc = fclose(fp);
	assert(rc == 0);
}

void
test_append_binary_pos()
{
	/*
	 * For compatibility with other implementations (glibc), we set the
	 * position to 0 when opening an automatically allocated binary stream
	 * for appending.
	 */

	FILE *fp;

	fp = fmemopen(NULL, 16, "ab+");
	assert(ftell(fp) == 0L);
	fclose(fp);

	/*
	 * Make sure that a pre-allocated buffer behaves correctly.
	 */
	char buf[] = "Hello";
	fp = fmemopen(buf, sizeof(buf), "ab+");
	assert(ftell(fp) == strlen(buf));
	fclose(fp);
}

void
test_size_0()
{
	/*
	 * POSIX mandates that we return EINVAL if size is 0.
	 */

	FILE *fp;

	fp = fmemopen(NULL, 0, "r+");
	assert(fp == NULL);
	assert(errno == EINVAL);
}

int
main(void)
{
	test_autoalloc();
	test_preexisting();
	test_data_length();
	test_binary();
	test_append_binary_pos();
	test_size_0();
	return (0);
}
