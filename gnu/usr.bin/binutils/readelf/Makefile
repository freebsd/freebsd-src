# $FreeBSD: src/gnu/usr.bin/binutils/readelf/Makefile,v 1.1.2.1 2000/11/25 10:38:21 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

# No a.out vs. ELF version so don't install in /usr/libexec/elf
BINDIR=/usr/bin

PROG=		readelf
NOMAN=		yes
SRCS=		${PROG}.c
CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils -I${SRCDIR}/binutils

DPADD=		${RELTOP}/libbinutils/libbinutils.a
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a
LDADD=		${DPADD}

.include <bsd.prog.mk>
