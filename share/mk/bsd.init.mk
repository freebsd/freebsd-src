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
beforebuild: .PHONY .NOTMAIN
.if !defined(_SKIP_BUILD)
all: beforebuild .WAIT
.endif

.if ${.MAKE.LEVEL:U1} == 0 && ${BUILD_AT_LEVEL0:Uyes:tl} == "no" && !make(clean*)
# this tells lib.mk and prog.mk to not actually build anything
_SKIP_BUILD = not building at level 0
.endif
.if ${.MAKE.LEVEL} > 0 && !empty(_SKIP_BUILD)
.warning ${_SKIP_BUILD}
.endif

.if ${MK_META_MODE} == "yes"
.if !exists(/dev/filemon) && \
    ${UPDATE_DEPENDFILE:Uyes:tl} != "no" && !defined(NO_FILEMON) && \
    !make(showconfig)
.warning The filemon module (/dev/filemon) is not loaded.
.warning META_MODE is less useful for incremental builds without filemon.
.warning 'kldload filemon' or pass -DNO_FILEMON to suppress this warning.
.endif
.endif	# ${MK_META_MODE} == "yes"

.endif	# !target(__<bsd.init.mk>__)
