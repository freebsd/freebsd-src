#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.4 1994/09/30 06:04:40 pst Exp $

PROG=	bdes
SRCS=	bdes.c

LDADD+= -L../../lib/libcipher -lcipher
DPADD+= ../../lib/libcipher/libcipher.a

.include <bsd.prog.mk>
