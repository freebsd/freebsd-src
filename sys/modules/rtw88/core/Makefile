# $FreeBSD$

KMOD=	rtw88_core

SRCS=	main.c
SRCS+=	bf.c coex.c debug.c efuse.c fw.c mac.c mac80211.c
SRCS+=	phy.c ps.c regd.c
SRCS+=	rx.c sar.c sec.c tx.c util.c

.if defined(WITH_CONFIG_PM) && ${WITH_CONFIG_PM} > 0
SRCR+=	wow.c
CFLAGS+=	-DCONFIG_PM=${WITH_CONFIG_PM}
.endif

CFLAGS+=	-DKBUILD_MODNAME='"rtw88_core"'

.include <bsd.kmod.mk>
