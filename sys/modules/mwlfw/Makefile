# $FreeBSD: src/sys/modules/mwlfw/Makefile,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

KMOD=	mw88W8363fw
FIRMWS=	mw88W8363.fw:mw88W8363fw mwlboot.fw:mwlboot

CLEANFILES+= mw88W8363.fw mwlboot.fw

mw88W8363.fw: ${.CURDIR}/../../contrib/dev/mwl/mw88W8363.fw.uu
	uudecode -p $? > ${.TARGET}

mwlboot.fw: ${.CURDIR}/../../contrib/dev/mwl/mwlboot.fw.uu
	uudecode -p $? > ${.TARGET}

.include <bsd.kmod.mk>
