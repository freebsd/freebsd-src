#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rexecd/Makefile,v 1.8.2.2 1999/08/29 15:03:41 peter Exp $

PROG=	rexecd
MAN8=	rexecd.8
CFLAGS+= -DSKEY

DPADD=  ${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
LDADD=	-lskey -lmd -lcrypt -lutil

.include <bsd.prog.mk>
