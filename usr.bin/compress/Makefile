#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD$

PROG=	compress
SRCS=	compress.c zopen.c
WARNS?= 2
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS=	compress.1 uncompress.1

# XXX zopen is not part of libc
# MAN=zopen.3

.include <bsd.prog.mk>
