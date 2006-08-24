# $FreeBSD$

PORTSDIR?=	/usr/ports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

# Needed to keep bsd.own.mk from reading in /etc/src.conf when building ports.
SRCCONF=	/dev/null

.include <bsd.own.mk>
.include "${BSDPORTMK}"
