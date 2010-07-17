# $FreeBSD: src/sys/modules/ipwfw/ipw_monitor/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/dev/ipw

KMOD=	ipw_monitor
FIRMWS=	ipw_monitor:ipw_monitor:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_monitor

ipw_monitor: ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-p.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-p.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

