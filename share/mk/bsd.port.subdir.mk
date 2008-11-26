# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31.24.1 2008/10/02 02:57:24 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
