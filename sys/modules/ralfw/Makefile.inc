# $FreeBSD: src/sys/modules/ralfw/Makefile.inc,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.
#
KMOD=	${IMG}fw
_FIRM=	${IMG}.fw

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${KMOD}

${_FIRM}: ${.CURDIR}/../../../contrib/dev/ral/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}
