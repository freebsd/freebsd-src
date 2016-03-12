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

.endif	# !target(__<bsd.init.mk>__)
