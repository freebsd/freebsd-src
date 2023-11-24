
DEVDIR=	${SRCTOP}/sys/contrib/dev/athk

.PATH: ${DEVDIR}

KMOD=	athk_common

WITH_ATH_DEBUG=		1
WITH_ATH_TRACEPOINTS=	0

SRCS=	main.c
SRCS+=	dfs_pattern_detector.c dfs_pri_detector.c
SRCS+=	hw.c key.c regd.c

.if defined(WITH_ATH_DEBUG) && ${WITH_ATH_DEBUG} > 0
SRCS+=	debug.c
CFLAGS+=	-DCONFIG_ATH_DEBUG
.endif

.if defined(WITH_ATH_TRACEPOINTS) && ${WITH_ATH_TRACEPOINTS} > 0
SRCS+=	trace.c
CFLAGS+=	-DCONFIG_ATH_TRACEPOINTS
.endif

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-DKBUILD_MODNAME='"athk_common"'

CFLAGS+=	-I${DEVDIR}
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

.include <bsd.kmod.mk>
