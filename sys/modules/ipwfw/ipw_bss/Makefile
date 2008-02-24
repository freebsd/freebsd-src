# $FreeBSD: src/sys/modules/ipwfw/ipw_bss/Makefile,v 1.1 2007/03/02 11:42:55 flz Exp $

.PATH: ${.CURDIR}/../../../contrib/dev/ipw

KMOD=	ipw_bss
FIRMWS=	ipw_bss:ipw_bss:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_bss

ipw_bss: ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

