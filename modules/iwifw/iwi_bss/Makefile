# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/dev/iwi

KMOD=	iwi_bss
FIRMWS=	iwi_bss:iwi_bss:300
FIRMWARE_LICENSE=	intel_iwi

CLEANFILES=	iwi_bss

iwi_bss: ${.CURDIR}/../../../contrib/dev/iwi/ipw2200-bss.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/iwi/ipw2200-bss.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

