#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=compat
CFLAGS+=-DLIBC_SCCS -DSYSLIBC_SCCS
AINC=	-I${.CURDIR}/../libc/${MACHINE}

.PATH:	${.CURDIR}/4.1/${MACHINE} ${.CURDIR}/4.1 \
	${.CURDIR}/4.3/${MACHINE} ${.CURDIR}/4.3 \
	${.CURDIR}/4.4/${MACHINE} ${.CURDIR}/4.4 \
	${.CURDIR}/regexp

# compat 4.1 sources
SRCS=	ftime.c getpw.c gtty.c stty.c tell.c vlimit.c vtimes.c

.if (${MACHINE} == "vax")
SRCS+=	reset.s
.endif

MAN3+=	ftime.0 getpw.0 stty.0 vlimit.0 vtimes.0

MLINKS+=stty.3 gtty.3

# compat 4.3 sources
SRCS+=	cfree.c ecvt.c gcvt.c lsearch.c regex.c rexec.c sibuf.c sobuf.c \
	strout.c

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

MAN3+=	ecvt.0 insque.0 lsearch.0 re_comp.0 rexec.0

MLINKS+=ecvt.3 fcvt.3 ecvt.3 gcvt.3
MLINKS+=insque.3 remque.3
MLINKS+=re_comp.3 re_exec.3

# compat 4.4 sources
SRCS+=	cuserid.c

# regexp sources
SRCS+=	regerror.c regexp.c regsub.c

MAN3+=	regexp.0

MLINKS+=regexp.3 regcomp.3 regexp.3 regexec.3 regexp.3 regsub.3 \
	regexp.3 regerror.3

.include <bsd.lib.mk>
