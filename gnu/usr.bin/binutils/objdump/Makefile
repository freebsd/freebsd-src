# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROG=	objdump
SRCS=	objdump.c prdbg.c
WARNS=	2
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
CFLAGS+= -DBFD_VERSION_STRING=\"${VERSION}\"
DPADD:=	${RELTOP}/libbinutils/libbinutils.a \
	${RELTOP}/libopcodes/libopcodes.a \
	${RELTOP}/libbfd/libbfd.a \
	${RELTOP}/libiberty/libiberty.a \
	${DPADD}
LDADD=	${DPADD}

.include <bsd.prog.mk>
