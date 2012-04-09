# $FreeBSD: src/gnu/usr.bin/binutils/ranlib/Makefile,v 1.19.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROGNAME= gnu-ranlib
MAN=	gnu-ranlib.1
gnu-ranlib.1: ranlib.1
	cat ${.ALLSRC} > ${.TARGET}
CLEANFILES+= gnu-ranlib.1

PROG=	ranlib
SRCS=	ar.c is-ranlib.c
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
CFLAGS+= -I${SRCDIR}/bfd
NO_SHARED?= yes
DPADD=	${RELTOP}/libbinutils/libbinutils.a
DPADD+=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
