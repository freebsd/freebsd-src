#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=compat
CFLAGS+=-DLIBC_SCCS -DSYSLIBC_SCCS -DCOMPAT_WEAK_SEEDING
AINC=	-I${.CURDIR}/../libc/${MACHINE}
NOPIC=

.PATH:	${.CURDIR}/4.1/${MACHINE} ${.CURDIR}/4.1 \
	${.CURDIR}/4.3/${MACHINE} ${.CURDIR}/4.3 \
	${.CURDIR}/4.4/${MACHINE} ${.CURDIR}/4.4 \
	${.CURDIR}/regexp \
	${.CURDIR}/SysV \
	${.CURDIR}/../libc/stdlib

# will be removed with the next FreeBSD release
SRCS=	random.c

# compat 4.1 sources
# XXX MISSING:	tell.c vlimit.c vtimes.c
SRCS+=	ascftime.c cftime.c ftime.c getpw.c gtty.c stty.c

.if (${MACHINE} == "vax")
SRCS+=	reset.s
.endif

MAN3+=	4.1/ftime.3 4.1/getpw.3 4.1/stty.3 4.1/vlimit.3 4.1/vtimes.3
MAN3+=	4.1/cftime.3

MLINKS+=stty.3 gtty.3
MLINKS+=cftime.3 ascftime.3

# compat 4.3 sources
# XXX MISSING:	ecvt.c gcvt.c sibuf.c sobuf.c strout.c
SRCS+=	cfree.c lsearch.c regex.c rexec.c

.if	(${MACHINE} == "hp300" || ${MACHINE} == "luna68k")
SRCS+=	insque.s remque.s
.elif	(${MACHINE} == "i386")
SRCS+=	insque.c remque.c
.elif	(${MACHINE} == "mips")
SRCS+=	insque.c remque.c
.elif	(${MACHINE} == "sparc")
SRCS+=	insque.c remque.c
.elif	(${MACHINE} == "tahoe")
SRCS+=	nargs.s insque.s remque.s
.elif	(${MACHINE} == "vax")
SRCS+=	nargs.s insque.s remque.s
.endif

# XXX MISSING:	ecvt.0
MAN3+=  4.3/cfree.3 4.3/insque.3 4.3/lsearch.3 4.3/re_comp.3 4.3/rexec.3

# XXX MISSING:	ecvt.3, so can't MLINK
#MLINKS+=ecvt.3 fcvt.3 ecvt.3 gcvt.3
MLINKS+=insque.3 remque.3
MLINKS+=re_comp.3 re_exec.3
MLINKS+=lsearch.3 lfind.3

# compat 4.4 sources
SRCS+=	cuserid.c
MAN3+=	4.4/cuserid.3

# regexp sources
SRCS+=	regerror.c regexp.c regsub.c

MAN3+=	regexp/regexp.3

# XXX name clash with libc
# MLINKS+=regexp.3 regcomp.3 regexp.3 regexec.3 regexp.3 regerror.3
MLINKS+=regexp.3 regsub.3

# SysV compat sources
SRCS+=	ftok.c

MAN3+=	SysV/ftok.3


.include <bsd.lib.mk>
