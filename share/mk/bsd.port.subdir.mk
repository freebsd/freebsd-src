# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31.34.1 2010/12/21 17:10:29 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
