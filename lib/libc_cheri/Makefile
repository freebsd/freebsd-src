# $FreeBSD$

LIB=		c_cheri
SRCS=		cheri_invoke.S

# libc/string
SRCS+=		memcpy.S		\
		memset.c

INCS=
SHLIB_MAJOR=	1
NO_MAN=		yes
USE_CHERI=	yes

FILES=		cheri_invoke.o
CLEANFILES=	cheri_invoke.o

.PATH: ${.CURDIR}/../libcheri
.PATH: ${.CURDIR}/../libcheri/${MACHINE_ARCH}
.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../libc/${MACHINE}/string
.PATH: ${.CURDIR}/../libc/string

CFLAGS+=	-Wno-cast-align

.include <bsd.lib.mk>
