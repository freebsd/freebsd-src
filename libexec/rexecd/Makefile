#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.9 1999/04/06 23:40:25 brian Exp $

PROG=	rexecd
MAN8=	rexecd.8
CFLAGS+= -DSKEY

DPADD=  ${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
LDADD=	-lskey -lmd -lcrypt -lutil

.include <bsd.prog.mk>
