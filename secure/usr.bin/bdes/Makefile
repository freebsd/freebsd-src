#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.2 1994/09/07 07:16:52 pst Exp $

PROG=	bdes
SRCS=	bdes.c

LDADD+= -lcrypt -lcipher
DPADD+= /usr/lib/libcrypt.a /usr/lib/libcipher.a

.include <bsd.prog.mk>
