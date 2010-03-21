# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31.32.1 2010/02/10 00:26:20 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
