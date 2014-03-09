# $FreeBSD$

.PATH:	${.CURDIR} ${.CURDIR}/../../../sbin/casper

PROG=	random

SRCS=	random.c

DPADD=	${LIBCAPSICUM} ${LIBCASPER} ${LIBNV} ${LIBPJDLOG} ${LIBUTIL}
LDADD=	-lcapsicum -lcasper -lnv -lpjdlog -lutil

BINDIR=	/libexec/casper

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../../../lib/libcapsicum
CFLAGS+=-I${.CURDIR}/../../../lib/libcasper
CFLAGS+=-I${.CURDIR}/../../../lib/libpjdlog
CFLAGS+=-I${.CURDIR}/../../../sbin/casper

NO_MAN=

.include <bsd.prog.mk>
