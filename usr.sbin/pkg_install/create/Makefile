#	$Id: Makefile,v 1.9.2.1 1998/10/28 12:00:32 jkh Exp $

PROG= pkg_create

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c pl.c

.include <bsd.prog.mk>
