# $FreeBSD: src/usr.sbin/pkg_install/add/Makefile,v 1.11 1999/08/28 01:17:54 peter Exp $

PROG= pkg_add

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c futil.c extract.c

.include <bsd.prog.mk>
