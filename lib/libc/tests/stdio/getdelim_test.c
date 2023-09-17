/*-
 * Copyright (c) 2009 David Schultz <das@FreeBSD.org>
 * Copyright (c) 2021 Dell EMC
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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

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

ATF_TC_WITHOUT_HEAD(getline_basic);
ATF_TC_BODY(getline_basic, tc)
{
	FILE *fp;
	char *line;
	size_t linecap;
	int i;

	srandom(0);

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
		ATF_REQUIRE(getline(&line, &linecap, fp) == sizeof(apothegm) - 1);
		ATF_REQUIRE(memcmp(line, apothegm, sizeof(apothegm)) == 0);
		ATF_REQUIRE(linecap >= sizeof(apothegm));
		/* Second line: the NUL terminator following the newline */
		ATF_REQUIRE(getline(&line, &linecap, fp) == 1);
		ATF_REQUIRE(line[0] == '\0' && line[1] == '\0');
		/* Third line: EOF */
		line[0] = 'X';
		ATF_REQUIRE(getline(&line, &linecap, fp) == -1);
		ATF_REQUIRE(line[0] == '\0');
		free(line);
		line = NULL;
		ATF_REQUIRE(feof(fp));
		ATF_REQUIRE(!ferror(fp));
		fclose(fp);
	}
	ATF_REQUIRE(errno == 0);
}

ATF_TC_WITHOUT_HEAD(stream_error);
ATF_TC_BODY(stream_error, tc)
{
	char *line;
	size_t linecap;

	/* Make sure read errors are handled properly. */
	line = NULL;
	linecap = 0;
	errno = 0;
	ATF_REQUIRE(getline(&line, &linecap, stdout) == -1);
	ATF_REQUIRE(errno == EBADF);
	errno = 0;
	ATF_REQUIRE(getdelim(&line, &linecap, 'X', stdout) == -1);
	ATF_REQUIRE(errno == EBADF);
	ATF_REQUIRE(ferror(stdout));
}

ATF_TC_WITHOUT_HEAD(invalid_params);
ATF_TC_BODY(invalid_params, tc)
{
	FILE *fp;
	char *line;
	size_t linecap;

	/* Make sure NULL linep or linecapp pointers are handled. */
	fp = mkfilebuf();
	ATF_REQUIRE(getline(NULL, &linecap, fp) == -1);
	ATF_REQUIRE(errno == EINVAL);
	ATF_REQUIRE(getline(&line, NULL, fp) == -1);
	ATF_REQUIRE(errno == EINVAL);
	ATF_REQUIRE(ferror(fp));
	fclose(fp);
}

ATF_TC_WITHOUT_HEAD(eof);
ATF_TC_BODY(eof, tc)
{
	FILE *fp;
	char *line;
	size_t linecap;

	/* Make sure getline() allocates memory as needed if fp is at EOF. */
	errno = 0;
	fp = mkfilebuf();
	while (!feof(fp))	/* advance to EOF; can't fseek this stream */
		getc(fp);
	line = NULL;
	linecap = 0;
	printf("getline\n");
	ATF_REQUIRE(getline(&line, &linecap, fp) == -1);
	ATF_REQUIRE(line[0] == '\0');
	ATF_REQUIRE(linecap > 0);
	ATF_REQUIRE(errno == 0);
	printf("feof\n");
	ATF_REQUIRE(feof(fp));
	ATF_REQUIRE(!ferror(fp));
	fclose(fp);
}

ATF_TC_WITHOUT_HEAD(nul);
ATF_TC_BODY(nul, tc)
{
	FILE *fp;
	char *line;
	size_t linecap, n;

	errno = 0;
	line = NULL;
	linecap = 0;
	/* Make sure a NUL delimiter works. */
	fp = mkfilebuf();
	n = strlen(apothegm);
	printf("getdelim\n");
	ATF_REQUIRE(getdelim(&line, &linecap, '\0', fp) == n + 1);
	ATF_REQUIRE(strcmp(line, apothegm) == 0);
	ATF_REQUIRE(line[n + 1] == '\0');
	ATF_REQUIRE(linecap > n + 1);
	n = strlen(apothegm + n + 1);
	printf("getdelim 2\n");
	ATF_REQUIRE(getdelim(&line, &linecap, '\0', fp) == n + 1);
	ATF_REQUIRE(line[n + 1] == '\0');
	ATF_REQUIRE(linecap > n + 1);
	ATF_REQUIRE(errno == 0);
	ATF_REQUIRE(!ferror(fp));
	fclose(fp);
}

