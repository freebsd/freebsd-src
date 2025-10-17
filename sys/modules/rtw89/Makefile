DEVRTW89DIR=	${SRCTOP}/sys/contrib/dev/rtw89

.PATH: ${DEVRTW89DIR}

WITH_CONFIG_PM=	0
WITH_DEBUGFS=	0

KMOD=	if_rtw89

SRCS=	core.c
SRCS+=	pci.c pci_be.c
SRCS+=	chan.c mac80211.c mac.c mac_be.c phy.c phy_be.c fw.c
SRCS+=	acpi.c cam.c efuse.c efuse_be.c regd.c sar.c coex.c ps.c ser.c
SRCS+=	util.c
SRCS+=	rtw8852a.c rtw8852a_rfk.c rtw8852a_rfk_table.c rtw8852a_table.c
SRCS+=	rtw8852ae.c
SRCS+=	rtw8852c.c rtw8852c_rfk.c rtw8852c_rfk_table.c rtw8852c_table.c
SRCS+=	rtw8852ce.c
SRCS+=	rtw8851b.c rtw8851b_rfk.c rtw8851b_rfk_table.c rtw8851b_table.c
SRCS+=	rtw8851be.c
SRCS+=	rtw8852b_common.c
SRCS+=	rtw8852b.c rtw8852b_rfk.c rtw8852b_rfk_table.c rtw8852b_table.c
SRCS+=	rtw8852be.c
SRCS+=	rtw8852bt.c rtw8852bt_rfk.c rtw8852bt_rfk_table.c
SRCS+=	rtw8852bte.c
SRCS+=	rtw8922a.c rtw8922a_rfk.c
SRCS+=	rtw8922ae.c

# USB parts
#SRCS+=	rtw8851bu.c rtw8852bu.c
#SRCS+=	usb.c

# CONFIG_RTW89_DEBUG (always on for now)
SRCS+=	debug.c

.if defined(WITH_CONFIG_PM) && ${WITH_CONFIG_PM} > 0
CFLAGS+=	-DCONFIG_PM=${WITH_CONFIG_PM}
SRCS+=	wow.c
.endif

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-DKBUILD_MODNAME='"rtw89"'
CFLAGS+=	-DLINUXKPI_VERSION=61700
CFLAGS+=	-DLINUXKPI_WANT_LINUX_ACPI

CFLAGS+=	-I${DEVRTW89DIR}
CFLAGS+=	${LINUXKPI_INCLUDES}
CFLAGS+=	-DCONFIG_RTW89_DEBUGMSG
.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
CFLAGS+=	-DCONFIG_RTW89_DEBUGFS
.endif

#CFLAGS+=	-ferror-limit=0

.include <bsd.kmod.mk>
