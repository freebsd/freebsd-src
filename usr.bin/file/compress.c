/*
 * compress routines:
 *	is_compress() returns 0 if uncompressed, number of bits if compressed.
 *	uncompress(old, n, newch) - uncompress old into new, return sizeof new
 * compress.c,v 1.1 1993/06/10 00:38:05 jtc Exp
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "file.h"

/* Check for compression, return nbits. Algorithm, in magic(4) format:
 * 0       string          \037\235        compressed data
 * >2      byte&0x80       >0              block compressed
 * >2      byte&0x1f       x               %d bits
 */
int
is_compress(p, b)
const unsigned char *p;
int *b;
{

	if (*p != '\037' || *(/*signed*/ char*)(p+1) != '\235')
		return 0;	/* not compress()ed */

	*b = *(p+2) & 0x80;
	return *(p+2) & 0x1f;
}

int
uncompress(old, newch, n)
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

		execlp("uncompress", "uncompress", "-c", NULL);
		error("could not execute `uncompress' (%s).\n", 
		      strerror(errno));
		/*NOTREACHED*/
	case -1:
		error("could not fork (%s).\n", strerror(errno));
		/*NOTREACHED*/

	default: /* parent */
		(void) close(fdin[0]);
		(void) close(fdout[1]);
		if (write(fdin[1], old, n) != n) {
			error("write failed (%s).\n", strerror(errno));
			/*NOTREACHED*/
		}
		(void) close(fdin[1]);
		if ((*newch = (unsigned char *) malloc(n)) == NULL) {
			error("out of memory.\n");
			/*NOTREACHED*/
		}
		if ((n = read(fdout[0], *newch, n)) <= 0) {
			free(*newch);
			error("read failed (%s).\n", strerror(errno));
			/*NOTREACHED*/
		}
		(void) close(fdout[0]);
		(void) wait(NULL);
		return n;
	}
}
