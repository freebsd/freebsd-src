/*
 *  To measure the speed of malloc - based on the algorithm described in
 *  "In Search of a Better Malloc" by David G. Korn and Kiem-Phong Vo,
 *  Usenix 1985. This is a vicious test of memory allocation, but does
 *  suffer from the problem that it asks for a uniform distribution of
 *  sizes - a more accurate distribution is a multi-normal distribution
 *  for all applications I've seen.
 */
/* Mark Moraes, CSRI, University of Toronto */
#ifndef lint
static char rcsid[] = "$Id: simumalloc.c,v 1.1 1994/03/06 23:01:46 nate Exp $";
#endif /*lint*/

#include <stdio.h>
#include <string.h>

/*
 * ANSI systems had better have this.  Non-ANSI systems had better not
 * complain about things that are implicitly declared int or void.
 */
#if defined(STDHEADERS)
# include <stdlib.h>
# include <unistd.h>
#endif

#include "malloc.h"

char *progname;
/* For getopt() */
extern int getopt();
extern int optind;
extern char *optarg;


int MaxTime, MaxLife, MaxSize, NumAllocs;

typedef union u {
	union u *ptr;
	int size;
} word;

#define MAXTIME 100000
static word *bufs[MAXTIME];

unsigned long alloced = 0;
static unsigned long maxalloced = 0;

#ifdef HAVE_RANDOM
extern long random();
#define rnd(x)		(random() % (long) (x))
#define seedrnd(x)	(srandom(x))
#else /* ! HAVE_RANDOM */
extern int rand();
#define rnd(x)		(rand() % (x))
#define seedrnd(x)	(srand(x))
#endif /* HAVE_RANDOM */

#ifdef MYMALLOC
extern char * (* _malloc_memfunc)();
#endif

/*
 *  generally sprintf() to errstring and then call complain rather than
 *  use a varargs routine
 */
char errstring[128];

/*
 *  Should probably have a more fancy version that does perror as well
 *  in a library someplace - like error()
 */
void
complain(s)
char *s;
{
	(void) fprintf(stderr, "%s: %s\n", progname, s);
	exit(-1);
}

void
usage()
{
	(void) fprintf(stderr, "\
Usage: %s [-t MaxTime] [-s MaxSize] [-l MaxLife] [-m Mmapfile] [-a] [-d]\n", progname);
	exit(-1);
}

int
main(argc, argv)
int argc;
char **argv;
{
	int c;
	register int t;
	char *before, *after;
	extern char *sbrk();
	extern int atoi();
	extern void freeall(), reserve();
	unsigned long grew;
	int alloconly = 0, use_mmap = 0, verbose = 0;

	progname = argv[0] ? argv[0] : "(no-argv[0])";
	NumAllocs = 1;
	MaxTime = 15000;
	MaxSize = 500;
	MaxLife = 1000;
	while((c = getopt(argc, argv, "n:t:s:l:dm:av")) != EOF) {
		/* optarg has the current argument if the option was followed by ':'*/
		switch (c) {
		case 't':
			MaxTime = atoi(optarg);
			if (MaxTime < 0 || MaxTime > MAXTIME) {
				(void) fprintf(stderr,
				 "%s: MaxTime must be >  0 and < %d\n", progname, MAXTIME);
				exit(-1);
			}
			break;
		case 's':
			MaxSize = atoi(optarg);
			if (MaxSize < 1)
				complain("MaxSize must be > 0");
			break;
		case 'l':
			MaxLife = atoi(optarg);
			if (MaxLife < 0)
				complain("MaxLife must be > 0");
			break;
		case 'n':
			NumAllocs = atoi(optarg);
			if (NumAllocs <= 0)
				complain("NumAllocs must be > 0");
			break;
		case 'd':
			/* Full heap debugging - S-L-O-W */
#ifdef MYMALLOC
			mal_debug(3);
#endif
			break;
		case 'm':
			use_mmap = 1;
#ifdef MYMALLOC
			mal_mmap(optarg);
#else
			complain("-m option needs CSRI malloc");
#endif
			break;
		case 'a':
			/* Only allocate -- no free */
			alloconly = 1;
			break;
		case 'v':
			verbose++;
			break;
		case '?':
			usage();
			break;
		}
	}
	/* Any filenames etc. after all the options */
	if (optind < argc) {
		usage();
	}

	for(t = 0; t < MaxTime; t++)
		bufs[t] = 0;

#ifdef MYMALLOC
	before = (* _malloc_memfunc)(0);
#else
	before = sbrk(0);
#endif
	for(t = 0; t < MaxTime; t++) {
		register int n;

		for(n = rnd(NumAllocs) + 1; n > 0; n--) {
			int s, l;
			
			s = rnd(MaxSize) + 2;
			l = rnd(MaxLife) + 1;
			reserve(s, t + l);
		}
		if (! alloconly)
			freeall(t);
	}
#ifdef MYMALLOC
	after = (* _malloc_memfunc)(0);
#else
	after = sbrk(0);
#endif
	grew = after - before;
	(void) sprintf(errstring, "Sbrked %ld,  MaxAlloced %ld, Wastage %.2f\n",
		       grew, maxalloced * sizeof(word),
		       grew == 0 ? 0.0 :
		       (1.0 - ((double) maxalloced * sizeof(word)) / grew));
	(void) write(1, errstring, strlen(errstring));
#ifdef MYMALLOC
	if (verbose)
		(void) mal_statsdump(stderr);
#endif
	return 0;
}

/*
 *  Mallocs a block s words long, and adds it to the list of blocks to
 *  be freed at time tfree
 */
void
reserve(s, tfree)
int s;
int tfree;
{
	word *wp;

	wp = (word *) malloc(s * sizeof(word));
	if (wp == NULL)
		complain("Out of memory");
	wp[0].ptr = bufs[tfree];
	wp[1].size = s;
	bufs[tfree] = wp;
	alloced += s;
	if (alloced > maxalloced)
		maxalloced = alloced;
}

/* free all blocks whose lifetime expires at time t */
void
freeall(t)
int t;
{
	word *wp;

	wp = bufs[t];
	while(wp != NULL) {
		word *tmp = wp[0].ptr;
		alloced -= wp[1].size;
		free((char *) wp);
		wp = tmp;
	}
}
