DEVDIR=	${SRCTOP}/sys/contrib/dev/broadcom/brcm80211/brcmutil

.PATH:	${DEVDIR}

WITH_DEBUG=1

KMOD=	brcmutil

SRCS=	d11.c utils.c

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	bus_if.h device_if.h pci_if.h vnode_if.h
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h

CFLAGS+=	-DKBUILD_MODNAME='"brcmutil"'
CFLAGS+=	-DLINUXKPI_VERSION=61700

.if defined(WITH_DEBUG) && ${WITH_DEBUG} > 0
CFLAGS+=	-DDEBUG=${WITH_DEBUG}
.endif

CFLAGS+=	-I${DEVDIR}
CFLAGS+=	-I${DEVDIR}/../include
CFLAGS+=	${LINUXKPI_INCLUDES}
#CFLAGS+=	-ferror-limit=0

.include <bsd.kmod.mk>
