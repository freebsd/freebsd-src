# $FreeBSD$

LIB=	cheri
SRCS=	cheri_invoke.S			\
	cheri_enter.c			\
	cheri_system.c			\
	libcheri_classes.S		\
	libcheri_stat.c			\
	sandbox.c			\
	sandbox_loader.c

INCS=	cheri_class.h			\
	cheri_enter.h			\
	cheri_invoke.h			\
	cheri_memcpy.h			\
	cheri_system.h			\
	sandbox.h			\
	sandboxasm.h

CFLAGS+=	-I${.CURDIR}/${MACHINE_ARCH}

INCSDIR=	${INCLUDEDIR}/cheri

SHLIB_MAJOR=	1

MAN=	cheri_invoke.3			\
	libcheri.3

USE_CHERI=	yes

CLEANFILES=	cheri_classes.o cheri_invoke.o

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.lib.mk>
