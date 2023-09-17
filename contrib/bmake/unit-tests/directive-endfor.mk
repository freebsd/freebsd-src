# $NetBSD: directive-endfor.mk,v 1.2 2023/06/01 20:56:35 rillig Exp $
#
# Test for the directive .endfor, which ends a .for loop.
#
# See also:
#	directive-for.mk

# An .endfor without a corresponding .for is a parse error.
# expect+1: for-less endfor
.endfor
