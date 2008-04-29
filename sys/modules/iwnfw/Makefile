# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/dev/iwn

KMOD=	iwnfw
FIRMWS=	iwlwifi-4965-4.44.17.fw:iwnfw:44417
FIRMWARE_LICENSE= intel_iwn

CLEANFILES=	iwlwifi-4965-4.44.17.fw

iwlwifi-4965-4.44.17.fw: ${.CURDIR}/../../contrib/dev/iwn/iwlwifi-4965-4.44.17.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/iwn/iwlwifi-4965-4.44.17.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
