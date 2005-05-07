# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.20 2004/08/12 20:06:00 ru Exp $

PROG=	pkg_info
SRCS=	main.c perform.c show.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
