#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/rm/Makefile,v 1.12 2000/02/05 18:42:29 joe Exp $

PROG=	rm
SRCS=	rm.c setflags.c

LINKS=	${BINDIR}/rm ${BINDIR}/unlink
MLINKS=	rm.1 unlink.1
.PATH:	${.CURDIR}/../../lib/libc/gen

.include <bsd.prog.mk>
