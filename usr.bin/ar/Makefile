# $FreeBSD$

PROG=	ar
SRCS=	ar.c acplex.l acpyacc.y read.c util.c write.c y.tab.h

WARNS?=	5

DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBELF}
LDADD=	-larchive -lbz2 -lz -lelf

# Do not depend on liblzma if we are building the bootstrap-tools and
# the local system doesn't include liblzma
.if !defined(BOOTSTRAPPING) || ${BOOTSTRAPPING} >= 800505
DPADD+=	${LIBLZMA}
LDADD+=	-llzma
.endif

CFLAGS+=-I. -I${.CURDIR}

NO_SHARED?=	yes
LINKS=	${BINDIR}/ar ${BINDIR}/ranlib
MLINKS= ar.1 ranlib.1

.include <bsd.prog.mk>
