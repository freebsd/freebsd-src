DEVATH10KDIR=	${SRCTOP}/sys/contrib/dev/athk/ath10k

.PATH: ${DEVATH10KDIR}

# Bus attachments.
ATH10K_PCI=		1
ATH10K_USB=		0	# Currently not working again, upstream kaputt.

ATH10K_CONFIG_FWLOG=	1
ATH10K_LEDS=		0

KMOD=	if_ath10k

SRCS+=	core.c debug.c mac.c ce.c hw.c bmi.c
SRCS+=	htc.c htt.c htt_rx.c htt_tx.c
SRCS+=	txrx.c wmi.c wmi-tlv.c
SRCS+=	p2p.c swap.c

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(ATH10K_PCI) && ${ATH10K_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	pci.c
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(ATH10K_USB) && ${ATH10K_USB} > 0
SRCS+=	usb.c
.endif

.if defined(ATH10K_CONFIG_FWLOG) && ${ATH10K_CONFIG_FWLOG} > 0
SRCS+=	fwlog.c
CFLAGS+=	-DCONFIG_FWLOG
.endif

.if defined(ATH10K_LEDS) && ${ATH10K_LEDS} > 0
CFLAGS+=	-DCONFIG_ATH10K_LEDS
SRCS+=		leds.c
.endif

# Always on for now.
CFLAGS+=	-DCONFIG_ATH10K_DEBUG
CFLAGS+=	-DLINUXKPI_WANT_LINUX_ACPI

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-I${DEVATH10KDIR}
CFLAGS+=	-I${DEVATH10KDIR}/..
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

# TODO: further options and SRCS files.
#CFLAGS+=	-DCONFIG_ATH10K_AHB
#CFLAGS+=	-DCONFIG_ATH10K_DEBUGFS
#CFLAGS+=	-DCONFIG_ATH10K_DFS_CERTIFIED
#CFLAGS+=	-DCONFIG_ATH10K_SPECTRAL
#CFLAGS+=	-DCONFIG_ATH10K_TRACING
#CFLAGS+=	-DCONFIG_DEV_COREDUMP
#CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS
#CFLAGS+=	-DCONFIG_MAC80211_MESH
#CFLAGS+=	-DCONFIG_NL80211_TESTMODE
#CFLAGS+=	-DCONFIG_PM
#CFLAGS+=	-DCONFIG_PM_SLEEP
#CFLAGS+=	-DCONFIG_THERMAL

CFLAGS+=	-DKBUILD_MODNAME='"ath10k"'
CFLAGS+=	-DLINUXKPI_VERSION=70000

.include <bsd.kmod.mk>
