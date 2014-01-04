# $FreeBSD$

LIB=	cheri
SRCS=	__cheri_enter.S			\
	cheri_invoke.S			\
	cheri_enter.c			\
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

FILES=	__cheri_enter.o cheri_invoke.o
CLEANFILES=	__cheri_enter.o cheri_invoke.o

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.lib.mk>
