/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/sys/netiso/xebec/Attic/malloc.c,v 1.2 1995/05/30 08:12:07 rgrimes Exp $ */
/* $Source: /pub/FreeBSD/FreeBSD-CVS/src/sys/netiso/xebec/Attic/malloc.c,v $ */
/*
 * This code is such a kludge that I don't want to put my name on it.
 * It was a ridiculously fast hack and needs rewriting.
 * However it does work...
 */

/*
 * a simple malloc
 * it might be brain-damaged but for the purposes of xebec
 * it's a whole lot faster than the c library malloc
 */

#include <stdio.h>
#include "malloc.h"
#include "debug.h"
#define CHUNKSIZE 4096*2

static char *hiwat, *highend;
int bytesmalloced=0;
int byteswasted = 0;


init_alloc()
{
#ifdef LINT
	hiwat = 0;
	highend = 0;
#else LINT
	extern char *sbrk();

	hiwat = (char *) sbrk(0);
	hiwat = (char *)((unsigned)(hiwat + 3) & ~0x3);
	highend = hiwat;
#endif LINT
}

HIWAT(s)
char *s;
{
	IFDEBUG(M)
		fprintf(stdout, "HIWAT 0x%x  %s\n", hiwat,s);
		fflush(stdout);
	ENDDEBUG
}

#define MIN(x,y) ((x<y)?x:y)

char *Malloc(x)
int x;
{
	char *c;
	extern char *sbrk();
	static int firsttime=1;
	int total = x;
	int first_iter = 1;
	char *returnvalue;

	IFDEBUG(N)
		fprintf(stdout, "Malloc 0x%x, %d, bytesmalloced %d\n",
			total,total, bytesmalloced);
		fflush(stdout);
	ENDDEBUG
	IFDEBUG(M)
		fprintf(stdout, "Malloc 0x%x, %d, hiwat 0x%x\n",
			total,total, hiwat);
		fflush(stdout);
	ENDDEBUG
	if(firsttime) {
		hiwat = sbrk(0);
		if(((unsigned)(hiwat) & 0x3)) {
			bytesmalloced = 4 - (int) ((unsigned)(hiwat) & 0x3);
			hiwat = sbrk( bytesmalloced );
		} else
			bytesmalloced = 0;
		firsttime = 0;
		highend = hiwat;
	}
	while( total ) {
		x = MIN(CHUNKSIZE, total);
		if(total != x)  {
			IFDEBUG(N)
				fprintf(stdout, "BIG Malloc tot %d, x %d, left %d net %d\n",
					total,x, total-x, bytesmalloced);
				fflush(stdout);
			ENDDEBUG
		}
		if ( (hiwat + x) > highend) {
			c = sbrk(CHUNKSIZE);
			IFDEBUG(M)
				fprintf(stdout, "hiwat 0x%x, x 0x%x, highend 0x%x, c 0x%x\n",
						hiwat, x, highend, c);
				fflush(stdout);
			ENDDEBUG
			if( c == (char *) -1 ) {
				fprintf(stderr, "Ran out of memory!\n");
				Exit(-1);
			}
			if(first_iter) {
				returnvalue = c;
				first_iter = 0;
			}
			bytesmalloced +=  CHUNKSIZE;
			IFDEBUG(m)
				if (highend != c) {
					fprintf(OUT, "warning: %d wasted bytes!\n", highend - hiwat);
				fprintf(OUT, " chunksize 0x%x,  x 0x%x \n", CHUNKSIZE, x);
				}
			ENDDEBUG
			highend = c + CHUNKSIZE;
			hiwat = c;
		}
		c = hiwat;
		if(first_iter) {
			returnvalue = c;
			first_iter = 0;
		}
		hiwat += x;
		total -= x;
	}
	if((unsigned)hiwat & 0x3) {
		byteswasted += (int)((unsigned)(hiwat) & 0x3);
		hiwat = (char *)((unsigned)(hiwat + 3) & ~0x3);
	}
	IFDEBUG(M)
		fprintf(stdout, "Malloc = 0x%x, bytesm 0x%x, wasted 0x%x, hiwat 0x%x\n",
			returnvalue, bytesmalloced, byteswasted, hiwat);
	ENDDEBUG
	IFDEBUG(N)
		fprintf(stdout, "Malloc returns 0x%x, sbrk(0) 0x%x\n", returnvalue, sbrk(0));
		fflush(stdout);
	ENDDEBUG
	return(returnvalue);
}

