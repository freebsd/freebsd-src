# $FreeBSD$

.if !defined(PORTSDIR)
# Autodetect if the command is being run in a ports tree that's not rooted
# in the default /usr/ports.  The ../../.. case is in case ports ever grows
# a third level.
.if exists(${.CURDIR}/Mk/bsd.port.mk)
PORTSDIR=	${.CURDIR}
.elif exists(${.CURDIR}/../Mk/bsd.port.mk)
PORTSDIR=	${.CURDIR}/..
.elif exists(${.CURDIR}/../../Mk/bsd.port.mk)
PORTSDIR=	${.CURDIR}/../..
.elif exists(${.CURDIR}/../../../Mk/bsd.port.mk)
PORTSDIR=	${.CURDIR}/../../..
.else
PORTSDIR=	/usr/ports
.endif
.endif

BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
