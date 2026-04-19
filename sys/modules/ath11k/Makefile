DEVATH11KDIR=	${SRCTOP}/sys/contrib/dev/athk/ath11k

.PATH: ${DEVATH11KDIR}

ATH11K_PCI=		1
ATH11K_AHB=		0

ATH11K_CFR=		0
ATH11K_DEBUGFS=		0
ATH11K_TRACE=		0
ATH11K_THERMAL=		0
ATH11K_SPECTRAL=	0
ATH11K_PM=		0
ATH11K_DEV_COREDUMP=	0

KMOD=	if_ath11k

SRCS+=	core.c hal.c hal_tx.c hal_rx.c
SRCS+=	wmi.c mac.c reg.c htc.c qmi.c
SRCS+=	dp.c dp_tx.c dp_rx.c debug.c
SRCS+=	ce.c peer.c dbring.c hw.c pcic.c
SRCS+=	fw.c p2p.c

# PCI
.if defined(ATH11K_PCI) && ${ATH11K_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	mhi.c pci.c
CFLAGS+=	-DCONFIG_ATH11K_PCI
.endif

.if defined(ATH11K_AHB) && ${ATH11K_AHB} > 0
SRCS+=	ahb.c
CFLAGS+=	-DCONFIG_ATH11K_AHB
.endif

.if defined(ATH11K_CFR) && ${ATH11K_CFR} > 0
SRCS+=	cfr.c
CFLAGS+=	-DCONFIG_ATH11K_CFR
.endif

.if defined(ATH11K_DEBUGFS) && ${ATH11K_DEBUGFS} > 0
SRCS+=	debugfs.c debugfs_htt_stats.c debugfs_sta.c
CFLAGS+=	-DCONFIG_ATH11K_DEBUGFS
CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS
.endif

.if defined(ATH11K_TRACE) && ${ATH11K_TRACE} > 0
SRCS+=	trace.c
CFLAGS+=	-DCONFIG_ATH11K_TRACING
.endif

.if defined(ATH11K_THERMAL) && ${ATH11K_THERMAL} > 0
SRCS+=	thermal.c
CFLAGS+=	-DCONFIG_ATH11K_THERMAL
.endif

.if defined(ATH11K_SPECTRAL) && ${ATH11K_SPECTRAL} > 0
SRCS+=	spectral.c
CFLAGS+=	-DCONFIG_ATH11K_SPECTRAL
.endif

.if defined(ATH11K_PM) && ${ATH11K_PM} > 0
CFLAGS+=	-DCONFIG_PM
SRCS+=	wow.c
.endif

.if defined(ATH11K_DEV_COREDUMP) && ${ATH11K_DEV_COREDUMP} > 0
CFLAGS+=	-DCONFIG_DEV_COREDUMP
SRCS+=	coredump.c
.endif

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-I${DEVATH11KDIR}
CFLAGS+=	-I${DEVATH11KDIR}/..
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

CFLAGS+=	-DCONFIG_ATH11K_DEBUG

CFLAGS+=	-DKBUILD_MODNAME='"ath11k"'
CFLAGS+=	-DLINUXKPI_VERSION=70000

.include <bsd.kmod.mk>
