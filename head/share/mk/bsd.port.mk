# $FreeBSD$

PORTSDIR?=	/usr/ports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

# Needed to keep bsd.own.mk from reading in /etc/src.conf
# and setting MK_* variables when building ports.
_WITHOUT_SRCCONF=

.include <bsd.own.mk>
.include "${BSDPORTMK}"
