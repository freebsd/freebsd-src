.include <kmod.opts.mk>

DEVATH12KDIR=	${SRCTOP}/sys/contrib/dev/athk/ath12k

.PATH: ${DEVATH12KDIR}

ATH12K_PCI=		1
ATH12K_AHB=		0

ATH12K_ACPI=		0
ATH12K_COREDUMP=	0
ATH12K_DEBUGFS=		0
ATH12K_PM=		0
ATH12K_TRACING=		0

KMOD=	if_ath12k

SRCS+=	core.c hal.c
SRCS+=	wmi.c mac.c reg.c htc.c qmi.c
SRCS+=	dp.c dp_tx.c dp_rx.c dp_mon.c debug.c
SRCS+=	ce.c peer.c dbring.c fw.c p2p.c
SRCS+=	dp_htt.c dp_peer.c

SRCS+=	wifi7/ce.c wifi7/core.c
SRCS+=	wifi7/dp.c wifi7/dp_mon.c wifi7/dp_rx.c wifi7/dp_tx.c
SRCS+=	wifif/hal.c wifi7/hal_tx.c wifi7/hal_rx.c
SRCS+=	wifi7/hal_qcc2072.c wifi7/hal_qcn9274.c wifi7/hal_wcn7850.c
SRCS+=	wifi7/hw.c wifi7/wmi.c

.if defined(ATH12K_PCI) && ${ATH12K_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	mhi.c pci.c
SRCS+=	wifi7/mhi.c wifi7/pci.c
.else
.error	ath12k requires PCI support compiled in at this point
.endif

.if defined(ATH12K_AHB) && ${ATH12K_AHB} > 0
CFLAGS+=	-DCONFIG_ATH12K_AHB
SRCS+=	ahb.c
SRCS+=	wifi7/ahb.c
.endif
.if defined(ATH12K_DEBUGFS) && ${ATH12K_DEBUGFS} > 0
CFLAGS+=	-DCONFIG_ATH12K_DEBUGFS
SRCS+=	debugfs.c debugfs_htt_stats.c debugfs_sta.c
.endif
.if defined(ATH12K_ACPI) && ${ATH12K_ACPI} > 0
CFLAGS+=	-DCONFIG_ATH12K_ACPI
SRCS+=	acpi.c
.endif
.if defined(ATH12K_TRACING) && ${ATH12K_TRACING} > 0
CFLAGS+=	-DCONFIG_ATH12K_TRACING
SRCS+=	trace.c
.endif
.if defined(ATH12K_PM) && ${ATH12K_PM} > 0
CFLAGS+=	-DCONFIG_PM
SRCS+=	wow.c
.endif
.if defined(ATH12K_COREDUMP) && ${ATH12K_COREDUMP} > 0
CFLAGS+=	-DCONFIG_ATH12K_COREDUMP
SRCS+=	coredump.c
.endif

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-I${DEVATH12KDIR}
CFLAGS+=	-I${DEVATH12KDIR}/..
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

CFLAGS+=	-DCONFIG_ATH12K_DEBUG

CFLAGS+=	-DKBUILD_MODNAME='"ath12k"'
CFLAGS+=	-DLINUXKPI_VERSION=70000

.include <bsd.kmod.mk>
