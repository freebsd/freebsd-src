# $FreeBSD$

DEVRTW89DIR=	${SRCTOP}/sys/contrib/dev/rtw89

.PATH: ${DEVRTW89DIR}

WITH_CONFIG_PM=	0

KMOD=	if_rtw89

SRCS=	core.c
SRCS+=	pci.c
SRCS+=	chan.c mac80211.c mac.c phy.c fw.c
SRCS+=	rtw8852a.c rtw8852a_table.c
SRCS+=	rtw8852a_rfk.c rtw8852a_rfk_table.c
SRCS+=	rtw8852ae.c
SRCS+=	rtw8852c.c rtw8852c_table.c
SRCS+=	rtw8852c_rfk.c rtw8852c_rfk_table.c
SRCS+=	rtw8852ce.c
SRCS+=	cam.c efuse.c regd.c sar.c coex.c ps.c ser.c
SRCS+=	debug.c

.if defined(WITH_CONFIG_PM) && ${WITH_CONFIG_PM} > 0
CFLAGS+=	-DCONFIG_PM=${WITH_CONFIG_PM}
.endif

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h

CFLAGS+=	-DKBUILD_MODNAME='"rtw89"'

CFLAGS+=	-I${DEVRTW89DIR}
CFLAGS+=	-I${SRCTOP}/sys/compat/linuxkpi/common/include
CFLAGS+=	-DCONFIG_RTW89_DEBUGMSG
#CFLAGS+=	-DCONFIG_RTW89_DEBUGFS

#CFLAGS+=	-ferror-limit=0

.include <bsd.kmod.mk>
