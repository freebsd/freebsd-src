# $FreeBSD$

PACKAGE=	lib${LIB}
LIB=		ifconfig
INTERNALLIB=	true

SHLIBDIR?=	/lib
SHLIB_MAJOR=	1
SRCS=		libifconfig.c \
		libifconfig_bridge.c \
		libifconfig_carp.c \
		libifconfig_inet.c \
		libifconfig_inet6.c \
		libifconfig_internal.c \
		libifconfig_lagg.c \
		libifconfig_media.c

# If libifconfig become public uncomment those two lines
#INCSDIR=	${INCLUDEDIR}
#INCS=		libifconfig.h

#MAN=		libifconfig.3

CFLAGS+= -I${.CURDIR}
NO_WCAST_ALIGN= yes

.include <bsd.lib.mk>
