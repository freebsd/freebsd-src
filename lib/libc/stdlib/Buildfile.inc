# $FreeBSD$

# machine-independent stdlib sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/stdlib ${.CURDIR}/stdlib

MISRCS += \
	_Exit.c \
	a64l.c \
	abort.c \
	abs.c \
	atexit.c \
	atof.c \
	atoi.c \
	atol.c \
	atoll.c \
	bsearch.c \
	div.c \
	exit.c \
	getenv.c \
	getopt.c \
	getopt_long.c \
	getsubopt.c \
	hcreate.c \
	heapsort.c \
	imaxabs.c \
	imaxdiv.c \
	insque.c \
	l64a.c \
	labs.c \
	ldiv.c \
	llabs.c \
	lldiv.c \
	lsearch.c \
	malloc.c \
	merge.c \
	ptsname.c \
	qsort.c \
	qsort_r.c \
	radixsort.c \
	rand.c \
	random.c \
	reallocf.c \
	realpath.c \
	remque.c \
	strfmon.c \
	strtoimax.c \
	strtol.c \
	strtoll.c \
	strtonum.c \
	strtoq.c \
	strtoul.c \
	strtoull.c \
	strtoumax.c \
	strtouq.c \
	system.c \
	tdelete.c \
	tfind.c \
	tsearch.c \
	twalk.c

SYM_MAPS += ${.CURDIR}/stdlib/Symbol.map

# machine-dependent stdlib sources
.if exists(${.CURDIR}/${MACHINE_ARCH}/stdlib/Buildfile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/stdlib/Buildfile.inc"
.endif
