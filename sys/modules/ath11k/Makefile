# $FreeBSD$

DEVATH11KDIR=	${SRCTOP}/sys/contrib/dev/athk/ath11k

.PATH: ${DEVATH11KDIR}

WITH_DEBUGFS=	0		# Does not yet compile

KMOD=	if_ath11k

SRCS+=	core.c hal.c hal_tx.c hal_rx.c
SRCS+=	wmi.c mac.c reg.c htc.c qmi.c
SRCS+=	dp.c dp_tx.c dp_rx.c debug.c
SRCS+=	ce.c peer.c dbring.c hw.c wow.c

SRCS+=	mhi.c pci.c

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-DKBUILD_MODNAME='"ath11k"'

CFLAGS+=	-I${DEVATH11KDIR}
CFLAGS+=	-I${DEVATH11KDIR}/..
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

CFLAGS+=	-DCONFIG_ATH11K_DEBUG

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=		debugfs.c debugfs_htt_stats.c debugfs_sta.c
CFLAGS+=	-DCONFIG_ATH11K_DEBUGFS=${WITH_DEBUGFS}
CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS=${WITH_DEBUGFS}
.endif

#CFLAGS+=	-DCONFIG_ATH11K_SPECTRAL
#CFLAGS+=	-DCONFIG_ATH11K_TRACING
#CFLAGS+=	-DCONFIG_NL80211_TESTMODE
#CFLAGS+=	-DCONFIG_PM
#CFLAGS+=	-DCONFIG_THERMAL

.include <bsd.kmod.mk>
