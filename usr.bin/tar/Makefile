# $FreeBSD$

PROG=	bsdtar
SRCS=	bsdtar.c matching.c read.c util.c write.c
WARNS?=	6
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD=	-larchive -lbz2 -lz

.if defined(WITH_BSDTAR)
LINKS=	${BINDIR}/bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1
.endif

.include <bsd.prog.mk>

