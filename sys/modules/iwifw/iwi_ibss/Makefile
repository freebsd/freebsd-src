# $FreeBSD: src/sys/modules/iwifw/iwi_ibss/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/dev/iwi

KMOD=	iwi_ibss
FIRMWS=	iwi_ibss:iwi_ibss:300
FIRMWARE_LICENSE=	intel_iwi

CLEANFILES=	iwi_fw

iwi_ibss: ${.CURDIR}/../../../contrib/dev/iwi/ipw2200-ibss.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/iwi/ipw2200-ibss.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

