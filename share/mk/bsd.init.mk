# $FreeBSD$

# The include file <bsd.init.mk> includes <bsd.opts.mk>,
# ../Makefile.inc and <bsd.own.mk>; this is used at the
# top of all <bsd.*.mk> files that actually "build something".
# bsd.opts.mk is included early so Makefile.inc can use the
# MK_FOO variables.

.if !target(__<bsd.init.mk>__)
__<bsd.init.mk>__:
.include <bsd.opts.mk>
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.include <bsd.own.mk>
.MAIN: all
.endif	# !target(__<bsd.init.mk>__)
