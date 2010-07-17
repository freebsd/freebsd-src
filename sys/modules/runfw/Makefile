# $FreeBSD: src/sys/modules/runfw/Makefile,v 1.2.2.3.2.1 2010/06/14 02:09:06 kensmith Exp $

KMOD=	runfw
FIRMWS=	runfw:runfw:1

CLEANFILES=	runfw

runfw: ${.CURDIR}/../../contrib/dev/run/rt2870.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/run/rt2870.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
