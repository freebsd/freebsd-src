#	$Id: Makefile,v 1.2 1997/12/16 17:43:15 bde Exp $

PROG= newkey
SRCS= newkey.c update.c generic.c

MAN8= newkey.8

# Later
#CFLAGS+= -DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
