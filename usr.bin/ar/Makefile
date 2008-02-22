# $FreeBSD$

.if defined(WITH_BSDAR)
PROG=	ar
.else
PROG=	bsdar
.endif
SRCS=	ar.c read.c util.c write.c

WARNS?=	5

DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBELF}
LDADD=	-larchive -lbz2 -lz -lelf

.if defined(WITH_BSDAR)
LINKS=	${BINDIR}/ar ${BINDIR}/ranlib
MLINKS=	ar ranlib
.else
LINKS=	${BINDIR}/bsdar ${BINDIR}/bsdranlib
MLINKS=	bsdar.1 bsdranlib.1

bsdar.1: ar.1
	ln -sf ${.ALLSRC} ${.TARGET}
.endif

.include <bsd.prog.mk>
