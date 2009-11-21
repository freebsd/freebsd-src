# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
