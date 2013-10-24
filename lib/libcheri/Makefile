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

.include <bsd.lib.mk>
