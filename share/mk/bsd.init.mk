# $FreeBSD: src/share/mk/bsd.init.mk,v 1.5.24.1 2010/02/10 00:26:20 kensmith Exp $

# The include file <bsd.init.mk> includes ../Makefile.inc and
# <bsd.own.mk>; this is used at the top of all <bsd.*.mk> files
# that actually "build something".

.if !target(__<bsd.init.mk>__)
__<bsd.init.mk>__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.include <bsd.compat.mk>
.include <bsd.own.mk>
.MAIN: all
.endif	# !target(__<bsd.init.mk>__)
