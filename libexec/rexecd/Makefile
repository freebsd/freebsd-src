#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.6 1996/09/22 21:54:40 wosch Exp $

PROG=	rexecd
MAN8=	rexecd.8
CFLAGS+= -DSKEY

DPADD=  ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD=	-lskey -lmd -lcrypt

.include <bsd.prog.mk>
