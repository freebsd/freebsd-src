# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31.26.1 2008/11/25 02:59:29 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
