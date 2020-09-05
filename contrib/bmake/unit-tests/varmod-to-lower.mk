# $NetBSD: varmod-to-lower.mk,v 1.3 2020/08/28 17:21:02 rillig Exp $
#
# Tests for the :tl variable modifier, which returns the words in the
# variable value, converted to lowercase.

.if ${:UUPPER:tl} != "upper"
.error
.endif

.if ${:Ulower:tl} != "lower"
.error
.endif

.if ${:UMixeD case.:tl} != "mixed case."
.error
.endif

all:
	@:;
