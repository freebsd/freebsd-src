# $FreeBSD: src/gnu/usr.bin/binutils/ranlib/Makefile,v 1.4.2.1 2000/07/06 22:16:11 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

PROG=		ranlib
SRCS=		ar.o is-ranlib.o
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
DPADD=		${RELTOP}/libbinutils/libbinutils.a
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a
LDADD=		${DPADD}

.include <bsd.prog.mk>
