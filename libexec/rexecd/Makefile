#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.8 1997/02/22 14:22:05 peter Exp $

PROG=	rexecd
MAN8=	rexecd.8
CFLAGS+= -DSKEY

DPADD=  ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD=	-lskey -lmd -lcrypt

.include <bsd.prog.mk>
