#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/rm/Makefile,v 1.5.2.2 1999/12/29 19:39:45 obrien Exp $

PROG=	rm
SRCS=   rm.c stat_flags.c

LINKS=	${BINDIR}/rm ${BINDIR}/unlink
MLINKS=	rm.1 unlink.1

.PATH: ${.CURDIR}/../ls

.include <bsd.prog.mk>
