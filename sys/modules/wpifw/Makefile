# $FreeBSD: src/sys/modules/wpifw/Makefile,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/dev/wpi

KMOD=	wpifw
FIRMWS=	iwlwifi-3945-2.14.4.fw:wpifw:2144

CLEANFILES=	iwlwifi-3945-2.14.4.fw

iwlwifi-3945-2.14.4.fw: ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-2.14.4.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-2.14.4.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
