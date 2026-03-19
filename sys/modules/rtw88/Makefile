.include <kmod.opts.mk>

DEVRTW88DIR=	${SRCTOP}/sys/contrib/dev/rtw88

.PATH: ${DEVRTW88DIR}

# Bus attachments.
RTW88_PCI=		1
RTW88_SDIO=		0
RTW88_USB=		0

# Options.
RTW88_CONFIG_PM=	0
RTW88_DEBUGFS=		1
RTW88_LEDS=		0

KMOD=	if_rtw88

# Core parts.
SRCS=	main.c
SRCS+=	bf.c coex.c debug.c efuse.c fw.c mac.c mac80211.c
SRCS+=	phy.c ps.c regd.c
SRCS+=	rx.c sar.c sec.c tx.c util.c

# Common
SRCS+=	rtw8723x.c				# 87x3 common
SRCS+=	rtw8703b.c rtw8703b_tables.c		# 11n
SRCS+=	rtw8723d.c rtw8723d_table.c		# 11n
SRCS+=	rtw8814a.c rtw8814a_table.c		# 11ac
SRCS+=	rtw8821c.c rtw8821c_table.c		# 11ac
SRCS+=	rtw8822b.c rtw8822b_table.c		# 11ac
SRCS+=	rtw8822c.c rtw8822c_table.c		# 11ac

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(RTW88_PCI) && ${RTW88_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	pci.c
SRCS+=	rtw8723de.c
SRCS+=	rtw8814ae.c
SRCS+=	rtw8821ce.c
SRCS+=	rtw8822be.c
SRCS+=	rtw8822ce.c
.endif

# SDIO parts; SDIO depends on an MMCCAM kernel.
.if defined(RTW88_SDIO) && ${RTW88_SDIO} > 0 && ${KERN_OPTS:MMMCCAM}
SRCS+=	rtw8723cs.c
SRCS+=	rtw8723ds.c
SRCS+=	rtw8821cs.c
SRCS+=	rtw8822bs.c
SRCS+=	rtw8822cs.c
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(RTW88_USB) && ${RTW88_USB} > 0
SRCS+=	usb.c
SRCS+=	rtw8723du.c
SRCS+=	rtw88xxa.c					# 88xxa common
SRCS+=	rtw8812a.c rtw8812a_table.c rtw8812au.c
SRCS+=	rtw8814au.c
SRCS+=	rtw8821a.c rtw8821a_table.c rtw8821au.c
SRCS+=	rtw8821cu.c
SRCS+=	rtw8822bu.c
SRCS+=	rtw8822cu.c
CFLAGS+=	-DCONFIG_RTW88_USB
.endif

.if defined(RTW88_CONFIG_PM) && ${RTW88_CONFIG_PM} > 0
SRCS+=	wow.c
CFLAGS+=	-DCONFIG_PM=${RTW88_CONFIG_PM}
.endif

.if defined(RTW88_DEBUGFS) && ${RTW88_DEBUGFS} > 0
CFLAGS+=	-DCONFIG_RTW88_DEBUGFS
.endif

.if defined(RTW88_LEDS) && ${RTW88_LEDS} > 0
CFLAGS+=	-DCONFIG_RTW88_LEDS
SRCS+=		led.c
.endif

# Other
SRCS+=		opt_wlan.h opt_inet6.h opt_inet.h
SRCS+=		${LINUXKPI_GENSRCS}

CFLAGS+=	-I${DEVRTW88DIR}
CFLAGS+=	${LINUXKPI_INCLUDES}
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

CFLAGS+=	-DCONFIG_RTW88_DEBUG

CFLAGS+=	-DKBUILD_MODNAME='"rtw88"'
CFLAGS+=	-DLINUXKPI_VERSION=61900

.include <bsd.kmod.mk>
