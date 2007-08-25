# $FreeBSD$

PROG=	bsdtar
VERSION=	1.9.3
SRCS=	bsdtar.c getdate.y matching.c read.c tree.c util.c write.c
WARNS?=	5
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD=	-larchive -lbz2 -lz
CFLAGS+=	-DPACKAGE_VERSION=\"${VERSION}\"
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+=	-I${.CURDIR}
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1

.include <bsd.prog.mk>
