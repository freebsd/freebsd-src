# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
