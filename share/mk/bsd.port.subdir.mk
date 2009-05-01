# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31.28.1 2009/04/15 03:14:26 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
