# Common information shared by all submodule builds.

COMMONDIR=	${SRCTOP}/sys/contrib/dev/mediatek/mt76
DEVDIR=		${SRCTOP}/sys/contrib/dev/mediatek/mt76/${MT76_DRIVER_NAME}

KMOD?=		if_${MT76_DRIVER_NAME}

# Bus attachment for the core part and that each driver can check
# what the common code supports.
MT76_PCI=		1
MT76_SDIO=		0
MT76_USB=		0

# General options for common code so drivers can check.
MT76_ACPI?=		0
MT76_CONFIG_PM?=	0
MT76_DEBUGFS?=		0
MT76_SOC_WED?=		0

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h

# This is true for all architectures we build for.
CFLAGS+=	-DCONFIG_ARCH_DMA_ADDR_T_64BIT

.if defined(MT76_CONFIG_PM) && ${MT76_CONFIG_PM} > 0
CFLAGS+=	-DCONFIG_PM
.endif

.if defined(MT76_DEBUGFS) && ${MT76_DEBUGFS} > 0
CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS
.endif

.if defined(MT76_SOC_WED) && ${MT76_SOC_WED} > 0
CFLAGS+=	-DCONFIG_NET_MEDIATEK_SOC_WED
.endif

CFLAGS+=	-I${COMMONDIR}
.if ${MT76_DRIVER_NAME} != "mt76_core"
CFLAGS+=	-I${DEVDIR}
.endif
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

CFLAGS+=        -DKBUILD_MODNAME='"${MT76_DRIVER_NAME}"'
CFLAGS+=	-DLINUXKPI_VERSION=61900

