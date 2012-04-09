#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD: src/usr.bin/compress/Makefile,v 1.8.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	compress
SRCS=	compress.c zopen.c
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS=	compress.1 uncompress.1
WARNS?=	6

# XXX zopen is not part of libc
# MAN=zopen.3

.include <bsd.prog.mk>
