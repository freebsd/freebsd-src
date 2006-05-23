# $FreeBSD: src/share/mk/bsd.port.mk,v 1.307 2004/07/02 20:47:18 eik Exp $

PORTSDIR?=	/usr/ports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

.include <bsd.own.mk>
.include "${BSDPORTMK}"
