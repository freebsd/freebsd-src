# $FreeBSD$

LIB=	cheri
SRCS=	cheri_invoke.S			\
	libcheri_stat.c			\
	sandbox.c

INCS=	cheri_invoke.h			\
	sandbox.h			\
	sandboxasm.h
INCSDIR=	${INCLUDEDIR}

SHLIB_MAJOR=	1

MAN=	cheri_invoke.3			\
	libcheri.3
USE_CHERI=	yes

FILES=	cheri_invoke.o
CLEANFILES=	cheri_invoke.o

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.lib.mk>
