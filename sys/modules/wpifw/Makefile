# $FreeBSD: src/sys/modules/wpifw/Makefile,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/dev/wpi

KMOD=	wpifw
FIRMWS=	iwlwifi-3945-2.14.4.fw:wpifw:2144

CLEANFILES=	iwlwifi-3945-2.14.4.fw

iwlwifi-3945-2.14.4.fw: ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-2.14.4.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-2.14.4.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
