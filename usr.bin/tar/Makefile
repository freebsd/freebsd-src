# $FreeBSD$

PROG=	bsdtar
VERSION=	1.00
SRCS=	bsdtar.c matching.c read.c util.c write.c
WARNS?=	6
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD=	-larchive -lbz2 -lz
CFLAGS+=	-DPACKAGE_VERSION=\"${VERSION}\"

.if !defined(WITH_GTAR)
SYMLINKS=	${BINDIR}/bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1
.endif

.include <bsd.prog.mk>

