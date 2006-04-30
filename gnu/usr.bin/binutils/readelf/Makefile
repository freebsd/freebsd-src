# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

# No a.out vs. ELF version so don't install in /usr/libexec/elf
BINDIR=/usr/bin

PROG=	readelf
SRCS=	${PROG}.c
WARNS?=	0
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils -I${SRCDIR}/binutils

DPADD=	${RELTOP}/libbinutils/libbinutils.a
DPADD+=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
