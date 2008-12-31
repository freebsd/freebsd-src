# $FreeBSD: src/sys/modules/ipwfw/ipw_ibss/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/dev/ipw

KMOD=	ipw_ibss
FIRMWS=	ipw_ibss:ipw_ibss:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_ibss

ipw_ibss: ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-i.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-i.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

