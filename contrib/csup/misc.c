/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/md5.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fattr.h"
#include "main.h"
#include "misc.h"

struct pattlist {
	char **patterns;
	size_t size;
	size_t in;
};

struct backoff_timer {
	time_t min;
	time_t max;
	time_t interval;
	float backoff;
	float jitter;
};

static void	bt_update(struct backoff_timer *);
static void	bt_addjitter(struct backoff_timer *);

int
asciitoint(const char *s, int *val, int base)
{
	char *end;
	long longval;

	errno = 0;
	longval = strtol(s, &end, base);
	if (errno || *end != '\0')
		return (-1);
	if (longval > INT_MAX || longval < INT_MIN) {
		errno = ERANGE;
		return (-1);
	}
	*val = longval;
	return (0);
}

int
lprintf(int level, const char *fmt, ...)
{
	FILE *to;
	va_list ap;
	int ret;

	if (level > verbose)
		return (0);
	if (level == -1)
		to = stderr;
	else
		to = stdout;
	va_start(ap, fmt);
	ret = vfprintf(to, fmt, ap);
	va_end(ap);
	fflush(to);
	return (ret);
}

/*
 * Compute the MD5 checksum of a file.  The md parameter must
 * point to a buffer containing at least MD5_DIGEST_SIZE bytes.
 *
 * Do not confuse OpenSSL's MD5_DIGEST_LENGTH with our own
 * MD5_DIGEST_SIZE macro.
 */
int
MD5_File(char *path, char *md)
{
	char buf[1024];
	MD5_CTX ctx;
	ssize_t n;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return (-1);
	MD5_Init(&ctx);
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		MD5_Update(&ctx, buf, n);
	close(fd);
	if (n == -1)
		return (-1);
	MD5_End(md, &ctx);
	return (0);
}

/*
 * Wrapper around MD5_Final() that converts the 128 bits MD5 hash
 * to an ASCII string representing this value in hexadecimal.
 */
void
MD5_End(char *md, MD5_CTX *c)
{
	unsigned char md5[MD5_DIGEST_LENGTH];
	const char hex[] = "0123456789abcdef";
	int i, j;

	MD5_Final(md5, c);
	j = 0;
	for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
		md[j++] = hex[md5[i] >> 4];
		md[j++] = hex[md5[i] & 0xf];
	}
	md[j] = '\0';
}

int
pathcmp(const char *s1, const char *s2)
{
	char c1, c2;

	do {
		c1 = *s1++;
		if (c1 == '/')
			c1 = 1;
		c2 = *s2++;
		if (c2 == '/')
			c2 = 1;
	} while (c1 == c2 && c1 != '\0');

	return (c1 - c2);
}

size_t
commonpathlength(const char *a, size_t alen, const char *b, size_t blen)
{
	size_t i, minlen, lastslash;

	minlen = min(alen, blen);
	lastslash = 0;
	for (i = 0; i < minlen; i++) {
		if (a[i] != b[i])
			return (lastslash);
		if (a[i] == '/') {
			if (i == 0)	/* Include the leading slash. */
				lastslash = 1;
			else
				lastslash = i;
		}
	}

	/* One path is a prefix of the other/ */
	if (alen > minlen) {		/* Path "b" is a prefix of "a". */
		if (a[minlen] == '/')
			return (minlen);
		else
			return (lastslash);
	} else if (blen > minlen) {	/* Path "a" is a prefix of "b". */
		if (b[minlen] == '/')
			return (minlen);
		else
			return (lastslash);
	}

	/* The paths are identical. */
	return (minlen);
}

char *
pathlast(char *path)
{
	char *s;

	s = strrchr(path, '/');
	if (s == NULL)
		return (path);
	return (++s);
}

int
rcsdatetotm(const char *revdate, struct tm *tm)
{
	char *cp;
	size_t len;

	cp = strchr(revdate, '.');
	if (cp == NULL)
		return (-1);
	len = cp - revdate;
	if (len >= 4)
		cp = strptime(revdate, "%Y.%m.%d.%H.%M.%S", tm);
	else if (len == 2)
		cp = strptime(revdate, "%y.%m.%d.%H.%M.%S", tm);
	else
		return (-1);
	if (cp == NULL || *cp != '\0')
		return (-1);
	return (0);
}

time_t
rcsdatetotime(const char *revdate)
{
	struct tm tm;
	time_t t;
	int error;

	error = rcsdatetotm(revdate, &tm);
	if (error)
		return (error);
	t = timegm(&tm);
	return (t);
}

/*
 * Returns a buffer allocated with malloc() containing the absolute
 * pathname to the checkout file made from the prefix and the path
 * of the corresponding RCS file relatively to the prefix.  If the
 * filename is not an RCS filename, NULL will be returned.
 */
char *
checkoutpath(const char *prefix, const char *file)
{
	const char *cp;
	char *path;
	size_t len;

	if (file[0] == '/')
		return (NULL);
	cp = file;
	while ((cp = strstr(cp, "..")) != NULL) {
		if (cp == file || cp[2] == '\0' ||
		    (cp[-1] == '/' && cp[2] == '/'))
			return (NULL);
		cp += 2;
	}
	len = strlen(file);
	if (len < 2 || file[len - 1] != 'v' || file[len - 2] != ',')
		return (NULL);
	xasprintf(&path, "%s/%.*s", prefix, (int)len - 2, file);
	return (path);
}

