# $FreeBSD$

PROG=	ar
VERSION=	1.0.2
SRCS=	ar.c read.c util.c write.c
WARNS?=	5
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBELF}
LDADD=	-larchive -lbz2 -lz -lelf
CFLAGS+=	-DBSDAR_VERSION=\"${VERSION}\"
LINKS=	${BINDIR}/ar ${BINDIR}/ranlib

.include <bsd.prog.mk>
