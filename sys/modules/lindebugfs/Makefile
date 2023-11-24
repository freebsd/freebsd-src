
.PATH: ${SRCTOP}/sys/compat/lindebugfs

KMOD=	lindebugfs
SRCS=	lindebugfs.c
SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>
