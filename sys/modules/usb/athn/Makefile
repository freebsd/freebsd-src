# $FreeBSD$

INCDIR=${SRCTOP}/sys/compat/linuxkpi/common/include

.PATH: ${SRCTOP}/sys/dev/usb/wlan ${SRCTOP}/sys/dev/athn

.include <src.opts.mk>

KMOD    = if_athn

EXPORT_SYMS=    YES

SRCS=   if_athn_usb.c ar9285.c athn.c
SRCS+=	bus_if.h device_if.h
SRCS+=  opt_inet.h opt_inet6.h opt_route.h opt_netlink.h

SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+=        -ferror-limit=0
CFLAGS+=	${LINUXKPI_INCLUDES}
CFLAGS+=	-I${INCDIR}
CFLAGS+= 	-I${SRCTOP}/sys/dev/athn/headers
CFLAGS+=	-I${SRCTOP}/sys/compat/linuxkpi/dummy/include/openbsd
# CFLAGS+= 	-I${SRCTOP}/sys/dev/athn/include/amd64
# CFLAGS+= 	-I${SRCTOP}/sys/dev/athn/include/sys
# CFLAGS+= 	-I${SRCTOP}/sys/dev/athn/include/net80211


CWARNFLAGS+=	${NO_WUNUSED_BUT_SET_VARIABLE}
CWARNFLAGS+=	${NO_WUNUSED_PARAMETER}
CWARNFLAGS+=	${NO_WUNUSED_FUNCTION}

.include <bsd.kmod.mk>
