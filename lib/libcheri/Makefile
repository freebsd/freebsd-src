# $FreeBSD$

LIB=	cheri
SRCS=	__cheri_enter.S			\
	cheri_invoke.S			\
	cheri_enter.c			\
	cheri_system.c			\
	libcheri_stat.c			\
	sandbox.c

INCS=	cheri_invoke.h			\
	cheri_system.h			\
	sandbox.h			\
	sandboxasm.h
INCSDIR=	${INCLUDEDIR}/cheri

SHLIB_MAJOR=	1

MAN=	cheri_invoke.3			\
	libcheri.3
USE_CHERI=	yes

FILES=	__cheri_enter.o cheri_invoke.o
CLEANFILES=	__cheri_enter.o cheri_invoke.o

.if !defined(CHERI_CC)
CFLAGS+=	-march=mips64r2
.endif

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.lib.mk>