int
mkdirhier(char *path, mode_t mask)
{
	struct fattr *fa;
	size_t i, last, len;
	int error, finish, rv;

	finish = 0;
	last = 0;
	len = strlen(path);
	for (i = len - 1; i > 0; i--) {
		if (path[i] == '/') {
			path[i] = '\0';
			if (access(path, F_OK) == 0) {
				path[i] = '/';
				break;
			}
			if (errno != ENOENT) {
				path[i] = '/';
				if (last == 0)
					return (-1);
				finish = 1;
				break;
			}
			last = i;
		}
	}
	if (last == 0)
		return (0);

	i = strlen(path);
	fa = fattr_new(FT_DIRECTORY, -1);
	fattr_mergedefault(fa);
	fattr_umask(fa, mask);
	while (i < len) {
		if (!finish) {
			rv = 0;
			error = fattr_makenode(fa, path);
			if (!error)
				rv = fattr_install(fa, path, NULL);
			if (error || rv == -1)
				finish = 1;
		}
		path[i] = '/';
		i += strlen(path + i);
        }
	assert(i == len);
	if (finish)
		return (-1);
        return (0);
}

/*
 * Compute temporary pathnames.
 * This can look a bit like overkill but we mimic CVSup's behaviour.
 */
#define	TEMPNAME_PREFIX		"#cvs.csup"

static pthread_mutex_t tempname_mtx = PTHREAD_MUTEX_INITIALIZER;
static pid_t tempname_pid = -1;
static int tempname_count;

char *
tempname(const char *path)
{
	char *cp, *temp;
	int count, error;

	error = pthread_mutex_lock(&tempname_mtx);
	assert(!error);
	if (tempname_pid == -1) {
		tempname_pid = getpid();
		tempname_count = 0;
	}
	count = tempname_count++;
	error = pthread_mutex_unlock(&tempname_mtx);
	assert(!error);
	cp = strrchr(path, '/');
	if (cp == NULL)
		xasprintf(&temp, "%s-%ld.%d", TEMPNAME_PREFIX,
		    (long)tempname_pid, count);
	else
		xasprintf(&temp, "%.*s%s-%ld.%d", (int)(cp - path + 1), path,
		    TEMPNAME_PREFIX, (long)tempname_pid, count);
	return (temp);
}

void *
xmalloc(size_t size)
{
	void *buf;

	buf = malloc(size);
	if (buf == NULL)
		err(1, "malloc");
	return (buf);
}

void *
xrealloc(void *buf, size_t size)
{

	buf = realloc(buf, size);
	if (buf == NULL)
		err(1, "realloc");
	return (buf);
}

char *
xstrdup(const char *str)
{
	char *buf;

	buf = strdup(str);
	if (buf == NULL)
		err(1, "strdup");
	return (buf);
}

int
xasprintf(char **ret, const char *format, ...)
{
	va_list ap;
	int rv;

	va_start(ap, format);
	rv = vasprintf(ret, format, ap);
	va_end(ap);
	if (*ret == NULL)
		err(1, "asprintf");
	return (rv);
}

struct pattlist *
pattlist_new(void)
{
	struct pattlist *p;

	p = xmalloc(sizeof(struct pattlist));
	p->size = 4;		/* Initial size. */
	p->patterns = xmalloc(p->size * sizeof(char *));
	p->in = 0;
	return (p);
}

void
pattlist_add(struct pattlist *p, const char *pattern)
{

	if (p->in == p->size) {
		p->size *= 2;
		p->patterns = xrealloc(p->patterns, p->size * sizeof(char *));
	}
	assert(p->in < p->size);
	p->patterns[p->in++] = xstrdup(pattern);
}

char *
pattlist_get(struct pattlist *p, size_t i)
{

	assert(i < p->in);
	return (p->patterns[i]);
}

size_t
pattlist_size(struct pattlist *p)
{

	return (p->in);
}

void
pattlist_free(struct pattlist *p)
{
	size_t i;
	
	for (i = 0; i < p->in; i++)
		free(p->patterns[i]);
	free(p->patterns);
	free(p);
}

/* Creates a backoff timer. */
struct backoff_timer *
bt_new(time_t min, time_t max, float backoff, float jitter)
{
	struct backoff_timer *bt;

	bt = xmalloc(sizeof(struct backoff_timer));
	bt->min = min;
	bt->max = max;
	bt->backoff = backoff;
	bt->jitter = jitter;
	bt->interval = min;
	bt_addjitter(bt);
	srandom(time(0));
	return (bt);
}

/* Updates the backoff timer. */
static void
bt_update(struct backoff_timer *bt)
{

	bt->interval = (time_t)min(bt->interval * bt->backoff, bt->max);
	bt_addjitter(bt);
}

/* Adds some jitter. */
static void
bt_addjitter(struct backoff_timer *bt)
{
	long mag;

	mag = (long)(bt->jitter * bt->interval);
	/* We want a random number between -mag and mag. */
	bt->interval += (time_t)(random() % (2 * mag) - mag);
}

/* Returns the current timer value. */
time_t
bt_get(struct backoff_timer *bt)
{

	return (bt->interval);
}

/* Times out for bt->interval seconds. */
void
bt_pause(struct backoff_timer *bt)
{

	sleep(bt->interval);
	bt_update(bt);
}

void
bt_free(struct backoff_timer *bt)
{

	free(bt);
}
