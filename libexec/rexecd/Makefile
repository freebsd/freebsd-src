#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	rexecd
MAN=	rexecd.8
CFLAGS+= -DSKEY

DPADD=  ${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
LDADD=	-lskey -lmd -lcrypt -lutil

.include <bsd.prog.mk>
