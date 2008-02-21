# $FreeBSD$

PROG=	ar
VERSION=	1.0.2
SRCS=	ar.c read.c util.c write.c
WARNS?=	5
DPADD=	${LIBARCHIVE} ${LIBELF}
LDADD=	-larchive -lelf
CFLAGS+=	-DBSDAR_VERSION=\"${VERSION}\"
LINKS=	${BINDIR}/ar ${BINDIR}/ranlib

.include <bsd.prog.mk>
