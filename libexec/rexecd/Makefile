#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.5.4.1 1996/11/22 09:01:33 pst Exp $

PROG=	rexecd
MAN8=	rexecd.8
CFLAGS+= -DSKEY

DPADD=  ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD=	-lskey -lmd -lcrypt

.include <bsd.prog.mk>
