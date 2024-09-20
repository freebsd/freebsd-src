# Common information shared by all submodule builds.

COMMONDIR=	${SRCTOP}/sys/contrib/dev/mediatek/mt76

.PATH: ${COMMONDIR}

WITH_CONFIG_PM=	0
WITH_DEBUGFS=	0

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h

# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

.if defined(WITH_CONFIG_PM) && ${WITH_CONFIG_PM} > 0
CFLAGS+=	-DCONFIG_PM=${WITH_CONFIG_PM}
.endif

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS=${WITH_DEBUGFS}
.endif

CFLAGS+=	-I${COMMONDIR}
CFLAGS+=	${LINUXKPI_INCLUDES}

# end
