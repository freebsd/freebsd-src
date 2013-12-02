# $FreeBSD$

LIB=	casper

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib

SRCS=	libcasper.c
INCS=	libcasper.h

DPADD=	${LIBCAPSICUM} ${LIBNV} ${LIBPJDLOG}
LDADD=	-lcapsicum -lnv -lpjdlog

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../libpjdlog
CFLAGS+=-I${.CURDIR}/../../sbin/casper

WARNS?=	6

.include <bsd.lib.mk>
