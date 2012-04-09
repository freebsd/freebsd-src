# $FreeBSD: src/sys/modules/runfw/Makefile,v 1.2.2.3.6.1 2012/03/03 06:15:13 kensmith Exp $

KMOD=	runfw
FIRMWS=	runfw:runfw:1

CLEANFILES=	runfw

runfw: ${.CURDIR}/../../contrib/dev/run/rt2870.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/run/rt2870.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
