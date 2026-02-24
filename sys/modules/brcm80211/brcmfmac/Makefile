DEVDIR=	${SRCTOP}/sys/contrib/dev/broadcom/brcm80211/brcmfmac

.PATH:	${DEVDIR}

# Should split this up into a core driver and 3 more
WITH_PCI=1
WITH_SDIO=0
WITH_USB=0

WITH_DMI=0
WITH_OF=0
WITH_DEBUG=1
WITH_TRACING=0	# ftrace probes; simple enough to change possibly; keep 0!

KMOD=	if_brcmfmac

SRCS=	core.c
SRCS+=	btcoex.c cfg80211.c chip.c common.c feature.c firmware.c
SRCS+=	fweh.c fwil.c fwvid.c p2p.c pno.c proto.c vendor.c xtlv.c

SRCS+=	wcc/core.c cyw/core.c bca/core.c

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(WITH_PCI) && ${WITH_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	pcie.c msgbuf.c commonring.c flowring.c
CFLAGS+=	-DCONFIG_BRCMFMAC_PCIE=1
.endif

# SDIO parts; SDIO depends on an MMCCAM kernel.
.if defined(WITH_SDIO) && ${WITH_SDIO} > 0 && ${KERN_OPTS:MMMCCAM}
SRCS+=	sdio.c bcmsdh.c
SRCS+=	sdio_if.h
SRCS+=	sdiodevs.h
CFLAGS+=	-DCONFIG_BRCMFMAC_SDIO=1
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(WITH_USB) && ${WITH_USB} > 0
SRCS+=	usb.c
CFLAGS+=	-DCONFIG_BRCMFMAC_USB=1
.endif

.if (defined(WITH_SDIO) && ${WITH_SDIO} > 0) || \
    (defined(WITH_USB) && ${WITH_USB} > 0)
CFLAGS+=	-DCONFIG_BRCMFMAC_PROTO_BCDC=1
SRCS+=	bcdc.c fwsignal.c
.endif

.if defined(WITH_DMI) && ${WITH_DMI} > 0
SRCS+=	dmi.c
.endif

.if defined(WITH_OF) && ${WITH_OF} > 0
SRCS+=	of.c
.endif

.if defined(WITH_DEBUG) && ${WITH_DEBUG} > 0
CFLAGS+=	-DDEBUG=${WITH_DEBUG}
SRCS+=	debug.c
.endif

.if defined(WITH_TRACING) && ${WITH_TRACING} > 0
SRCS+=	tracepoint.c
.endif

# Other
SRCS+=	bus_if.h device_if.h pci_if.h vnode_if.h
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-DKBUILD_MODNAME='"brcmfmac"'
CFLAGS+=	-DLINUXKPI_VERSION=61700

CFLAGS+=	-I${DEVDIR}
CFLAGS+=	-I${DEVDIR}/../include
CFLAGS+=	${LINUXKPI_INCLUDES}
CFLAGS+=	-ferror-limit=0

#CFLAGS+=	-DCONFIG_BRCM_TRACING=${WITH_TRACING}
CFLAGS+=	-DCONFIG_BRCMFMAC_PROTO_MSGBUF=${WITH_PCI}
CFLAGS+=	-DCONFIG_BRCMDBG=${WITH_DEBUG}

#CFLAGS+=	-DCONFIG_DMI=${WITH_DMI}
#CFLAGS+=	-DCONFIG_OF=${WITH_OF}

#CFLAGS+=	-DCONFIG_PM_SLEEP=

#CFLAGS+=	-DCONFIG_ACPI=0
#CFLAGS+=	-DCONFIG_PM
#CFLAGS+=	-DCONFIG_IPV6=0

.include <bsd.kmod.mk>
