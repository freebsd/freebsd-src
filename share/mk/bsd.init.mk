# $FreeBSD$

# The include file <bsd.init.mk> includes <bsd.opts.mk>,
# ../Makefile.inc and <bsd.own.mk>; this is used at the
# top of all <bsd.*.mk> files that actually "build something".
# bsd.opts.mk is included early so Makefile.inc can use the
# MK_FOO variables.

.if !target(__<bsd.init.mk>__)
__<bsd.init.mk>__:
.include <bsd.opts.mk>
.-include "local.init.mk"
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.include <bsd.own.mk>
.MAIN: all

# Some targets need to know when something may build.  This is used to
# optimize targets that are only needed when building something, such as
# (not) reading in depend files.  For DIRDEPS_BUILD, it will only calculate
# the dependency graph at .MAKE.LEVEL==0, so nothing should be built there.
# Skip "build" logic if:
# - DIRDEPS_BUILD at MAKELEVEL 0
# - make -V is used without an override
# - make install is used without other targets.  This is to avoid breaking
#   things like 'make all install' or 'make foo install'.
# - non-build targets are called
.if ${MK_DIRDEPS_BUILD} == "yes" && ${.MAKE.LEVEL:U1} == 0 && \
    ${BUILD_AT_LEVEL0:Uyes:tl} == "no" && !make(clean*)
_SKIP_BUILD=	not building at level 0
.elif !empty(.MAKEFLAGS:M-V${_V_DO_BUILD}) || \
    ${.TARGETS:M*install*} == ${.TARGETS} || \
    make(clean*) || make(obj) || make(analyze) || make(print-dir) || \
    make(destroy*)
# Skip building, but don't show a warning.
_SKIP_BUILD=
.endif
.if ${MK_DIRDEPS_BUILD} == "yes" && ${.MAKE.LEVEL} > 0 && !empty(_SKIP_BUILD)
.warning ${_SKIP_BUILD}
.endif

beforebuild: .PHONY .NOTMAIN
.if !defined(_SKIP_BUILD)
all: beforebuild .WAIT
.endif

.if ${MK_META_MODE} == "yes"
.if !exists(/dev/filemon) && \
    ${UPDATE_DEPENDFILE:Uyes:tl} != "no" && !defined(NO_FILEMON) && \
    !make(showconfig) && !make(print-dir) && ${.MAKEFLAGS:M-V} == ""
.warning The filemon module (/dev/filemon) is not loaded.
.warning META_MODE is less useful for incremental builds without filemon.
.warning 'kldload filemon' or pass -DNO_FILEMON to suppress this warning.
.endif
.endif	# ${MK_META_MODE} == "yes"

.endif	# !target(__<bsd.init.mk>__)
