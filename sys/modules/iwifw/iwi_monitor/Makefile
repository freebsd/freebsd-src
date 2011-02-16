# $FreeBSD: src/sys/modules/iwifw/iwi_monitor/Makefile,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/dev/iwi

KMOD=	iwi_monitor
FIRMWS=	iwi_monitor:iwi_monitor:300
FIRMWARE_LICENSE=	intel_iwi

CLEANFILES=	iwi_monitor

iwi_monitor: ${.CURDIR}/../../../contrib/dev/iwi/ipw2200-sniffer.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/iwi/ipw2200-sniffer.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

