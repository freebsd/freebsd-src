# $FreeBSD$

.include <bsd.own.mk>

PROG=	ar
SRCS=	ar.c acplex.l acpyacc.y read.c util.c write.c y.tab.h

DPADD=	${LIBARCHIVE} ${LIBELF} ${LIBZ} ${LIBBZ2}
LDADD=	-larchive -lelf -lz -lbz2

NO_WMISSING_VARIABLE_DECLARATIONS=

CFLAGS+=-I. -I${.CURDIR}

.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?=	yes
.endif
LINKS=	${BINDIR}/ar ${BINDIR}/ranlib
MLINKS= ar.1 ranlib.1

.include <bsd.prog.mk>
