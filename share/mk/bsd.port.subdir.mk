# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.28.2.3 2004/07/05 23:10:25 eik Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
