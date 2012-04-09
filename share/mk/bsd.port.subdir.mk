# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
