# $FreeBSD$

LIB=	cheri
SRCS=	sandbox.c			\
	sandboxasm.S

INCS=	sandbox.h
INCSDIR=	${INCLUDEDIR}

SHLIB_MAJOR=	1

NO_MAN=yes

FILES=	sandboxasm.o
CLEANFILES=	sandboxasm.o

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.if defined(CHERI_CC) && defined(SYSROOT)
CC=	${CHERI_CC} --sysroot=${SYSROOT} -integrated-as
.endif

.include <bsd.lib.mk>
