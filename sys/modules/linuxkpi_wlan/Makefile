# $FreeBSD$
.PATH:	${SRCTOP}/sys/compat/linuxkpi/common/src

KMOD=	linuxkpi_wlan
SRCS=	linux_80211.c \
	linux_80211_macops.c

SRCS+=	opt_wlan.h
SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+= ${LINUXKPI_INCLUDES}

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
