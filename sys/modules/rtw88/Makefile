DEVRTW88DIR=	${SRCTOP}/sys/contrib/dev/rtw88

.PATH: ${DEVRTW88DIR}

WITH_CONFIG_PM=	0
WITH_DEBUGFS=	1
WITH_LEDS=	0

KMOD=	if_rtw88

# Core parts.
SRCS=	main.c
SRCS+=	bf.c coex.c debug.c efuse.c fw.c mac.c mac80211.c
SRCS+=	phy.c ps.c regd.c
SRCS+=	rx.c sar.c sec.c tx.c util.c

.if defined(WITH_CONFIG_PM) && ${WITH_CONFIG_PM} > 0
SRCS+=	wow.c
CFLAGS+=	-DCONFIG_PM=${WITH_CONFIG_PM}
.endif

# PCI parts.
SRCS+=	pci.c
SRCS+=	rtw8723x.c					# 87x3 common
SRCS+=	rtw8703b.c rtw8703b_tables.c			# 11n
SRCS+=	rtw8723d.c rtw8723d_table.c rtw8723de.c		# 11n
SRCS+=	rtw8821c.c rtw8821c_table.c rtw8821ce.c		# 11ac
SRCS+=	rtw8822b.c rtw8822b_table.c rtw8822be.c		# 11ac
SRCS+=	rtw8822c.c rtw8822c_table.c rtw8822ce.c		# 11ac
SRCS+=	rtw8814a.c rtw8814a_table.c rtw8814ae.c		# 11ac

# USB parts
#SRCS+=	rtw88xxa.c					# 88xxa common
#SRCS+=	rtw8812a.c rtw8812a_table.c rtw8812au.c
#SRCS+=	rtw8814au.c
#SRCS+=	rtw8821a.c rtw8821a_table.c rtw8821au.c
#CFLAGS+=	-DCONFIG_RTW88_USB

.if defined(WITH_LEDS) && ${WITH_LEDS} > 0
CFLAGS+=	-DCONFIG_RTW88_LEDS
SRCS+=		led.c
.endif

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h

CFLAGS+=	-DKBUILD_MODNAME='"rtw88"'
CFLAGS+=	-DLINUXKPI_VERSION=61700

CFLAGS+=	-I${DEVRTW88DIR}
CFLAGS+=	${LINUXKPI_INCLUDES}
CFLAGS+=	-DCONFIG_RTW88_DEBUG
.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
CFLAGS+=	-DCONFIG_RTW88_DEBUGFS
.endif

# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

.include <bsd.kmod.mk>
