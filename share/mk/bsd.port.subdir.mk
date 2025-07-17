
.if !defined(PORTSDIR)
# Autodetect if the command is being run in a ports tree that's not rooted
# in the default /usr/ports.  The ../../.. case is in case ports ever grows
# a third level.
.for RELPATH in . .. ../.. ../../..
.if !defined(_PORTSDIR) && exists(${.CURDIR}/${RELPATH}/Mk/bsd.port.mk)
_PORTSDIR=	${.CURDIR}/${RELPATH}
.endif
.endfor
_PORTSDIR?=	/usr/ports
PORTSDIR=	${_PORTSDIR:tA}
.endif

BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
