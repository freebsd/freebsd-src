#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD: src/usr.bin/compress/Makefile,v 1.7 2002/02/08 22:31:37 markm Exp $

PROG=	compress
SRCS=	compress.c zopen.c
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress
MLINKS=	compress.1 uncompress.1

# XXX zopen is not part of libc
# MAN=zopen.3

.include <bsd.prog.mk>
