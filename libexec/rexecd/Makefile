#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	rexecd
MAN=	rexecd.8

CFLAGS+=-DOPIE
WFORMAT=0

DPADD=  ${LIBOPIE} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
LDADD=	-lopie -lmd -lcrypt -lutil

.include <bsd.prog.mk>
