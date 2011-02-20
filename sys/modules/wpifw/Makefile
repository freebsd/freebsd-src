# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/dev/wpi

KMOD=	wpifw
FIRMWS=	iwlwifi-3945-15.32.2.9.fw:wpifw:153229

CLEANFILES=	iwlwifi-3945-15.32.2.9.fw

iwlwifi-3945-15.32.2.9.fw: ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-15.32.2.9.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-15.32.2.9.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
