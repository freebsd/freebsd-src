# $NetBSD: varmod-to-lower.mk,v 1.5 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the :tl variable modifier, which returns the words in the
# variable value, converted to lowercase.
#
# TODO: What about non-ASCII characters? ISO-8859-1, UTF-8?

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
