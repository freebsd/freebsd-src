# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31 2004/07/02 20:47:18 eik Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
