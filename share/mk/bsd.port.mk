# $FreeBSD: src/share/mk/bsd.port.mk,v 1.307.20.1 2008/10/02 02:57:24 kensmith Exp $

PORTSDIR?=	/usr/ports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

.include <bsd.own.mk>
.include "${BSDPORTMK}"
