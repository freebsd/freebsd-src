# $FreeBSD: src/sys/modules/ipwfw/ipw_ibss/Makefile,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/dev/ipw

KMOD=	ipw_ibss
FIRMWS=	ipw_ibss:ipw_ibss:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_ibss

ipw_ibss: ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-i.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-i.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

