# $FreeBSD: src/share/mk/bsd.port.mk,v 1.303.2.3 2004/07/05 23:10:25 eik Exp $

PORTSDIR?=	/usr/ports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

.include <bsd.own.mk>
.include "${BSDPORTMK}"
