#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rexecd/Makefile,v 1.16 2002/05/02 05:06:32 des Exp $

PROG=	rexecd
MAN=	rexecd.8
#WARNS?=	4
WFORMAT=0

DPADD=  ${LIBPAM} ${LIBUTIL}
LDADD=	${MINUSLPAM} -lutil

.include <bsd.prog.mk>
