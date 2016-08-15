# $FreeBSD$

LIB=	cheri_support
SRCS=	cheri_invoke_cabi.S \
	cheri_system_stub.c

NEED_CHERI=	sandbox

LIBCHERI_DIR=${.CURDIR}/../libcheri
.PATH: ${LIBCHERI_DIR}/${MACHINE_ARCH}

.include <bsd.lib.mk>
