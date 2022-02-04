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
# LINKER_FEATURES may contain one or more of the following, based on
# linker support for that feature:
#
# - build-id:  support for generating a Build-ID note
# - retpoline: support for generating PLT with retpoline speculative
#              execution vulnerability mitigation
#
# LINKER_FREEBSD_VERSION is the linker's internal source version.
#
# These variables with an X_ prefix will also be provided if XLD is set.
#
# This file may be included multiple times, but only has effect the first time.
#

.if !target(__<bsd.linker.mk>__)
__<bsd.linker.mk>__:

_ld_vars=LD $${_empty_var_}
.if !empty(_WANT_TOOLCHAIN_CROSS_VARS)
# Only the toplevel makefile needs to compute the X_LINKER_* variables.
_ld_vars+=XLD X_
.endif

.for ld X_ in ${_ld_vars}
.if ${ld} == "LD" || !empty(XLD)
# Try to import LINKER_TYPE and LINKER_VERSION from parent make.
# The value is only used/exported for the same environment that impacts
# LD and LINKER_* settings here.
_exported_vars=	${X_}LINKER_TYPE ${X_}LINKER_VERSION ${X_}LINKER_FEATURES \
		${X_}LINKER_FREEBSD_VERSION
${X_}_ld_hash=	${${ld}}${MACHINE}${PATH}
${X_}_ld_hash:=	${${X_}_ld_hash:hash}
# Only import if none of the vars are set differently somehow else.
_can_export=	yes
.for var in ${_exported_vars}
.if defined(${var}) && (!defined(${var}__${${X_}_ld_hash}) || ${${var}__${${X_}_ld_hash}} != ${${var}})
.if defined(${var}__${${X_}_ld_hash})
.info Cannot import ${X_}LINKER variables since cached ${var} is different: ${${var}__${${X_}_ld_hash}} != ${${var}}
.endif
_can_export=	no
.endif
.endfor
.if ${_can_export} == yes
.for var in ${_exported_vars}
.if defined(${var}__${${X_}_ld_hash})
${var}=	${${var}__${${X_}_ld_hash}}
.endif
.endfor
.endif

.if ${ld} == "LD" || (${ld} == "XLD" && ${XLD} != ${LD})
.if !defined(${X_}LINKER_TYPE) || !defined(${X_}LINKER_VERSION)
_ld_version!=	(${${ld}} -v 2>&1 || echo none) | sed -n 1p
.if ${_ld_version} == "none"
.warning Unable to determine linker type from ${ld}=${${ld}}
.endif
.if ${_ld_version:[1..2]} == "GNU ld"
${X_}LINKER_TYPE=	bfd
${X_}LINKER_FREEBSD_VERSION=	0
_v=	${_ld_version:M[1-9]*.[0-9]*:[1]}
.elif ${_ld_version:MLLD}
# Strip any leading PACKAGE_VENDOR string (e.g. "Homebrew")
_ld_version:=${_ld_version:[*]:C/^.* LLD /LLD /:[@]}
${X_}LINKER_TYPE=	lld
_v=	${_ld_version:[2]}
.if ${_ld_version:[3]} == "(FreeBSD"
${X_}LINKER_FREEBSD_VERSION:=	${_ld_version:[4]:C/.*-([^-]*)\)/\1/}
.else
${X_}LINKER_FREEBSD_VERSION=	0
.endif
.elif ${_ld_version:[1]} == "@(\#)PROGRAM:ld"
# bootstrap linker on MacOS
${X_}LINKER_TYPE=        mac
_v=        ${_ld_version:[2]:S/PROJECT:ld64-//}
# Convert version 409.12 to 409.12.0 so that the echo + awk below works
.if empty(_v:M[1-9]*.[0-9]*.[0-9]*) && !empty(_v:M[1-9]*.[0-9]*)
_v:=${_v}.0
.else
# Some versions do not contain a minor version so we need to append .0.0 there
_v:=${_v}.0.0
.endif
.else
.warning Unknown linker from ${ld}=${${ld}}: ${_ld_version}, defaulting to bfd
${X_}LINKER_TYPE=	bfd
_v=	2.17.50
.endif
${X_}LINKER_VERSION!=	echo "${_v:M[1-9]*.[0-9]*}" | \
			  awk -F. '{print $$1 * 10000 + $$2 * 100 + $$3;}'
.undef _ld_version
.undef _v
${X_}LINKER_FEATURES=
.if ${${X_}LINKER_TYPE} != "bfd" || ${${X_}LINKER_VERSION} > 21750
${X_}LINKER_FEATURES+=	build-id
${X_}LINKER_FEATURES+=	ifunc
.endif
.if ${${X_}LINKER_TYPE} == "bfd" && ${${X_}LINKER_VERSION} > 21750
${X_}LINKER_FEATURES+=	riscv-relaxations
.endif
.if ${${X_}LINKER_TYPE} == "lld" && ${${X_}LINKER_VERSION} >= 60000
${X_}LINKER_FEATURES+=	retpoline
.endif
.if ${${X_}LINKER_TYPE} == "lld" && ${${X_}LINKER_VERSION} >= 90000
${X_}LINKER_FEATURES+=	ifunc-noplt
.endif
.endif
.else
# Use LD's values
X_LINKER_TYPE=		${LINKER_TYPE}
X_LINKER_VERSION=	${LINKER_VERSION}
X_LINKER_FEATURES=	${LINKER_FEATURES}
X_LINKER_FREEBSD_VERSION= ${LINKER_FREEBSD_VERSION}
.endif	# ${ld} == "LD" || (${ld} == "XLD" && ${XLD} != ${LD})

# Export the values so sub-makes don't have to look them up again, using the
# hash key computed above.
.for var in ${_exported_vars}
${var}__${${X_}_ld_hash}:=	${${var}}
.export-env ${var}__${${X_}_ld_hash}
.undef ${var}__${${X_}_ld_hash}
.endfor

.endif	# ${ld} == "LD" || !empty(XLD)
.endfor	# .for ld in LD XLD


.endif	# !target(__<bsd.linker.mk>__)
