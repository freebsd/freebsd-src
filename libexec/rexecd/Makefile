#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	rexecd
MAN=	rexecd.8
WARNS?=	6
WFORMAT=0

DPADD=  ${LIBPAM} ${LIBUTIL}
LDADD=	${MINUSLPAM} -lutil

.include <bsd.prog.mk>
