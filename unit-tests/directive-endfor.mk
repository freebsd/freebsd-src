# $NetBSD: directive-endfor.mk,v 1.1 2020/12/30 14:50:08 rillig Exp $
#
# Test for the directive .endfor, which ends a .for loop.
#
# See also:
#	directive-for.mk

# An .endfor without a corresponding .for is a parse error.
.endfor
