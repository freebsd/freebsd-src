# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.11.2.1 2000/07/16 01:58:42 des Exp $

PROG= pkg_info
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
