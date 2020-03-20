# $FreeBSD$

.PATH: ${SRCTOP}/sys/compat/lindebugfs

KMOD=	lindebugfs
SRCS=	lindebugfs.c
SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>
