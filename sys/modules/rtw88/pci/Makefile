# $FreeBSD$

KMOD=	if_rtw88_pci

SRCS+=	pci.c
SRCS+=	rtw8723d.c rtw8723d_table.c rtw8723de.c		# 11n
SRCS+=	rtw8821c.c rtw8821c_table.c rtw8821ce.c		# 11ac
SRCS+=	rtw8822b.c rtw8822b_table.c rtw8822be.c		# 11ac
SRCS+=	rtw8822c.c rtw8822c_table.c rtw8822ce.c		# 11ac

CFLAGS+=	-DKBUILD_MODNAME='"rtw88"'

.include <bsd.kmod.mk>
