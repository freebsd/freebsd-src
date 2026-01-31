.include <kmod.opts.mk>

DEVRTW89DIR=	${SRCTOP}/sys/contrib/dev/rtw89

.PATH: ${DEVRTW89DIR}

# Bus attachments.
WITH_PCI=	1
WITH_USB=	0
WITH_SDIO=	0

# Options.
WITH_CONFIG_PM=	0
WITH_DEBUGFS=	0
.if ${KERN_OPTS:MDEV_ACPI}
WITH_CONFIG_ACPI=	1
.endif

KMOD=	if_rtw89

# Core parts.
SRCS=	core.c
SRCS+=	chan.c mac80211.c mac.c mac_be.c phy.c phy_be.c fw.c
SRCS+=	cam.c efuse.c efuse_be.c regd.c sar.c coex.c ps.c ser.c
SRCS+=	util.c

# Common
SRCS+=	rtw8851b.c rtw8851b_rfk.c rtw8851b_rfk_table.c rtw8851b_table.c
SRCS+=	rtw8852a.c rtw8852a_rfk.c rtw8852a_rfk_table.c rtw8852a_table.c
SRCS+=	rtw8852b_common.c
SRCS+=	rtw8852b.c rtw8852b_rfk.c rtw8852b_rfk_table.c rtw8852b_table.c
SRCS+=	rtw8852bt.c rtw8852bt_rfk.c rtw8852bt_rfk_table.c
SRCS+=	rtw8852c.c rtw8852c_rfk.c rtw8852c_rfk_table.c rtw8852c_table.c
SRCS+=	rtw8922a.c rtw8922a_rfk.c

.if defined(WITH_CONFIG_ACPI) && ${WITH_CONFIG_ACPI} > 0
SRCS.DEV_ACPI+=	acpi.c
CFLAGS+=	-DCONFIG_ACPI
.endif
# This needs to always stay on for the LinuxKPI header file.
CFLAGS+=	-DLINUXKPI_WANT_LINUX_ACPI

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(WITH_PCI) && ${WITH_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	pci.c pci_be.c
SRCS+=	rtw8851be.c
SRCS+=	rtw8852ae.c
SRCS+=	rtw8852be.c
SRCS+=	rtw8852bte.c
SRCS+=	rtw8852ce.c
SRCS+=	rtw8922ae.c
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(WITH_USB) && ${WITH_USB} > 0
SRCS+=	usb.c
SRCS+=	rtw8851bu.c
SRCS+=	rtw8852au.c
SRCS+=	rtw8852bu.c
SRCS+=	rtw8852cu.c
.endif

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
CFLAGS+=	-DLINUXKPI_VERSION=61900

CFLAGS+=	-I${DEVRTW89DIR}
CFLAGS+=	${LINUXKPI_INCLUDES}
CFLAGS+=	-DCONFIG_RTW89_DEBUGMSG
.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
CFLAGS+=	-DCONFIG_RTW89_DEBUGFS
.endif

#CFLAGS+=	-ferror-limit=0 -fmacro-backtrace-limit=0

.include <bsd.kmod.mk>
