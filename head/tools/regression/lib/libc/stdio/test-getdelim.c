/*-
 * Copyright (c) 2009 David Schultz <das@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define	_WITH_GETLINE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	CHUNK_MAX	10

/* The assertions depend on this string. */
char apothegm[] = "All work and no play\0 makes Jack a dull boy.\n";

/*
 * This is a neurotic reader function designed to give getdelim() a
 * hard time. It reads through the string `apothegm' and returns a
 * random number of bytes up to the requested length.
 */
static int
_reader(void *cookie, char *buf, int len)
{
	size_t *offp = cookie;
	size_t r;

	r = random() % CHUNK_MAX + 1;
	if (len > r)
		len = r;
	if (len > sizeof(apothegm) - *offp)
		len = sizeof(apothegm) - *offp;
	memcpy(buf, apothegm + *offp, len);
	*offp += len;
	return (len);
}

static FILE *
mkfilebuf(void)
{
	size_t *offp;

	offp = malloc(sizeof(*offp));	/* XXX leak */
	*offp = 0;
	return (fropen(offp, _reader));
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *line;
	size_t linecap;
	int i, n;

	srandom(0);

	printf("1..6\n");

	/*
	 * Test multiple times with different buffer sizes
	 * and different _reader() return values.
	 */
	errno = 0;
	for (i = 0; i < 8; i++) {
		fp = mkfilebuf();
		linecap = i;
		line = malloc(i);
		/* First line: the full apothegm */
		assert(getline(&line, &linecap, fp) == sizeof(apothegm) - 1);
		assert(memcmp(line, apothegm, sizeof(apothegm)) == 0);
		assert(linecap >= sizeof(apothegm));
		/* Second line: the NUL terminator following the newline */
		assert(getline(&line, &linecap, fp) == 1);
		assert(line[0] == '\0' && line[1] == '\0');
		/* Third line: EOF */
		line[0] = 'X';
		assert(getline(&line, &linecap, fp) == -1);
		assert(line[0] == '\0');
		free(line);
		line = NULL;
		assert(feof(fp));
		assert(!ferror(fp));
		fclose(fp);
	}
	assert(errno == 0);
	printf("ok 1 - getline basic\n");

	/* Make sure read errors are handled properly. */
	linecap = 0;
	errno = 0;
	assert(getline(&line, &linecap, stdout) == -1);
	assert(errno == EBADF);
	errno = 0;
	assert(getdelim(&line, &linecap, 'X', stdout) == -1);
	assert(errno == EBADF);
	assert(ferror(stdout));
	printf("ok 2 - stream error\n");

	/* Make sure NULL linep or linecapp pointers are handled. */
	fp = mkfilebuf();
	assert(getline(NULL, &linecap, fp) == -1);
	assert(errno == EINVAL);
	assert(getline(&line, NULL, fp) == -1);
	assert(errno == EINVAL);
	assert(ferror(fp));
	fclose(fp);
	printf("ok 3 - invalid params\n");

	/* Make sure getline() allocates memory as needed if fp is at EOF. */
	errno = 0;
	fp = mkfilebuf();
	while (!feof(fp))	/* advance to EOF; can't fseek this stream */
		getc(fp);
	free(line);
	line = NULL;
	linecap = 0;
	assert(getline(&line, &linecap, fp) == -1);
	assert(line[0] == '\0');
	assert(linecap > 0);
	assert(errno == 0);
	assert(feof(fp));
	assert(!ferror(fp));
	fclose(fp);
	printf("ok 4 - eof\n");

	/* Make sure a NUL delimiter works. */
	fp = mkfilebuf();
	n = strlen(apothegm);
	assert(getdelim(&line, &linecap, '\0', fp) == n + 1);
	assert(strcmp(line, apothegm) == 0);
	assert(line[n + 1] == '\0');
	assert(linecap > n + 1);
	n = strlen(apothegm + n + 1);
	assert(getdelim(&line, &linecap, '\0', fp) == n + 1);
	assert(line[n + 1] == '\0');
	assert(linecap > n + 1);
	assert(errno == 0);
	assert(!ferror(fp));
	fclose(fp);
	printf("ok 5 - nul\n");

	/* Make sure NULL *linep and zero *linecapp are handled. */
	fp = mkfilebuf();
	free(line);
	line = NULL;
	linecap = 42;
	assert(getline(&line, &linecap, fp) == sizeof(apothegm) - 1);
	assert(memcmp(line, apothegm, sizeof(apothegm)) == 0);
	fp = mkfilebuf();
	free(line);
	line = malloc(100);
	linecap = 0;
	assert(getline(&line, &linecap, fp) == sizeof(apothegm) - 1);
	assert(memcmp(line, apothegm, sizeof(apothegm)) == 0);
	free(line);
	assert(!ferror(fp));
	fclose(fp);
	printf("ok 6 - empty/NULL initial buffer\n");

	exit(0);
}
