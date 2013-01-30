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
test_preexisting ()
{
	/* 
	 * use a pre-existing buffer
	 */

	char buf[512];
	char buf2[512];
	char str[]  = "Test writing some stuff";
	char str2[] = "AAAAAAAAA";
	char str3[] = "AAAA writing some stuff";
	FILE *fp;
	size_t nofw, nofr;
	int rc;

	/* open a FILE * using fmemopen */
	fp = fmemopen (buf, sizeof buf, "w");
	assert (fp != NULL);

	/* write to the buffer */
	nofw = fwrite (str, 1, sizeof str, fp);
	assert (nofw == sizeof str);

	/* close the FILE * */
	rc = fclose (fp);
	assert (rc == 0);

	/* re-open the FILE * to read back the data */
	fp = fmemopen (buf, sizeof buf, "r");
	assert (fp != NULL);

	/* read from the buffer */
	bzero (buf2, sizeof buf2);
	nofr = fread (buf2, 1, sizeof buf2, fp);
	assert (nofr == sizeof buf2);

	/* since a write on a FILE * retrieved by fmemopen
	 * will add a '\0' (if there's space), we can check
	 * the strings for equality */
	assert (strcmp(str, buf2) == 0);

	/* close the FILE * */
	rc = fclose (fp);
	assert (rc == 0);

	/* now open a FILE * on the first 4 bytes of the string */
	fp = fmemopen (str, 4, "w");
	assert (fp != NULL);

	/* try to write more bytes than we shoud, we'll get a short count (4) */
	nofw = fwrite (str2, 1, sizeof str2, fp);
	assert (nofw == 4);

	/* close the FILE * */
	rc = fclose (fp);

	/* check that the string was not modified after the first 4 bytes */
	assert (strcmp (str, str3) == 0);
}

void
test_autoalloc ()
{
	/* 
	 * let fmemopen allocate the buffer
	 */

	char str[] = "A quick test";
	FILE *fp;
	long pos;
	size_t nofw, nofr, i;
	int rc;

	/* open a FILE * using fmemopen */
	fp = fmemopen (NULL, 512, "w");
	assert (fp != NULL);

	/* fill the buffer */
	for (i = 0; i < 512; i++) {
		nofw = fwrite ("a", 1, 1, fp);
		assert (nofw == 1);
	}

	/* get the current position into the stream */
	pos = ftell (fp);
	assert (pos == 512);

	/* try to write past the end, we should get a short object count (0) */
	nofw = fwrite ("a", 1, 1, fp);
	assert (nofw == 0);

	/* close the FILE * */
	rc = fclose (fp);
	assert (rc == 0);
}

int
main (void)
{
	test_autoalloc   ();
	test_preexisting ();
	return (0);
}
