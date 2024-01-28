.include <bsd.init.mk>

LIB=	kboot
WARNS?= 4

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}

SRCS=	crt1.c

.include <bsd.lib.mk>
