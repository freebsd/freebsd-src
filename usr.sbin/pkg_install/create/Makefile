#	$Id: Makefile,v 1.10 1998/03/07 08:54:54 bde Exp $

PROG= pkg_create

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c pl.c

.include <bsd.prog.mk>
