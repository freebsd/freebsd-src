# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

# No a.out vs. ELF version so don't install in /usr/libexec/elf
BINDIR=/usr/bin

PROG=	readelf
SRCS=	${PROG}.c
WARNS=	2
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils -I${SRCDIR}/binutils

DPADD:=	${RELTOP}/libbinutils/libbinutils.a \
	${RELTOP}/libbfd/libbfd.a \
	${RELTOP}/libiberty/libiberty.a \
	${DPADD}
LDADD=	${DPADD}

.include <bsd.prog.mk>
