.PATH:	${SRCTOP}/sys/compat/linuxkpi/common/src

KMOD=	linuxkpi_hdmi
SRCS=	linux_hdmi.c \
	linuxkpi_hdmikmod.c

SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+=		${LINUXKPI_INCLUDES}
CFLAGS.linux_hdmi.c=	-Wno-cast-qual

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
