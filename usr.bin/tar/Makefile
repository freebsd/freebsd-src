# $FreeBSD$

PROG=	bsdtar
SRCS=	bsdtar.c matching.c read.c util.c write.c
WARNS?=	6
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD=	-larchive -lbz2 -lz

.include <bsd.prog.mk>

