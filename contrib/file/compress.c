/*
 * compress routines:
 *	zmagic() - returns 0 if not recognized, uncompresses and prints
 *		   information if recognized
 *	uncompress(method, old, n, newch) - uncompress old into new, 
 *					    using method, return sizeof new
 */
#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef lint
FILE_RCSID("@(#)$Id: compress.c,v 1.19 2001/03/20 04:22:02 christos Exp $")
#endif


static struct {
	const char *magic;
	int   maglen;
	const char *const argv[3];
	int	 silent;
} compr[] = {
	{ "\037\235", 2, { "gzip", "-cdq", NULL }, 1 },		/* compressed */
	/* Uncompress can get stuck; so use gzip first if we have it
	 * Idea from Damien Clark, thanks! */
	{ "\037\235", 2, { "uncompress", "-c", NULL }, 1 },	/* compressed */
	{ "\037\213", 2, { "gzip", "-cdq", NULL }, 1 },		/* gzipped */
	{ "\037\236", 2, { "gzip", "-cdq", NULL }, 1 },		/* frozen */
	{ "\037\240", 2, { "gzip", "-cdq", NULL }, 1 },		/* SCO LZH */
	/* the standard pack utilities do not accept standard input */
	{ "\037\036", 2, { "gzip", "-cdq", NULL }, 0 },		/* packed */
	{ "BZh",      3, { "bzip2", "-d", NULL }, 1 },		/* bzip2-ed */
};

static int ncompr = sizeof(compr) / sizeof(compr[0]);


static int uncompress __P((int, const unsigned char *, unsigned char **, int));
static int swrite __P((int, const void *, size_t));
static int sread __P((int, void *, size_t));

int
zmagic(buf, nbytes)
	unsigned char *buf;
	int nbytes;
{
	unsigned char *newbuf;
	int newsize;
	int i;

	for (i = 0; i < ncompr; i++) {
		if (nbytes < compr[i].maglen)
			continue;
		if (memcmp(buf, compr[i].magic, compr[i].maglen) == 0 &&
		    (newsize = uncompress(i, buf, &newbuf, nbytes)) != 0) {
			tryit(newbuf, newsize, 1);
			free(newbuf);
			printf(" (");
			tryit(buf, nbytes, 0);
			printf(")");
			return 1;
		}
	}

	if (i == ncompr)
		return 0;

	return 1;
}

/*
 * `safe' write for sockets and pipes.
 */
static int
swrite(fd, buf, n)
	int fd;
	const void *buf;
	size_t n;
{
	int rv;
	size_t rn = n;

	do
		switch (rv = write(fd, buf, n)) {
		case -1:
			if (errno == EINTR)
				continue;
			return -1;
		default:
			n -= rv;
			buf = ((char *)buf) + rv;
			break;
		}
	while (n > 0);
	return rn;
}


/*
 * `safe' read for sockets and pipes.
 */
static int
sread(fd, buf, n)
	int fd;
	void *buf;
	size_t n;
{
	int rv;
	size_t rn = n;

	do
		switch (rv = read(fd, buf, n)) {
		case -1:
			if (errno == EINTR)
				continue;
			return -1;
		default:
			n -= rv;
			buf = ((char *)buf) + rv;
			break;
		}
	while (n > 0);
	return rn;
}

static int
uncompress(method, old, newch, n)
	int method;
	const unsigned char *old;
	unsigned char **newch;
	int n;
{
	int fdin[2], fdout[2];

	if (pipe(fdin) == -1 || pipe(fdout) == -1) {
		error("cannot create pipe (%s).\n", strerror(errno));	
		/*NOTREACHED*/
	}
	switch (fork()) {
	case 0:	/* child */
		(void) close(0);
		(void) dup(fdin[0]);
		(void) close(fdin[0]);
		(void) close(fdin[1]);

		(void) close(1);
		(void) dup(fdout[1]);
		(void) close(fdout[0]);
		(void) close(fdout[1]);
		if (compr[method].silent)
			(void) close(2);

		execvp(compr[method].argv[0],
		       (char *const *)compr[method].argv);
		exit(1);
		/*NOTREACHED*/
	case -1:
		error("could not fork (%s).\n", strerror(errno));
		/*NOTREACHED*/

	default: /* parent */
		(void) close(fdin[0]);
		(void) close(fdout[1]);
		if (swrite(fdin[1], old, n) != n) {
			n = 0;
			goto err;
		}
		(void) close(fdin[1]);
		fdin[1] = -1;
		if ((*newch = (unsigned char *) malloc(n)) == NULL) {
			n = 0;
			goto err;
		}
		if ((n = sread(fdout[0], *newch, n)) <= 0) {
			free(*newch);
			n = 0;
			goto err;
		}
err:
		if (fdin[1] != -1)
			(void) close(fdin[1]);
		(void) close(fdout[0]);
		(void) wait(NULL);
		return n;
	}
}
