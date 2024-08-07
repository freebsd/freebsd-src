.PATH:	${SRCTOP}/sys/compat/linuxkpi/common/src

KMOD=	linuxkpi_video
SRCS=	linux_hdmi.c \
	linux_aperture.c \
	linux_cmdline.c \
	linuxkpi_videokmod.c

SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+=		${LINUXKPI_INCLUDES}
CFLAGS.linux_hdmi.c=	-Wno-cast-qual

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
