#	$Id: Makefile,v 1.10 1998/03/07 08:54:51 bde Exp $

PROG= pkg_add

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c futil.c extract.c

.include <bsd.prog.mk>
