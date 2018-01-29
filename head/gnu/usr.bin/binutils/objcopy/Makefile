# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROG=	objcopy
SRCS=	objcopy.c not-strip.c
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${.CURDIR}/${GNURELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils -I${SRCDIR}/bfd
DPADD=	${GNURELTOP}/libbinutils/libbinutils.a
DPADD+=	${GNURELTOP}/libbfd/libbfd.a
DPADD+=	${GNURELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
