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
FILE_RCSID("@(#)$Id: compress.c,v 1.17 2000/08/05 17:36:47 christos Exp $")
#endif


static struct {
	const char *magic;
	int   maglen;
	const char *const argv[3];
	int	 silent;
} compr[] = {
	{ "\037\235", 2, { "uncompress", "-c", NULL }, 0 },	/* compressed */
	{ "\037\235", 2, { "gzip", "-cdq", NULL }, 1 },		/* compressed */
	{ "\037\213", 2, { "gzip", "-cdq", NULL }, 1 },		/* gzipped */
	{ "\037\236", 2, { "gzip", "-cdq", NULL }, 1 },		/* frozen */
	{ "\037\240", 2, { "gzip", "-cdq", NULL }, 1 },		/* SCO LZH */
	/* the standard pack utilities do not accept standard input */
	{ "\037\036", 2, { "gzip", "-cdq", NULL }, 0 },		/* packed */
	{ "BZh",      3, { "bzip2", "-d", NULL }, 1 },		/* bzip2-ed */
};

static int ncompr = sizeof(compr) / sizeof(compr[0]);


static int uncompress __P((int, const unsigned char *, unsigned char **, int));

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
		if (write(fdin[1], old, n) != n)
			return 0;
		(void) close(fdin[1]);
		if ((*newch = (unsigned char *) malloc(n)) == NULL)
			return 0;
		if ((n = read(fdout[0], *newch, n)) <= 0) {
			free(*newch);
			return 0;
		}
		(void) close(fdout[0]);
		(void) wait(NULL);
		return n;
	}
}