ATF_TC_WITHOUT_HEAD(empty_NULL_buffer);
ATF_TC_BODY(empty_NULL_buffer, tc)
{
	FILE *fp;
	char *line;
	size_t linecap;

	/* Make sure NULL *linep and zero *linecapp are handled. */
	fp = mkfilebuf();
	line = NULL;
	linecap = 42;
	ATF_REQUIRE(getline(&line, &linecap, fp) == sizeof(apothegm) - 1);
	ATF_REQUIRE(memcmp(line, apothegm, sizeof(apothegm)) == 0);
	fp = mkfilebuf();
	free(line);
	line = malloc(100);
	linecap = 0;
	ATF_REQUIRE(getline(&line, &linecap, fp) == sizeof(apothegm) - 1);
	ATF_REQUIRE(memcmp(line, apothegm, sizeof(apothegm)) == 0);
	free(line);
	ATF_REQUIRE(!ferror(fp));
	fclose(fp);
}

static void
_ipc_read(int fd, char wait_c)
{
	char c;
	ssize_t len;

	c = 0;
	while (c != wait_c) {
		len = read(fd, &c, 1);
		ATF_CHECK_MSG(len != 0,
		    "EOF on IPC pipe while waiting. Did other side fail?");
		ATF_CHECK_MSG(len == 1 || errno == EINTR,
		    "read %zu bytes errno %d\n", len, errno);
		if (len != 1 || errno != EINTR)
			break;
	}
}

static void
_ipc_write(int fd, char c)
{

	while ((write(fd, &c, 1) != 1))
		ATF_REQUIRE(errno == EINTR);
}

static void
ipc_wait(int ipcfd[2])
{

	_ipc_read(ipcfd[0], '+');
	/* Send ACK. */
	_ipc_write(ipcfd[1], '-');
}

static void
ipc_wakeup(int ipcfd[2])
{

	_ipc_write(ipcfd[1], '+');
	/* Wait for ACK. */
	_ipc_read(ipcfd[0], '-');
}

