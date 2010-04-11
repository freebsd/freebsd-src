#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

LIB=	compat
CFLAGS+=-DLIBC_SCCS -DSYSLIBC_SCCS -I${.CURDIR}/../libc/locale
NO_PIC=

WARNS?=	0

.PATH:	${.CURDIR}/4.1 ${.CURDIR}/4.3 ${.CURDIR}/4.4

# compat 4.1 sources
SRCS+=	ascftime.c cftime.c ftime.c getpw.c

MAN+=	4.1/ftime.3 4.1/getpw.3
MAN+=	4.1/cftime.3

MLINKS+=cftime.3 ascftime.3

# compat 4.3 sources
SRCS+=	cfree.c re_comp.c rexec.c

MAN+=	4.3/cfree.3 4.3/re_comp.3 4.3/rexec.3

MLINKS+=re_comp.3 re_exec.3

# compat 4.4 sources
SRCS+=	cuserid.c

MAN+=	4.4/cuserid.3

.include <bsd.lib.mk>
