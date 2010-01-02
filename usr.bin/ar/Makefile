# $FreeBSD$

PROG=	ar
SRCS=	ar.c acplex.l acpyacc.y read.c util.c write.c y.tab.h

DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBELF}
LDADD=	-larchive -lbz2 -lz -lelf

CFLAGS+=-I. -I${.CURDIR}

NO_SHARED?=	yes
LINKS=	${BINDIR}/ar ${BINDIR}/ranlib
MLINKS= ar.1 ranlib.1

.include <bsd.prog.mk>