static void
_nonblock_eagain(int buf_mode)
{
	FILE *fp;
	const char delim = '!';
	const char *strs[] = {
	    "first line partial!",
	    "second line is sent in full!",
	    "third line is sent partially!",
	    "last line is sent in full!",
	};
	char *line;
	size_t linecap, strslen[nitems(strs)];
	ssize_t linelen;
	int fd_fifo, flags, i, ipcfd[2], pipedes[2], pipedes2[2], status;
	pid_t pid;

	line = NULL;
	linecap = 0;
	for (i = 0; i < nitems(strslen); i++)
		strslen[i] = strlen(strs[i]);
	ATF_REQUIRE(pipe2(pipedes, O_CLOEXEC) == 0);
	ATF_REQUIRE(pipe2(pipedes2, O_CLOEXEC) == 0);

	(void)unlink("fifo");
	ATF_REQUIRE(mkfifo("fifo", 0666) == 0);
	ATF_REQUIRE((pid = fork()) >= 0);
	if (pid == 0) {
		close(pipedes[0]);
		ipcfd[1] = pipedes[1];
		ipcfd[0] = pipedes2[0];
		close(pipedes2[1]);

		ATF_REQUIRE((fd_fifo = open("fifo", O_WRONLY)) != -1);

		/* Partial write. */
		ATF_REQUIRE(write(fd_fifo, strs[0], strslen[0] - 3) ==
		    strslen[0] - 3);
		ipc_wakeup(ipcfd);

		ipc_wait(ipcfd);
		/* Finish off the first line. */
		ATF_REQUIRE(write(fd_fifo,
		    &(strs[0][strslen[0] - 3]), 3) == 3);
		/* And include the second full line and a partial 3rd line. */
		ATF_REQUIRE(write(fd_fifo, strs[1], strslen[1]) == strslen[1]);
		ATF_REQUIRE(write(fd_fifo, strs[2], strslen[2] - 3) ==
		    strslen[2] - 3);
		ipc_wakeup(ipcfd);

		ipc_wait(ipcfd);
		/* Finish the partial write and partially send the last. */
		ATF_REQUIRE(write(fd_fifo,
		    &(strs[2][strslen[2] - 3]), 3) == 3);
		ATF_REQUIRE(write(fd_fifo, strs[3], strslen[3] - 3) ==
		    strslen[3] - 3);
		ipc_wakeup(ipcfd);

		ipc_wait(ipcfd);
		/* Finish the write */
		ATF_REQUIRE(write(fd_fifo,
		    &(strs[3][strslen[3] - 3]), 3) == 3);
		ipc_wakeup(ipcfd);
		_exit(0);
	}
	ipcfd[0] = pipedes[0];
	close(pipedes[1]);
	close(pipedes2[0]);
	ipcfd[1] = pipedes2[1];

	ATF_REQUIRE((fp = fopen("fifo", "r")) != NULL);
	setvbuf(fp, (char *)NULL, buf_mode, 0);
	ATF_REQUIRE((flags = fcntl(fileno(fp), F_GETFL, 0)) != -1);
	ATF_REQUIRE(fcntl(fileno(fp), F_SETFL, flags | O_NONBLOCK) >= 0);

	/* Wait until the writer completes its partial write. */
	ipc_wait(ipcfd);
	ATF_REQUIRE_ERRNO(EAGAIN,
	    (linelen = getdelim(&line, &linecap, delim, fp)) == -1);
	ATF_REQUIRE_STREQ("", line);
	ATF_REQUIRE(ferror(fp));
	ATF_REQUIRE(!feof(fp));
	clearerr(fp);
	ipc_wakeup(ipcfd);

	ipc_wait(ipcfd);
	/*
	 * Should now have the finished first line, a full second line,
	 * and a partial third line.
	 */
	ATF_CHECK(getdelim(&line, &linecap, delim, fp) == strslen[0]);
	ATF_REQUIRE_STREQ(strs[0], line);
	ATF_REQUIRE(getdelim(&line, &linecap, delim, fp) == strslen[1]);
	ATF_REQUIRE_STREQ(strs[1], line);

	ATF_REQUIRE_ERRNO(EAGAIN,
	    (linelen = getdelim(&line, &linecap, delim, fp)) == -1);
	ATF_REQUIRE_STREQ("", line);
	ATF_REQUIRE(ferror(fp));
	ATF_REQUIRE(!feof(fp));
	clearerr(fp);
	ipc_wakeup(ipcfd);

	/* Wait for the partial write to be completed and another to be done. */
	ipc_wait(ipcfd);
	ATF_REQUIRE((linelen = getdelim(&line, &linecap, delim, fp)) != -1);
	ATF_REQUIRE(!ferror(fp));
	ATF_REQUIRE(!feof(fp));
	ATF_REQUIRE_STREQ(strs[2], line);
	ATF_REQUIRE(linelen == strslen[2]);

	ATF_REQUIRE_ERRNO(EAGAIN,
	    (linelen = getdelim(&line, &linecap, delim, fp)) == -1);
	ATF_REQUIRE_STREQ("", line);
	ATF_REQUIRE(ferror(fp));
	ATF_REQUIRE(!feof(fp));
	clearerr(fp);
	ipc_wakeup(ipcfd);

	ipc_wait(ipcfd);
	ATF_REQUIRE((linelen = getdelim(&line, &linecap, delim, fp)) != -1);
	ATF_REQUIRE(!ferror(fp));
	ATF_REQUIRE(!feof(fp));
	ATF_REQUIRE_STREQ(strs[3], line);
	ATF_REQUIRE(linelen == strslen[3]);

	ATF_REQUIRE(waitpid(pid, &status, WEXITED) != -1);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);
}

ATF_TC_WITHOUT_HEAD(nonblock_eagain_buffered);
ATF_TC_BODY(nonblock_eagain_buffered, tc)
{

	_nonblock_eagain(_IOFBF);
}

ATF_TC_WITHOUT_HEAD(nonblock_eagain_unbuffered);
ATF_TC_BODY(nonblock_eagain_unbuffered, tc)
{

	_nonblock_eagain(_IONBF);
}


ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, getline_basic);
	ATF_TP_ADD_TC(tp, stream_error);
	ATF_TP_ADD_TC(tp, eof);
	ATF_TP_ADD_TC(tp, invalid_params);
	ATF_TP_ADD_TC(tp, nul);
	ATF_TP_ADD_TC(tp, empty_NULL_buffer);
	ATF_TP_ADD_TC(tp, nonblock_eagain_unbuffered);
	ATF_TP_ADD_TC(tp, nonblock_eagain_buffered);

	return (atf_no_error());
}
