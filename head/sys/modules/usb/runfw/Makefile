# $FreeBSD$

KMOD=	runfw
FIRMWS=	run.fw:runfw:1

CLEANFILES=	run.fw

run.fw: ${.CURDIR}/../../../contrib/dev/run/rt2870.fw.uu
	uudecode -p $? > ${.TARGET}

.include <bsd.kmod.mk>
