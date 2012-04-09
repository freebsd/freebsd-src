# $FreeBSD: src/sys/modules/iwnfw/Makefile.inc,v 1.1.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.fw

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${KMOD}

${_FIRM}: ${.CURDIR}/../../../contrib/dev/iwn/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}
