DEVDIR=	${SRCTOP}/sys/contrib/dev/broadcom/brcm80211/brcmfmac

.PATH:	${DEVDIR}

# Should split this up into a core driver and 3 more
BRCMFMAC_PCI=		1
BRCMFMAC_USB=		0
BRCMFMAC_SDIO=		0

BRCMFMAC_PROTO_BCDC=	0 # SDIO, USB -- do not set manually
BRCMFMAC_DEBUG=		1
BRCMFMAC_TRACING=	0 # ftrace probes; simple enough to change possibly; keep 0!
BRCMFMAC_OF=		0
BRCMFMAC_DMI=		0
BRCMFMAC_ACPI=		0

KMOD=	if_brcmfmac

SRCS=	core.c
SRCS+=	btcoex.c cfg80211.c chip.c common.c feature.c firmware.c
SRCS+=	fweh.c fwil.c fwvid.c p2p.c pno.c proto.c vendor.c xtlv.c

SRCS+=	wcc/core.c cyw/core.c bca/core.c

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(BRCMFMAC_PCI) && ${BRCMFMAC_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	pcie.c
CFLAGS+=	-DCONFIG_BRCMFMAC_PCIE
SRCS+=	msgbuf.c commonring.c flowring.c
CFLAGS+=	-DCONFIG_BRCMFMAC_PROTO_MSGBUF
.endif

# SDIO parts; SDIO depends on an MMCCAM kernel.
.if defined(BRCMFMAC_SDIO) && ${BRCMFMAC_SDIO} > 0 && ${KERN_OPTS:MMMCCAM}
SRCS+=	sdio.c bcmsdh.c
SRCS+=	sdio_if.h
SRCS+=	sdiodevs.h
CFLAGS+=	-DCONFIG_BRCMFMAC_SDIO
BRCMFMAC_PROTO_BCDC=	1
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(BRCMFMAC_USB) && ${BRCMFMAC_USB} > 0
SRCS+=	usb.c
CFLAGS+=	-DCONFIG_BRCMFMAC_USB
BRCMFMAC_PROTO_BCDC=	1
.endif

.if defined(BRCMFMAC_PROTO_BCDC) && ${BRCMFMAC_PROTO_BCDC} > 0
CFLAGS+=	-DCONFIG_BRCMFMAC_PROTO_BCDC
SRCS+=	bcdc.c fwsignal.c
.endif

.if defined(BRCMFMAC_DEBUG) && ${BRCMFMAC_DEBUG} > 0
CFLAGS+=	-DDEBUG -DCONFIG_BRCMDBG
SRCS+=	debug.c
.endif

.if defined(BRCMFMAC_TRACING) && ${BRCMFMAC_TRACING} > 0
SRCS+=	tracepoint.c
CFLAGS+=	-DCONFIG_BRCM_TRACING
.endif

.if defined(BRCMFMAC_OF) && ${BRCMFMAC_OF} > 0
SRCS+=	of.c
CFLAGS+=	-DCONFIG_OF
.endif

.if defined(BRCMFMAC_DMI) && ${BRCMFMAC_DMI} > 0
SRCS+=	dmi.c
CFLAGS+=	-DCONFIG_DMI
.endif

.if defined(BRCMFMAC_ACPI) && ${BRCMFMAC_ACPI} > 0
SRCS+=	acpi.c
CFLAGS+=	-DCONFIG_ACPI
.endif

# Other
SRCS+=	bus_if.h device_if.h pci_if.h vnode_if.h
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-I${DEVDIR}
CFLAGS+=	-I${DEVDIR}/../include
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

#CFLAGS+=	-DCONFIG_PM_SLEEP=

#CFLAGS+=	-DCONFIG_PM
#CFLAGS+=	-DCONFIG_IPV6=0

CFLAGS+=	-DKBUILD_MODNAME='"brcmfmac"'
CFLAGS+=	-DLINUXKPI_VERSION=61900

.include <bsd.kmod.mk>
