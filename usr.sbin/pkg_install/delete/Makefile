#	$Id: Makefile,v 1.10 1998/03/07 08:54:57 bde Exp $

PROG= pkg_delete
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c

.include <bsd.prog.mk>
