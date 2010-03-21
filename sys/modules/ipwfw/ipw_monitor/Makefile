# $FreeBSD: src/sys/modules/ipwfw/ipw_monitor/Makefile,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/dev/ipw

KMOD=	ipw_monitor
FIRMWS=	ipw_monitor:ipw_monitor:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_monitor

ipw_monitor: ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-p.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-p.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

