# $FreeBSD$

PROG=	bsdtar
SRCS=	bsdtar.c matching.c read.c util.c write.c

WARNS?=	6
LDADD+=	-larchive -lbz2 -lz
DPADD+=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}

.include <bsd.prog.mk>

