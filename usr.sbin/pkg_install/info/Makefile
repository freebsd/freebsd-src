#	$Id: Makefile,v 1.10 1998/03/07 08:55:00 bde Exp $

PROG= pkg_info
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
