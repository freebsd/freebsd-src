#	@(#)Makefile.inc	8.2 (Berkeley) 2/16/94

# machine-independent stdlib sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/stdlib ${.CURDIR}/../libc/stdlib

SRCS+=	abort.c atexit.c atof.c atoi.c atol.c bsearch.c calloc.c div.c \
	exit.c getenv.c getopt.c getsubopt.c strhash.c heapsort.c labs.c \
	ldiv.c malloc.c merge.c putenv.c qsort.c radixsort.c rand.c random.c \
	realpath.c setenv.c strtod.c strtol.c strtoq.c strtoul.c \
	strtouq.c system.c

# machine-dependent stdlib sources
.include "${.CURDIR}/${MACHINE}/stdlib/Makefile.inc"
