#	from @(#)Makefile.inc	8.3 (Berkeley) 2/4/95
#	$Id: Makefile.inc,v 1.9 1997/06/22 17:54:24 phk Exp $

# machine-independent stdlib sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/stdlib ${.CURDIR}/../libc/stdlib

SRCS+=	abort.c atexit.c atof.c atoi.c atol.c bsearch.c calloc.c div.c \
	exit.c getenv.c getopt.c getsubopt.c strhash.c heapsort.c labs.c \
	ldiv.c malloc.c merge.c putenv.c qsort.c radixsort.c rand.c random.c \
	realpath.c setenv.c strtod.c strtol.c strtoq.c strtoul.c \
	strtouq.c system.c

# machine-dependent stdlib sources
.include "${.CURDIR}/../libc/${MACHINE}/stdlib/Makefile.inc"

# Only build man pages with libc.
.if ${LIB} == "c"
MAN3+=	stdlib/abort.3 stdlib/abs.3 stdlib/alloca.3 stdlib/atexit.3 \
	stdlib/atof.3 stdlib/atoi.3 stdlib/atol.3 stdlib/bsearch.3 \
	stdlib/div.3 stdlib/exit.3 \
	stdlib/getenv.3 stdlib/getopt.3 stdlib/getsubopt.3 stdlib/labs.3 \
	stdlib/ldiv.3 stdlib/malloc.3 stdlib/memory.3 stdlib/qsort.3 \
	stdlib/radixsort.3 stdlib/rand.3 stdlib/random.3 \
	stdlib/realpath.3 stdlib/strtod.3 stdlib/strtol.3 stdlib/strtoul.3 \
	stdlib/system.3

MLINKS+=getenv.3 setenv.3 getenv.3 unsetenv.3 getenv.3 putenv.3
MLINKS+=qsort.3 heapsort.3 qsort.3 mergesort.3
MLINKS+=rand.3 srand.3
MLINKS+=random.3 initstate.3 random.3 setstate.3 random.3 srandom.3 \
	random.3 srandomdev.3
MLINKS+=strtol.3 strtoq.3
MLINKS+=strtoul.3 strtouq.3
MLINKS+=malloc.3 free.3 malloc.3 realloc.3 malloc.3 calloc.3
.endif
