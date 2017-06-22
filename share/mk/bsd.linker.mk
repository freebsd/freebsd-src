# $FreeBSD$

# Setup variables for the linker.
#
# LINKER_TYPE is the major type of linker. Currently binutils and lld support
# automatic detection.
#
# LINKER_VERSION is a numeric constant equal to:
#     major * 10000 + minor * 100 + tiny
# It too can be overridden on the command line.
#
# This file may be included multiple times, but only has effect the first time.
#

.if !target(__<bsd.linker.mk>__)
__<bsd.linker.mk>__:

_ld_version!=	${LD} --version 2>/dev/null | head -n 1 || echo none
.if ${_ld_version} == "none"
.error Unable to determine linker type from LD=${LD}
.endif
.if ${_ld_version:[1..2]} == "GNU ld"
LINKER_TYPE=	binutils
_v=	${_ld_version:[3]}
.elif ${_ld_version:[1]} == "LLD"
LINKER_TYPE=	lld
_v=	${_ld_version:[2]}
.else
.error Unknown linker from LD=${LD}: ${_ld_version}
.endif
LINKER_VERSION!=echo "${_v:M[1-9].[0-9]*}" | awk -F. '{print $$1 * 10000 + $$2 * 100 + $$3;}'

.endif	# !target(__<bsd.linker.mk>__)
