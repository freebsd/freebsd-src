# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.11.2.6 2004/12/13 13:50:01 marius Exp $

PROG=	pkg_info
SRCS=	main.c perform.c show.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DPADD+=	${LIBSSL} ${LIBCRYPTO}
LDADD+=	-lssl -lcrypto
.endif

.include <bsd.prog.mk>
