# $FreeBSD$

LIB=	cheri
SRCS=	libcheri_stat.c			\
	sandbox.c			\
	sandboxasm.S

INCS=	cheri_invoke.h			\
	sandbox.h			\
	sandboxasm.h
INCSDIR=	${INCLUDEDIR}

SHLIB_MAJOR=	1

MAN=	libcheri.3
USE_CHERI=	yes

FILES=	sandboxasm.o
CLEANFILES=	sandboxasm.o

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.lib.mk>
