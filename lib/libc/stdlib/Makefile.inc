#	@(#)Makefile.inc	8.2 (Berkeley) 2/16/94

# machine-independent stdlib sources
.PATH: ${.CURDIR}/${MACHINE}/stdlib ${.CURDIR}/stdlib

SRCS+=	abort.c atexit.c atof.c atoi.c atol.c bsearch.c calloc.c div.c \
	exit.c getenv.c getopt.c getsubopt.c heapsort.c labs.c ldiv.c \
	malloc.c merge.c putenv.c qsort.c radixsort.c rand.c random.c \
	realpath.c setenv.c strtod.c strtol.c strtoq.c strtoul.c \
	strtouq.c system.c

# machine-dependent stdlib sources
.include "${.CURDIR}/${MACHINE}/stdlib/Makefile.inc"

MAN3+=	abort.0 abs.0 alloca.0 atexit.0 atof.0 atoi.0 atol.0 bsearch.0 \
	calloc.0 div.0 exit.0 free.0 getenv.0 getopt.0 getsubopt.0 labs.0 \
	ldiv.0 malloc.0 memory.0 qsort.0 radixsort.0 rand.0 random.0 \
	realloc.0 realpath.0 strtod.0 strtol.0 strtoul.0 system.0

MLINKS+=getenv.3 setenv.3 getenv.3 unsetenv.3 getenv.3 putenv.3
MLINKS+=qsort.3 heapsort.3 qsort.3 mergesort.3
MLINKS+=rand.3 srand.3
MLINKS+=random.3 initstate.3 random.3 setstate.3 random.3 srandom.3
MLINKS+=strtol.3 strtoq.3
MLINKS+=strtoul.3 strtouq.3
