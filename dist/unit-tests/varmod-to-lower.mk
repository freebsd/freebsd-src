# $NetBSD: varmod-to-lower.mk,v 1.4 2020/10/24 08:46:08 rillig Exp $
#
# Tests for the :tl variable modifier, which returns the words in the
# variable value, converted to lowercase.

.if ${:UUPPER:tl} != "upper"
.  error
.endif

.if ${:Ulower:tl} != "lower"
.  error
.endif

.if ${:UMixeD case.:tl} != "mixed case."
.  error
.endif

all:
	@:;
