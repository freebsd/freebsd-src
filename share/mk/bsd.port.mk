# $FreeBSD$

PORTSDIR?=	/usr/ports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

# Needed to keep bsd.own.mk from reading in /etc/src.conf
# and setting MK_* variables when building ports.
_WITHOUT_SRCCONF=

# Enable CTF conversion on request.
.if defined(WITH_CTF)
.undef NO_CTF
.endif

.include <bsd.own.mk>
.include "${BSDPORTMK}"
