#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/date/Makefile,v 1.10 2002/02/04 02:49:18 kris Exp $

PROG=	date
SRCS=	date.c netdate.c vary.c
WARNS=	0
WFORMAT=0
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
