#	$Id: Makefile,v 1.1.1.1 1997/05/28 15:52:07 wpaul Exp $

PROG= newkey
SRCS= newkey.c update.c generic.c

MAN8= newkey.8

# Later
#CFLAGS+= -DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
