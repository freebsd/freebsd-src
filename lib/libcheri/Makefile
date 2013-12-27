# $FreeBSD$

LIB=	cheri
SRCS=	sandbox.c			\
	sandbox_stat.c			\
	sandboxasm.S

INCS=	sandbox.h			\
	sandbox_stat.h			\
	sandboxasm.h
INCSDIR=	${INCLUDEDIR}

SHLIB_MAJOR=	1

NO_MAN=yes
USE_CHERI=	yes

FILES=	sandboxasm.o
CLEANFILES=	sandboxasm.o

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.lib.mk>
