# $FreeBSD$

PORTSDIR?=	/usr/ports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

.include <bsd.own.mk>
.include "${BSDPORTMK}"
