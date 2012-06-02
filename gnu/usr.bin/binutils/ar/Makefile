# $FreeBSD$

.include "../Makefile.inc0"
.include <bsd.own.mk>

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROGNAME= gnu-ar
MAN=   gnu-ar.1
gnu-ar.1: ar.1
	cat ${.ALLSRC} > ${.TARGET}
CLEANFILES+= gnu-ar.1

PROG=	ar
SRCS=	ar.c not-ranlib.c
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
CFLAGS+= -I${SRCDIR}/bfd
.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?= yes
.endif
DPADD=	${RELTOP}/libbinutils/libbinutils.a
DPADD+=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
