# $FreeBSD$

DEVATH12KDIR=	${SRCTOP}/sys/contrib/dev/athk/ath12k

.PATH: ${DEVATH12KDIR}

WITH_CONFIG_ATH12K_TRACING=	0

KMOD=	if_ath12k

SRCS+=	core.c hal.c hal_tx.c hal_rx.c
SRCS+=	wmi.c mac.c reg.c htc.c qmi.c
SRCS+=	dp.c dp_tx.c dp_rx.c dp_mon.c debug.c
SRCS+=	ce.c peer.c dbring.c hw.c

SRCS+=	mhi.c pci.c

.if defined(WITH_CONFIG_ATH12K_TRACING) && ${WITH_CONFIG_ATH12K_TRACING} > 0
CFLAGS+=	-DCONFIG_ATH12K_TRACING=${WITH_CONFIG_ATH12K_TRACING}
SRCS+=	trace.c
.endif

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-DKBUILD_MODNAME='"ath12k"'

CFLAGS+=	-I${DEVATH12KDIR}
CFLAGS+=	-I${DEVATH12KDIR}/..
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

CFLAGS+=	-DCONFIG_ATH12K_DEBUG

.include <bsd.kmod.mk>
