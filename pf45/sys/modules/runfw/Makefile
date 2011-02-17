# $FreeBSD$

KMOD=	runfw
FIRMWS=	runfw:runfw:1

CLEANFILES=	runfw

runfw: ${.CURDIR}/../../contrib/dev/run/rt2870.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/run/rt2870.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
