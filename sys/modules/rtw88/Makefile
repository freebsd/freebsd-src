
DEVRTW88DIR=	${SRCTOP}/sys/contrib/dev/rtw88

.PATH: ${DEVRTW88DIR}

WITH_CONFIG_PM=	0

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
SRCS+=	rtw8723d.c rtw8723d_table.c rtw8723de.c		# 11n
SRCS+=	rtw8821c.c rtw8821c_table.c rtw8821ce.c		# 11ac
SRCS+=	rtw8822b.c rtw8822b_table.c rtw8822be.c		# 11ac
SRCS+=	rtw8822c.c rtw8822c_table.c rtw8822ce.c		# 11ac

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h

# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

CFLAGS+=	-DKBUILD_MODNAME='"rtw88"'

CFLAGS+=	-I${DEVRTW88DIR}
CFLAGS+=	${LINUXKPI_INCLUDES}
CFLAGS+=	-DCONFIG_RTW88_DEBUG
#CFLAGS+=	-DCONFIG_RTW88_DEBUGFS

.include <bsd.kmod.mk>
