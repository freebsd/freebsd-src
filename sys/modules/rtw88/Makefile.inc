# $FreeBSD$

# Common information shared by all submodule builds.

DEVRTW88DIR=	${SRCTOP}/sys/contrib/dev/rtw88

.PATH: ${DEVRTW88DIR}

WITH_CONFIG_PM=	0

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h

# Helpful after fresh imports.
CFLAGS+=	-ferror-limit=0

CFLAGS+=	-I${DEVRTW88DIR}
CFLAGS+=	-I${SRCTOP}/sys/compat/linuxkpi/common/include
CFLAGS+=	-DCONFIG_RTW88_DEBUG
#CFLAGS+=	-DCONFIG_RTW88_DEBUGFS

# end
