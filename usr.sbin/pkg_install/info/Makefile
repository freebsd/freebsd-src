#	$Id: Makefile,v 1.9.2.1 1998/10/28 12:00:33 jkh Exp $

PROG= pkg_info
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
