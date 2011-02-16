# $FreeBSD: src/sys/modules/iwnfw/Makefile.inc,v 1.1.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.fw

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${KMOD}

${_FIRM}: ${.CURDIR}/../../../contrib/dev/iwn/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}
